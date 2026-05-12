/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QApplication>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <QSplitter>
#include <QLineEdit>
#include <QPushButton>
#include <QToolBar>
#include <QTextStream>
#include <QDebug>
#include <QStyleFactory>
#include <QProcessEnvironment>
#include <QDirIterator>

#include "mainwindow.h"
#include "circuit.h"
#include "componentlist.h"
#include "editorwindow.h"
#include "circuitwidget.h"
#include "filewidget.h"
#include "installer.h"
#include "utils.h"

MainWindow* MainWindow::m_pSelf = nullptr;

MainWindow::MainWindow()
          : QMainWindow()
{
    setWindowIcon( QIcon(":/simulide.png") );
    m_pSelf   = this;
    m_circuitW = nullptr;
    m_autoBck = 15;
    m_state = "■";
    m_revision = QString( REVNO ).remove("R").toInt();
    m_version = "SimulIDE_"+QString( APP_VERSION )+"-"+QString( REVNO );

    this->setWindowTitle( m_version );

#ifdef __EMSCRIPTEN__
    // WASM has no host filesystem we can write to outside /tmp.
    m_configDir.setPath( QStandardPaths::writableLocation( QStandardPaths::TempLocation ) );
#else
    m_configDir.setPath( QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) );
#endif

    m_settings     = new QSettings( getConfigPath("simulide.ini"), QSettings::IniFormat, this );
    m_compSettings = new QSettings( getConfigPath("compList.ini"), QSettings::IniFormat, this );

    if( m_settings->contains( "120userPath" ) )
        m_userDir = m_settings->value("120userPath").toString();
    else m_userDir = m_settings->value("userPath").toString();

    if( m_userDir.isEmpty() || !QDir( m_userDir ).exists() )
        m_userDir = QDir::homePath();

    // Fonts --------------------------------------
    QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf" );
    QFontDatabase::addApplicationFont(":/Ubuntu-B.ttf" );
    QFontDatabase::addApplicationFont(":/UbuntuMono-B.ttf" );
    QFontDatabase::addApplicationFont(":/UbuntuMono-BI.ttf" );
    QFontDatabase::addApplicationFont(":/UbuntuMono-R.ttf" );
    QFontDatabase::addApplicationFont(":/UbuntuMono-RI.ttf" );
    QFontDatabase::addApplicationFont(":/Roboto-Regular.ttf" );
    QFontDatabase::addApplicationFont(":/Roboto-Bold.ttf" );
    QFontDatabase::addApplicationFont(":/Roboto-Italic.ttf" );
    QFontDatabase::addApplicationFont(":/Roboto-BoldItalic.ttf" );

    float scale = 1.0;
    if( m_settings->contains( "fontScale" ) )
    {
        scale = m_settings->value( "fontScale" ).toFloat();
    }else{
        QScreen* screen = QGuiApplication::primaryScreen();
        float dpiX = screen->logicalDotsPerInchX();
        scale = dpiX/96.0;
    }
    setFontScale( scale );

    QString fontName = "Roboto";
    if( m_settings->contains("fontName") ) fontName = m_settings->value("fontName").toString();
    setDefaultFontName( fontName );

    QFont df = qApp->font();
    df.setFamily( fontName );
    qApp->setFont( df );
    setFont( df );
    //----------------------------------------------

    QApplication::setStyle( QStyleFactory::create("Fusion") ); //applyStyle();

    // Load centralized UI theme stylesheet. Default is the bright modern light theme.
    // Switch by setting "theme" in simulide.ini (e.g. theme=dark) once more themes ship.
    QString themeName = m_settings->value( "theme", "light" ).toString();
    QFile themeFile( ":/themes/" + themeName + ".qss" );
    if( themeFile.open( QFile::ReadOnly | QFile::Text ) ){
        qApp->setStyleSheet( QString::fromUtf8( themeFile.readAll() ) );
        themeFile.close();
    }

    // qApp->setStyleSheet(
    //     "QMainWindow, EditorWidget, CircuitWidget, QSplitter { background: white; }"
    //     "QToolBar { background: white; border: 0; }"
    //     "QSplitter::handle { background: white; }"
    //     "QToolButton { background: transparent; }"
    // );

    createWidgets();
    m_circuitW->newCircuit();
    readSettings();

#ifndef HIDE_SOME_ACTIONS
    if( m_autoUpdt ){
        QTimer::singleShot( 5000, CircuitWidget::self()
                           , [=]()->void{ m_installer->checkForUpdates(); } );
    }
#endif

    QString backPath = getConfigPath( "backup.sim2" );
    if( QFile::exists( backPath ) )
    {
        QMessageBox msgBox;
        msgBox.setText( tr("Looks like SimulIDE crashed...")+"\n\n"
                       +tr("There is an auto-saved copy of the Circuit\n")
                       +tr("You must save it with any other name if you want to keep it")+"\n\n"
                       +tr("This file will be auto-deleted!!")+"\n");
        msgBox.setInformativeText(tr("Do you want to open the auto-saved copy of the Circuit?"));
        msgBox.setStandardButtons( QMessageBox::Open | QMessageBox::Discard );
        msgBox.setDefaultButton( QMessageBox::Open );

        if( msgBox.exec() == QMessageBox::Open ) CircuitWidget::self()->loadCirc( backPath );
        else                                     QFile::remove( backPath ); // Remove backup file
    }
}
MainWindow::~MainWindow(){ }

void MainWindow::hideGui()
{
    m_sidepanel->hide();
    m_editor->hide();
    m_circuitW->hideGui();
}

void MainWindow::keyPressEvent( QKeyEvent* event)
{
    if( event->key() == Qt::Key_F5 )
        CircuitWidget::self()->powerCircOn();
    else
        QMainWindow::keyPressEvent( event);
}

void MainWindow::closeEvent( QCloseEvent *event )
{
    if( CircuitWidget::self()->isHiddenGui() ) return;

    if( !m_editor->close() )       { event->ignore(); return; }
    if( !m_circuitW->newCircuit()) { event->ignore(); return; }

    writeSettings();
    event->accept();
}

bool MainWindow::eventFilter( QObject* obj, QEvent* event )
{
    if( obj == m_searchComponent ){
        const QEvent::Type t = event->type();
        if( t == QEvent::FocusIn || t == QEvent::MouseButtonPress )
            searchChanged();
    }
    return QMainWindow::eventFilter( obj, event );
}

void MainWindow::readSettings()
{
    restoreGeometry( m_settings->value("geometry" ).toByteArray());
    restoreState(    m_settings->value("windowState" ).toByteArray());
    m_mainSplitter->restoreState( m_settings->value("Centralsplitter/geometry").toByteArray());
    CircuitWidget::self()->splitter()->restoreState( m_settings->value("Circsplitter/geometry").toByteArray());

    m_autoBck = 15;
    if( m_settings->contains("autoBck") ) m_autoBck = m_settings->value("autoBck").toInt();
    Circuit::self()->setAutoBck( m_autoBck );

    m_autoUpdt = 1;
    if( m_settings->contains("autoUpdt") ) m_autoUpdt = m_settings->value("autoUpdt").toInt();
}

void MainWindow::writeSettings()
{
    m_settings->setValue("autoUpdt",  m_autoUpdt );
    m_settings->setValue("autoBck",   m_autoBck );
    m_settings->setValue("fontName",  m_fontName );
    m_settings->setValue("fontScale", m_fontScale );
    m_settings->setValue("geometry",  saveGeometry() );
    m_settings->setValue("windowState", saveState() );
    m_settings->setValue("Centralsplitter/geometry", m_mainSplitter->saveState() );
    m_settings->setValue("Circsplitter/geometry", CircuitWidget::self()->splitter()->saveState() );

    m_installer->writeSettings();
    ComponentList::self()->writeSettings();
    FileWidget::self()->writeSettings();
}

QString MainWindow::loc()
{
    if( m_lang == Chinese )    return "zh_CN";
    if( m_lang == Czech )      return "cz";
    //if( m_lang == Dutch )      return "nl";
    //if( m_lang == French )     return "fr";
    if( m_lang == German )     return "de";
    //if( m_lang == Italian )    return "it";
    //if( m_lang == Russian )    return "ru";
    if( m_lang == Spanish )    return "es";
    //if( m_lang == Portuguese ) return "pt_PT";
    if( m_lang == Pt_Brasil )  return "pt_BR";
    //if( m_lang == Slovak )     return "sk";
    //if( m_lang == Turkish )    return "tr";
    if( m_lang == Traditional_Chinese ) return "zh_TW";

    return "en";
}

void MainWindow::setLoc( QString loc )
{
    Langs lang = English;
    if     ( loc == "zh_CN" ) lang = Chinese;
    else if( loc == "zh_TW" ) lang = Traditional_Chinese;
    else if( loc == "cz" )    lang = Czech;
    //else if( loc == "nl" )    lang = Dutch;
    //else if( loc == "fr" )    lang = French;
    else if( loc == "de" )    lang = German;
    //else if( loc == "it" )    lang = Italian;
    //else if( loc == "ru" )    lang = Russian;
    else if( loc == "es" )    lang = Spanish;
    //else if( loc == "pt_PT" ) lang = Portuguese;
    else if( loc == "pt_BR" ) lang = Pt_Brasil;
    //else if( loc == "sk" )    lang = Slovak;
    //else if( loc == "tr" )    lang = Turkish;

    m_lang = lang;
}

void MainWindow::setLang( Langs lang ) // From appDialog
{
    m_lang = lang;

    QString langF = ":/simulide_"+loc()+".qm";
    if( !QFileInfo::exists( langF) ) m_lang = English;
    settings()->setValue( "language", loc() );
}

void MainWindow::setDefaultFontName( const QString& fontName )
{
    m_fontName = fontName;
}

void MainWindow::setFile( QString file )
{
    m_file = file;
    setWindowTitle( m_state+" "+m_version+" - "+file );
}

void MainWindow::setState( QString state )
{
    m_state = state;
    QString changed = windowTitle().endsWith("*") ? "*" : "";
    setWindowTitle( state+" "+m_version+" - "+m_file+changed );
}

void MainWindow::createWidgets()
{
    QWidget *centralWidget = new QWidget( this );
    setCentralWidget(centralWidget);

    QGridLayout *baseWidgetLayout = new QGridLayout( centralWidget );
    baseWidgetLayout->setSpacing( 0 );
    baseWidgetLayout->setContentsMargins(0, 0, 0, 0);

    m_mainSplitter = new QSplitter( this );
    m_mainSplitter->setOrientation( Qt::Horizontal );

    // m_sidepanel = new QTabWidget( this );
    // m_sidepanel->setTabPosition( QTabWidget::North );
    // m_sidepanel->setIconSize( QSize( 20*m_fontScale, 20*m_fontScale ) );
    // //QString fontSize = QString::number( int(11*m_fontScale) );
    // //m_sidepanel->tabBar()->setStyleSheet("QTabBar { font-size:"+fontSize+"px; }");
    // m_mainSplitter->addWidget( m_sidepanel );

    m_listWidget = new QWidget(centralWidget);
    QVBoxLayout* listLayout = new QVBoxLayout();
    // listLayout->setSpacing( 6 );
    // listLayout->setContentsMargins(0, 2, 0, 0);

    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setSpacing(1);

    m_searchComponent = new QLineEdit( this );
    // Click-only focus so Qt doesn't auto-focus this widget on window show.
    // Initial auto-focus would fire FocusIn → searchChanged() → show the
    // list before layout is finalized, so it would appear at top-left.
    m_searchComponent->setFocusPolicy( Qt::ClickFocus );
    QFont font = m_searchComponent->font();
    font.setPixelSize( 14*m_fontScale );
    m_searchComponent->setFont( font );
    m_searchComponent->setFixedHeight( 26*m_fontScale );
    m_searchComponent->setPlaceholderText( " "+tr("Search Components"));
    m_searchComponent->setMinimumWidth(250);
    m_searchComponent->setMaximumWidth(350);
    m_searchComponent->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    connect(m_searchComponent, &QLineEdit::textChanged,
        this, &MainWindow::searchChanged);

    // connect( m_searchComponent, SIGNAL( editingFinished() ),
    //          this,              SLOT(   searchChanged() ) );

    // Show the list when the search bar is focused or clicked. QLineEdit has
    // no focus signal, and a click when the bar already has focus won't fire
    // FocusIn — so we cover both via an event filter. (We deliberately don't
    // hook editingFinished: it fires on focus loss, which would re-show the
    // list immediately after the outside-click hide and cause a flicker loop.)
    m_searchComponent->installEventFilter( this );

    // m_clearButton = new QPushButton( this );
    // m_clearButton->setFixedSize( 26*m_fontScale, 26*m_fontScale );
    // m_clearButton->setIcon( QIcon(":/reload.svg") );
    // m_clearButton->setToolTip( tr("Clear and reload component list"));

    // connect( m_clearButton, SIGNAL( clicked() ),
    //          this,          SLOT(   clearSearch()) );

    // QHBoxLayout *searchBarLayout = new QHBoxLayout();
    // Inner layout: search bar + clear button side by side. Wrapped in a
    // QWidget so it can be inserted into the CircuitWidget's QToolBar
    // (QToolBar accepts widgets, not bare layouts).
    m_searchBarContainer = new QWidget( this );
    QHBoxLayout *searchBarLayout = new QHBoxLayout( m_searchBarContainer );
    searchBarLayout->setSpacing(1);
    searchBarLayout->setContentsMargins(0, 0, 0, 0);
    searchBarLayout->addWidget(m_searchComponent);
    // searchBarLayout->addWidget(m_clearButton);

    // Old placement above the central area (kept for reference). The
    // search bar now lives at the start of the CircuitWidget toolbar; see
    // the m_circuitW->circToolBar()->insertWidget(...) call below.
    // searchLayout->addStretch( 1 );
    // searchLayout->addLayout( searchBarLayout );
    // searchLayout->addStretch( 3 );
    // listLayout->addLayout( searchLayout );
    Q_UNUSED( searchLayout );

#ifndef HIDE_SOME_ACTIONS
    m_installer = new Installer( this );
    m_fileTree  = new FileWidget( this );
#else
    // Construct without a parent so they don't appear as floating
    // top-level windows. The singletons (Installer::self() and
    // FileWidget::self()) stay valid for the few callers that reach in
    // (e.g. filebrowser.cpp uses FileWidget::self()->setPath()).
    m_installer = new Installer( nullptr );
    m_fileTree  = new FileWidget( nullptr );
#endif
    m_circuitW  = new CircuitWidget( this );

    // Inject the search bar at the start of the CircuitWidget's toolbar
    // so it sits as the first item there. insertWidget(before, w) places
    // it before the given action; passing the toolbar's first action
    // (or nullptr if empty) makes it the leftmost item.
    {
        QToolBar* tb = m_circuitW->circToolBar();
        QAction* firstAct = tb->actions().isEmpty() ? nullptr : tb->actions().first();
        tb->insertWidget( firstAct, m_searchBarContainer );
        tb->insertSeparator( firstAct );
    }

    // Top-level popup approach (kept for reference). In WASM, repeated
    // show/hide cycles on a Qt::Tool top-level desync the canvas/surface
    // from Qt's isVisible state, so show() silently no-ops after a few
    // cycles. Switching to a child widget avoids that.
    // m_components = new ComponentList(nullptr);
    // m_components->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    // m_components->setAttribute(Qt::WA_ShowWithoutActivating);
    // listLayout->addWidget( m_components );

    m_components = new ComponentList( this );
    m_components->hide();

    m_listWidget->setLayout(listLayout);

    // m_sidepanel->addTab( m_listWidget, QIcon(":/ic2.png")    , "" );
#ifndef HIDE_SOME_ACTIONS
    m_sidepanel->addTab( m_installer , QIcon(":/complib.svg"), "" );
    m_sidepanel->addTab( m_fileTree  , QIcon(":/files.svg")  , "" );
#endif

    // m_sidepanel->setTabToolTip( 0, tr("Components") );
#ifndef HIDE_SOME_ACTIONS
    m_sidepanel->setTabToolTip( 1, tr("Libraries") );
    m_sidepanel->setTabToolTip( 2, tr("Files") );
#endif

    m_editor = new EditorWindow( this );

    m_mainSplitter->addWidget( m_editor );
    m_mainSplitter->addWidget( m_circuitW );

    // Add listWidget to the base layout above the splitter
    // baseWidgetLayout->addWidget( m_listWidget, 0, 0, Qt::AlignTop );
    baseWidgetLayout->addWidget( m_mainSplitter, 1, 0 );

    m_mainSplitter->setSizes( {400, 600} );

    m_editor->hide();   // hidden until MCU is placed

    this->showMaximized();
}

void MainWindow::showEditor() { if( m_editor && !m_editor->isVisible() ) m_editor->show(); }
void MainWindow::hideEditor() { if( m_editor &&  m_editor->isVisible() ) m_editor->hide(); }

void MainWindow::clearSearch()
{
    m_searchComponent->clear();
    searchChanged();
}

void MainWindow::searchChanged()
{
    // Top-level popup positioning (kept for reference) — see createWidgets.
    // QPoint pos = m_searchComponent->mapToGlobal(QPoint(0, m_searchComponent->height()));
    // m_components->setGeometry(QRect(pos, QSize(m_searchComponent->width(), 700)));

    // Position relative to MainWindow (the new parent). Dynamic height:
    // from just below the search bar down to a small margin above the
    // MainWindow's bottom so the list grows with the window.
    const int bottomMargin = 20*m_fontScale;
    QPoint topLeft = m_searchComponent->mapTo( this,
                                               QPoint(0, m_searchComponent->height()) );
    int height = this->height() - topLeft.y() - bottomMargin;
    if( height < 100 ) height = 100;
    m_components->setGeometry( QRect( topLeft,
                                      QSize( m_searchComponent->width(), height ) ) );
    m_components->raise();

    QString filter = m_searchComponent->text();
    m_components->search( filter );

    m_components->show();
}

QString MainWindow::getHelp( QString name, bool save )
{
    if( save && m_help.contains( name ) ) return m_help.value( name );

    QString help = tr("No help available");
    QString locale = loc();
    QString localeFolder = "";

    if( loc() != "en" ) {
        locale.prepend("_");
        localeFolder = locale + "/";
    }
    else locale = "";

    name = name.toLower().replace( " ", "" );

    QString                dfPath = getFilePath( name+locale, m_userDir+"help" );
    if( dfPath.isEmpty() ) dfPath = getFilePath( name+locale, m_configDir.absoluteFilePath("help") );
    if( dfPath.isEmpty() ) dfPath = getFilePath( name+locale, ":/help" );
    if( dfPath.isEmpty() ) dfPath = getFilePath( name, m_userDir+"help" );
    if( dfPath.isEmpty() ) dfPath = getFilePath( name, m_configDir.absoluteFilePath("help") );
    if( dfPath.isEmpty() ) dfPath = getFilePath( name, ":/help" );
    if( dfPath.isEmpty() ) return help;

    if( QFileInfo::exists( dfPath ) )
    {
        help.clear();
        QStringList lines = fileToStringList( dfPath, "MainWindow::getHelp" );
        for( QString line : lines )
        {
            if( line.startsWith("#include") )
            {
                QString file = line.remove("#include ");
                line = getHelp( file );
                help.append( line );
            }
            else help.append( line+"\n" );
        }
    }
    if( save ) m_help[name] = help;
    return help;
}

QString MainWindow::getFilePath( QString filename, QString directory )
{
    QDirIterator it( directory, QDirIterator::Subdirectories );
    while( it.hasNext() ) {
        it.next();
        QFileInfo fileInfo( it.filePath() );
        if( fileInfo.isFile() && fileInfo.baseName() == filename )
            return it.filePath();
    }
    return "";
}

void MainWindow::getUserPath()
{
    QString path = getDirDialog( tr("Select User data directory"), m_userDir );

    setUserPath( path );
}

void MainWindow::setUserPath( QString path )
{
    if( !QFileInfo::exists( path ) ) return;
    m_settings->setValue("120userPath", path);
    m_userDir = path;
}

QString MainWindow::getConfigPath( QString file )
{
    return m_configDir.absoluteFilePath( file );
}

QString MainWindow::getUserFilePath( QString file )
{
    if( m_userDir.isEmpty() ) return "";
    return QDir( m_userDir ).absoluteFilePath( file );
}

QString MainWindow::getDataFilePath( QString file )
{
    QString path = getUserFilePath( file );           // File in user data folder

    if( path.isEmpty() || !QFileInfo::exists( path ) )
        path = getConfigPath("data/"+file );          // File in Config data folder

    if( path.isEmpty() || !QFileInfo::exists( path ) )
        path = ":/"+file;                             // File in SimulIDE resources

    if( path.isEmpty() || !QFileInfo::exists( path ) ) return "";

    return path;
}

QString MainWindow::getCircFilePath( QString file )
{
    if( !Circuit::self() ) return "";

    QString circPath = Circuit::self()->getFilePath();
    if( circPath.isEmpty() ) return "";

    QDir circuitDir = QFileInfo( circPath ).absoluteDir();
    QString path = circuitDir.absoluteFilePath( file );    // Search in Circuit folder
    if( !QFileInfo::exists( path ) )
        path = circuitDir.absoluteFilePath("data/"+file ); // Search in Circuit/data folder

    if( !QFileInfo::exists( path ) ) return "";

    return path;
}

QSettings* MainWindow::settings() { return m_settings; }

QSettings* MainWindow::compSettings() { return m_compSettings; }

#include "moc_mainwindow.cpp"
