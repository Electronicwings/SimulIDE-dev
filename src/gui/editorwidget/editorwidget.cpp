/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QDebug>
#include <QWidget>
#include <QTabWidget>
#include <QTabBar>
#include <QToolBar>
#include <QFileDialog>
#include <QFileInfo>
#include <QFile>
#include <QMessageBox>
#include <QMimeData>
#include <QGridLayout>
#include <QFormLayout>
#include <QSplitter>
#include <QToolButton>
#include <QSettings>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

#include "editorwidget.h"
#include "findreplace.h"
#include "propdialog.h"
#include "scrollbar.h"
#include "basedebugger.h"
#include "mainwindow.h"
#include "simulator.h"
#include "utils.h"

#include "intprop.h"
#include "boolprop.h"

EditorWidget::EditorWidget( QWidget* parent )
            : QWidget( parent )
            , CompBase( "Editor", "" )
            , m_outPane( this )
            , m_settingsMenu( this )
            , m_fileMenu( this )
{
    setAcceptDrops( true );

    createActions();
    createToolBars();
    createWidgets();
    updateRecentFileActions();
    readSettings();

    addPropGroup( { tr("Editor Settings"), {
        new IntProp <EditorWidget>("FontSize", tr("Font Size"),"_px"
                                  , this,&EditorWidget::fontSize, &EditorWidget::setFontSize, 0 ),

        new IntProp <EditorWidget>("TabSize", tr("Tab Size"),""
                                  , this, &EditorWidget::tabSize, &EditorWidget::setTabSize, 0 ),

        new BoolProp<EditorWidget>("SpaceTabs", tr("Tabs as spaces"),""
                                  , this, &EditorWidget::spaceTabs, &EditorWidget::setSpaceTabs ),

        new BoolProp<EditorWidget>("ShowSpaces", tr("Show Spaces and Tabs"),""
                                  , this, &EditorWidget::showSpaces, &EditorWidget::setShowSpaces ),

        new ComProperty("", tr("Auto close pairs:"),"","",0),

        new BoolProp<EditorWidget>("CloseParenthesis", tr("Auto close Parenthesis ( )"),""
                                  , this, &EditorWidget::closeParenthesis, &EditorWidget::setCloseParenthesis ),

        new BoolProp<EditorWidget>("CloseBraces", tr("Auto close Braces { }"),""
                                  , this, &EditorWidget::closeBraces, &EditorWidget::setCloseBraces ),

        new BoolProp<EditorWidget>("CloseBrackets", tr("Auto close Brackets [ ]"),""
                                  , this, &EditorWidget::closeBrackets, &EditorWidget::setCloseBrackets ),

        new BoolProp<EditorWidget>("CloseQuotes", tr("Auto close Quotes \" \""),""
                                  , this, &EditorWidget::closeQuotes, &EditorWidget::setCloseQuotes ),

        new BoolProp<EditorWidget>("CloseSquotes", tr("Auto close Single Quotes ' '"),""
                                  , this, &EditorWidget::closeSquotes, &EditorWidget::setCloseSquotes ),
    }, 0} );
}
EditorWidget::~EditorWidget(){}

void EditorWidget::setFontSize( int size )
{
    m_fontSize = size;
    m_font.setPixelSize( size );
    MainWindow::self()->settings()->setValue( "Editor_font_size", QString::number(m_fontSize) );

    for( CodeEditor* ce : getCodeEditors() ) ce->setFont( m_font );

    setTabSize( m_tabSize );
}

void EditorWidget::setTabSize( int size )
{
    m_tabSize = size;
    MainWindow::self()->settings()->setValue( "Editor_tab_size", QString::number(m_tabSize) );

    for( CodeEditor* ce : getCodeEditors() ) ce->setTabStopDistance( calcTabstopWidth() );
}

void EditorWidget::setShowSpaces( bool show )
{
    m_showSpaces = show;

    for( CodeEditor* ce : getCodeEditors() ) docShowSpaces( ce );

    QString showStr = m_showSpaces ? "true" : "false";
    MainWindow::self()->settings()->setValue( "Editor_show_spaces", showStr );
}

void EditorWidget::docShowSpaces( CodeEditor* ce )
{
    QTextOption option = ce->document()->defaultTextOption();

    if( m_showSpaces ) option.setFlags( option.flags() | QTextOption::ShowTabsAndSpaces  );
    else               option.setFlags( option.flags() & ~QTextOption::ShowTabsAndSpaces );

    ce->document()->setDefaultTextOption( option );
}

void EditorWidget::setSpaceTabs( bool on )
{
    m_spaceTabs = on;

    if( on ) { m_tab = ""; for( int i=0; i<m_tabSize; i++) m_tab += " "; }
    else m_tab = "\t";

    if( m_spaceTabs )
         MainWindow::self()->settings()->setValue( "Editor_spaces_tabs", "true" );
    else MainWindow::self()->settings()->setValue( "Editor_spaces_tabs", "false" );
}

void EditorWidget::setCloseParenthesis( bool c )
{
    if( c ) m_autoClose |= 1<<0;
    else    m_autoClose &= ~(1<<0);

    for( CodeEditor* ce : getCodeEditors() ) ce->setAutoClose( m_autoClose );
}

void EditorWidget::setCloseBraces( bool c )
{
    if( c ) m_autoClose |= 1<<1;
    else    m_autoClose &= ~(1<<1);

    for( CodeEditor* ce : getCodeEditors() ) ce->setAutoClose( m_autoClose );
}

void EditorWidget::setCloseBrackets( bool c )
{
    if( c ) m_autoClose |= 1<<2;
    else    m_autoClose &= ~(1<<2);

    for( CodeEditor* ce : getCodeEditors() ) ce->setAutoClose( m_autoClose );
}

void EditorWidget::setCloseQuotes( bool c )
{
    if( c ) m_autoClose |= 1<<3;
    else    m_autoClose &= ~(1<<3);

    for( CodeEditor* ce : getCodeEditors() ) ce->setAutoClose( m_autoClose );
}

void EditorWidget::setCloseSquotes( bool c )
{
    if( c ) m_autoClose |= 1<<4;
    else    m_autoClose &= ~(1<<4);

    for( CodeEditor* ce : getCodeEditors() ) ce->setAutoClose( m_autoClose );
}

CodeEditor* EditorWidget::getCodeEditor()
{
    return (CodeEditor*)m_docWidget->currentWidget();
}

QList<CodeEditor*> EditorWidget::getCodeEditors()
{
    QList<CodeEditor*> list;
    for( QWidget* widget : m_fileList.values() ) list.append( (CodeEditor*)widget );
    return list;
}

bool EditorWidget::close()
{
    int count = m_docWidget->count();
    for( int i=0; i<count; i++ )
    {
        m_docWidget->setCurrentIndex( i );
        if( !maybeSave() ) return false;
    }
    while( m_docWidget->count() )
    {
        m_docWidget->setCurrentIndex( 0 );
        closeTab( 0 );
    }
    writeSettings();
    return true;
}

void EditorWidget::newFile()
{
    QString sourcepath = MainWindow::self()->getConfigPath("") + "/";
    // Ask the user for a filename + extension up front so the new tab is
    // named meaningfully and so save() knows what extension to use later.
    QDialog dlg( this );
    dlg.setWindowTitle( tr("New File") );

    QLineEdit* nameEdit = new QLineEdit( &dlg );
    nameEdit->setPlaceholderText( tr("filename (no extension)") );
    // Reject characters that would make a bad filename on any platform.
    QRegularExpression rx( "[A-Za-z0-9_\\-]+" );
    nameEdit->setValidator( new QRegularExpressionValidator( rx, nameEdit ) );

    QComboBox* extCombo = new QComboBox( &dlg );
    extCombo->addItems( {".h", ".c", ".cpp", ".ino"} );

    QFormLayout* form = new QFormLayout;
    form->addRow( new QLabel(tr("Name:"), &dlg),      nameEdit );
    form->addRow( new QLabel(tr("Extension:"), &dlg), extCombo );

    QLabel* statusLabel = new QLabel( &dlg );
    statusLabel->setStyleSheet("color: #B91C1C;");
    statusLabel->setWordWrap( true );

    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg );
    QPushButton* okBtn = buttons->button( QDialogButtonBox::Ok );
    okBtn->setEnabled( false );
    connect( buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept );
    connect( buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject );

    QVBoxLayout* root = new QVBoxLayout( &dlg );
    root->addLayout( form );
    root->addWidget( statusLabel );
    root->addWidget( buttons );

    // Live validation: empty → disable, exists in sourcepath → flag + disable.
    auto revalidate = [=]() {
        QString name = nameEdit->text().trimmed();
        if( name.isEmpty() ){
            statusLabel->clear();
            okBtn->setEnabled( false );
            return;
        }
        QString full = sourcepath + name + extCombo->currentText();
        if( QFileInfo::exists( full ) ){
            statusLabel->setText( tr("A file with this name already exists. Choose a different name.") );
            okBtn->setEnabled( false );
        }else{
            statusLabel->clear();
            okBtn->setEnabled( true );
        }
    };
    connect( nameEdit, &QLineEdit::textChanged,           &dlg, revalidate );
    connect( extCombo, &QComboBox::currentTextChanged,    &dlg, revalidate );

    nameEdit->setFocus();

    if( dlg.exec() != QDialog::Accepted ) return;

    QString name = nameEdit->text().trimmed();
    if( name.isEmpty() ) return;

    QString filepath = sourcepath + name + extCombo->currentText();

    QFileInfo fi( filepath );
    if( !fi.exists() ){
        QFile f( filepath );
        if( f.open( QIODevice::WriteOnly | QIODevice::Text ) ){
            f.write( "" );
            f.close();
        }
    }

    addDocument( filepath, true );
}

void EditorWidget::addDocument(  QString file, bool main  )
{
    CodeEditor* ce = new CodeEditor( this, &m_outPane );
    ce->setVerticalScrollBar( new scrollWidget( ce, Qt::Vertical ) );
    ce->setTabStopDistance( calcTabstopWidth() );
    ce->setAutoClose( m_autoClose );
    docShowSpaces( ce );

    QString tabString = file.isEmpty() ? tr("NEW") : getFileName(file);
    m_docWidget->addTab( ce, tabString );

    if( !file.isEmpty() ){
        // Only read content if the file actually exists on disk. newFile()
        // passes a chosen filename for a brand-new doc that hasn't been
        // written yet — record the name on the editor but skip the read.
        if( QFile::exists( file ) )
            ce->setPlainText( fileToString( file, "EditorWidget::addDocument" ) );
        ce->setFile( file );
    }
    connect( ce->document(), &QTextDocument::contentsChanged,
                       this, &EditorWidget::documentWasModified, Qt::UniqueConnection);

    m_fileList[file] = ce;
    if( main ){
        m_docWidget->setCurrentWidget( ce );
        if( file.isEmpty() ) ce->document()->setModified( true ); // New
        documentWasModified();
        enableFileActs( true );
        enableDebugActs( true );
        updateDoc();
    }
}

int EditorWidget::calcTabstopWidth() 
{
    QFontMetrics fm( m_font );
    QString spaces( m_tabSize, QChar(' ') );
    return fm.horizontalAdvance( spaces );
}

void EditorWidget::open()
{
#ifdef __EMSCRIPTEN__
    // WASM: async picker. Cancel does not fire the callback.
    QFileDialog::getOpenFileContent(
        tr("All files")+" (*);;"
        +tr("Arduino")+" (*.ino);;"
        +tr("C/C++")+" (*.c *.cc *.cpp *.h *.hpp);;"
        +tr("Asm")+" (*.asm);;"
        +tr("GcBasic")+" (*.gcb)",
        [this]( const QString& fileName, const QByteArray& content )
        {
            if( fileName.isEmpty() ) return;
            // QString tmp = "/tmp/" + QFileInfo( fileName ).fileName();
            QString tmp = MainWindow::self()->getConfigPath( QFileInfo( fileName ).fileName() );
            QFile f( tmp );
            if( !f.open( QIODevice::WriteOnly ) ){
                qDebug() << "EditorWidget::open: cannot stage" << tmp;
                return;
            }
            f.write( content );
            f.close();
            loadFile( tmp );
        });
#else
    QString dir = m_lastDir;
    QString fileName = QFileDialog::getOpenFileName( this, tr("Load File"), dir,
                       tr("All files")+" (*);;"
                       +tr("Arduino")+" (*.ino);;"
                       +tr("C/C++")+" (*.c *.cc *.cpp *.h *.hpp);;"
                       +tr("Asm")+" (*.asm);;"
                       +tr("GcBasic")+" (*.gcb)" );

    if( !fileName.isEmpty() ) loadFile( fileName );
#endif
}

void EditorWidget::openRecentFile()
{
    QAction* action = qobject_cast<QAction*>( sender() );
    if( action )
    {
        QString file = action->data().toString();
        QFile pfile( file );
        if( pfile.exists() ) loadFile( file );
        else{
            QMessageBox::StandardButton ret
            = QMessageBox::warning( this, "EditorWidget::openRecentFile",
                                   tr("\nCan't find file:\n")+
                                   file+"\n\n"+
                                   tr("Do you want to remove it from Recent Files?\n"),
              QMessageBox::Yes | QMessageBox::No );

            if( ret == QMessageBox::Yes )
            {
                QSettings* settings = MainWindow::self()->settings();
                QStringList files = settings->value("recentFileList").toStringList();
                files.removeAll( file );
                settings->setValue("recentFileList", files );
                updateRecentFileActions();
            }
        }
    }
}

void EditorWidget::dragEnterEvent( QDragEnterEvent* event)
{
    event->accept();
}

void EditorWidget::dropEvent( QDropEvent* event )
{
    QString id = event->mimeData()->text();

    QString file = "file://";
    if( id.startsWith( file ) )
    {
        id.replace( file, "" ).replace("\r\n", "" ).replace("%20", " ");
#ifdef _WIN32
        if( id.startsWith( "/" )) id.remove( 0, 1 );
#endif
        loadFile( id );
    }
}

void EditorWidget::keyPressEvent( QKeyEvent* event )
{
    if( event->modifiers() & Qt::ControlModifier )
    {
        if     ( event->key() == Qt::Key_N ) newFile();
        else if( event->key() == Qt::Key_O ) open();
        else if( event->key() == Qt::Key_R ) reload();
        else if( event->key() == Qt::Key_S )
        {
            if( event->modifiers() & Qt::ShiftModifier) saveAs();
            else                                        save();
        }
    }
}

void EditorWidget::restoreFile( QString filePath )
{
    if( !QFileInfo::exists( filePath ) )
    {
        m_outPane.appendLine( tr("File doesn't exist")+":\n"+filePath );
        return;
    }
    if( m_fileList.contains( filePath ) ) return;

    addDocument( filePath, false );
}

void EditorWidget::loadFile( QString filePath )
{
    if( !QFileInfo::exists( filePath ) )
    {
        m_outPane.appendLine( tr("File doesn't exist")+":\n"+filePath );
        return;
    }
    if( m_fileList.contains( filePath ) )
    {
        m_docWidget->setCurrentWidget( m_fileList.value( filePath ) );
        return;
    }
    QApplication::setOverrideCursor( Qt::WaitCursor );

    m_lastDir = filePath;
    addDocument( filePath, true );
    enableFileActs( true );
    enableDebugActs( true );
    addRecentFile( filePath );

    QApplication::restoreOverrideCursor();
}

void EditorWidget::reload()
{
    CodeEditor* ce = getCodeEditor();
    QString file = m_fileList.key( m_docWidget->currentWidget() );
    ce->setPlainText( fileToString( file, "EditorWidget::reload" ) );
    ce->document()->setModified( false );
    documentWasModified();
}

bool EditorWidget::save()
{
#ifdef __EMSCRIPTEN__
    // WASM has no writable host path; always route through the browser
    // download flow in saveAs().
    return saveAs();
#else
    QString file = getCodeEditor()->getFile();
    if( file.isEmpty() ) return saveAs();
    else                 return saveFile( file );
#endif
}

bool EditorWidget::saveAs()
{
    CodeEditor* ce = getCodeEditor();

#ifdef __EMSCRIPTEN__
    // WASM: no Save dialog. Push the document to the browser as a download
    // named after the current file (or a default).
    QString defName = QFileInfo( ce->getFile() ).fileName();
    if( defName.isEmpty() ) defName = "untitled.txt";

    QFileDialog::saveFileContent( ce->toPlainText().toUtf8(), defName );

    ce->document()->setModified( false );
    documentWasModified();
    return true;
#else
    QFileInfo fi = QFileInfo( ce->getFile() );
    QString ext  = fi.suffix();
    QString path = fi.absolutePath();
    if( path == "" ) path = m_lastDir;

    QString extensions = "";
    if( ext == "" ) extensions = tr("All files")+" (*);;Arduino (*.ino);;Asm (*.asm);;GcBasic (*.gcb)";
    else            extensions = "."+ext+"(*."+ext+");;"+tr("All files")+" (*.*)";

    QString fileName = QFileDialog::getSaveFileName( MainWindow::self(), tr("Save Document As"), path, extensions );
    if( fileName.isEmpty() ) return false;

    m_fileList.remove( ce->getFile() );
    m_fileList[fileName] = ce;

    return saveFile( fileName );
#endif
}

bool EditorWidget::saveFile( QString fileName )
{
    QFile file( fileName );
    if( !file.open( QFile::WriteOnly | QFile::Text) )
    {
        QMessageBox::warning(this, "EditorWindow::saveFile",
                             tr("Cannot write file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }
    QApplication::setOverrideCursor( Qt::WaitCursor );

    QTextStream out( &file );
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    out.setCodec("UTF-8");
#else
    out.setEncoding(QStringConverter::Utf8);
#endif

    CodeEditor* ce = getCodeEditor();
    out << ce->toPlainText();
    file.close();

    //ce->saveConfig();
    ce->document()->setModified( false );
    documentWasModified();

    m_docWidget->setTabText( m_docWidget->currentIndex(), getFileName(fileName) );
    ce->setFile( fileName );

    QApplication::restoreOverrideCursor();
    return true;
}

bool EditorWidget::maybeSave()
{
    if( m_fileList.isEmpty() ) return true;
    if( getCodeEditor()->document()->isModified() )
    {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning( this, "EditorWindow::saveFile",
              tr("\nThe Document has been modified.\nDo you want to save your changes?\n"),
              QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if     ( ret == QMessageBox::Save )   return save();
        else if( ret == QMessageBox::Cancel ) return false;
    }
    return true;
}

void EditorWidget::documentWasModified()
{
    CodeEditor* ce = getCodeEditor();
    QTextDocument *doc = ce->document();

    bool    modified = doc->isModified();
    int     index    = m_docWidget->currentIndex();
    QString tabText  = m_docWidget->tabText( index );

    if     ( modified && !tabText.endsWith("*") ) tabText.append("*");
    else if( !modified && tabText.endsWith("*") ) tabText.remove("*");

    m_docWidget->setTabText( index, tabText );

    redoAct->setEnabled( false );
    undoAct->setEnabled( false );
    if( doc->isRedoAvailable() ) redoAct->setEnabled( true );
    if( doc->isUndoAvailable() ) undoAct->setEnabled( true );
}

void EditorWidget::updateDoc( int )
{
#ifndef HIDE_SOME_ACTIONS
    CodeEditor* ce = getCodeEditor();
    m_findRepDialog->setEditor( ce );
    if( !ce ) return;
    bool show = ce->compName() == "None" ? false : true;
    confCompAct->setVisible( show );
    confFileAct->setVisible( true );
#endif
}

void EditorWidget::closeFileTab( const QString& filePath )
{
    if( !m_fileList.contains( filePath ) ) return;
    QWidget* w = m_fileList.value( filePath );
    int idx = m_docWidget->indexOf( w );
    if( idx >= 0 ) closeTab( idx );
}

void EditorWidget::closeTab( int index )
{
    if( m_debuggerToolBar->isVisible() ) stop();

    m_docWidget->setCurrentIndex( index );
    if( !maybeSave() ) return;

    CodeEditor* ce = getCodeEditor();
    ce->closing();
    m_fileList.remove( m_fileList.key( ce ) );
    m_docWidget->removeTab( index );
    delete ce;

    if( m_fileList.isEmpty() )  // disable file actions
    {
        enableFileActs( false );
        enableDebugActs( false );
#ifndef HIDE_SOME_ACTIONS
        m_findRepDialog->hide();
        confFileAct->setVisible( false );
        confCompAct->setVisible( false );
#endif
    }

    int last = m_docWidget->count()-1;
    if( index > last ) index = last;
    m_docWidget->setCurrentIndex( index );

#ifndef HIDE_SOME_ACTIONS
    m_findRepDialog->setEditor( getCodeEditor() );
#endif
}

void EditorWidget::confEditor()
{
    if( !m_propDialog )
    {
        if( m_help == "" ) m_help = MainWindow::self()->getHelp( "editor" );

        m_propDialog = new PropDialog( this, m_help );
        m_propDialog->setComponent( this, false );
    }
    m_propDialog->show();
}

void EditorWidget::confFile()
{
    CodeEditor* ce = getCodeEditor();
    if( ce ) ce->fileProps();
}

void EditorWidget::confCompiler()
{
    CodeEditor* ce = getCodeEditor();
    if( !ce ) return;
    Compiler* c = ce->getCompiler();
    if( !c || c->compName() == "None" ) return;
    c->compilerProps();
}

void EditorWidget::addRecentFile( QString filePath )
{
    QSettings* settings = MainWindow::self()->settings();
    QStringList files = settings->value("recentFileList").toStringList();
    files.removeAll( filePath );
    files.prepend( filePath );
    while( files.size() > MaxRecentFiles ) files.removeLast();
    settings->setValue("recentFileList", files );
    updateRecentFileActions();
}

void EditorWidget::updateRecentFileActions()
{
    QSettings* settings = MainWindow::self()->settings();
    QStringList files = settings->value("recentFileList").toStringList();

    int numRecentFiles = qMin( files.size(), (int)MaxRecentFiles );

#ifndef HIDE_SOME_ACTIONS
    for( int i=0; i<numRecentFiles; i++ )
    {
        QString text = QString::number(i + 1)+" "+getFileName( files[i] );
        recentFileActs[i]->setText(text);
        recentFileActs[i]->setData( files[i] );
        recentFileActs[i]->setVisible( true );
    }
    for( int i=numRecentFiles; i<MaxRecentFiles; i++ ) recentFileActs[i]->setVisible(false);
#endif
}

void EditorWidget::readSettings()
{
    QSettings* settings = MainWindow::self()->settings();

    m_showSpaces = false;
    m_spaceTabs  = false;
    m_autoClose = 0;
    m_fontSize = 14;
    m_tabSize = 4;

    m_font.setFamily("Roboto");
    m_font.setWeight( QFont::Normal );
    m_font.setPixelSize( m_fontSize );

    if( settings->contains( "Editor_show_spaces" ) )
        setShowSpaces( settings->value( "Editor_show_spaces" ).toBool() );

    if( settings->contains( "Editor_tab_size" ) )
        setTabSize( settings->value( "Editor_tab_size" ).toInt() );
    else setTabSize( 4 );

    if( settings->contains( "Editor_font_size" ) )
        setFontSize( settings->value( "Editor_font_size" ).toInt() );

    bool spacesTab = false;
    if( settings->contains( "Editor_spaces_tabs" ) )
        spacesTab = settings->value( "Editor_spaces_tabs" ).toBool();

    if( settings->contains( "Editor_autoclose" ) )
        setAutoClose( settings->value( "Editor_autoclose" ).toInt() );

    setSpaceTabs( spacesTab );

    restoreGeometry( settings->value("geometry").toByteArray() );
    m_docWidget->restoreGeometry( settings->value("docWidget/geometry").toByteArray() );
    m_lastDir = settings->value("lastDir").toString();
}

void EditorWidget::writeSettings()
{
    QSettings* settings = MainWindow::self()->settings();
    settings->setValue( "geometry", saveGeometry() );
    settings->setValue( "docWidget/geometry", m_docWidget->saveGeometry() );
    settings->setValue( "lastDir", m_lastDir );
    settings->setValue( "Editor_autoclose", QString::number( m_autoClose ) );
}

void EditorWidget::findReplaceDialog()
{
#ifndef HIDE_SOME_ACTIONS
    CodeEditor* ce = getCodeEditor();

    m_findRepDialog->setEditor( ce );

    QString text =ce->textCursor().selectedText();
    if( text != "" ) m_findRepDialog->setTextToFind( text );

    m_findRepDialog->show();
#endif
}

void EditorWidget::enableFileActs( bool enable )
{
    saveAct->setEnabled( enable );
#ifndef HIDE_SOME_ACTIONS
    saveAsAct->setEnabled( enable );
    findQtAct->setEnabled( enable );
#endif
    cutAct->setEnabled( enable );
    copyAct->setEnabled( enable );
    pasteAct->setEnabled( enable );
    undoAct->setEnabled( enable );
    redoAct->setEnabled( enable );
}

void EditorWidget::enableDebugActs( bool enable )
{
    debugAct->setEnabled( enable );
    runAct->setEnabled( enable );
    stepAct->setEnabled( enable );
    stepOverAct->setEnabled( enable );
    pauseAct->setEnabled( enable );
    resetAct->setEnabled( enable );
    stopAct->setEnabled( enable );
#ifndef HIDE_SOME_ACTIONS
    compileAct->setEnabled( enable );
#endif
    loadAct->setEnabled( enable );
}

void EditorWidget::setStepActs( bool s )
{
    runAct->setEnabled( s );
    stepAct->setEnabled( s );
    stepOverAct->setEnabled( s );
    resetAct->setEnabled( s );
    pauseAct->setEnabled( !s );
}

void EditorWidget::createWidgets()
{
    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setSpacing(0);
    hLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vLayout = new QVBoxLayout( this );
    vLayout->setSpacing(0);
    vLayout->setContentsMargins(0, 0, 0, 0);

    hLayout->addWidget( m_editorToolBar );
#ifndef HIDE_SOME_ACTIONS
    hLayout->addWidget( m_findToolBar );
#endif
    hLayout->addWidget( m_compileToolBar );
    hLayout->addWidget( m_debuggerToolBar );
    hLayout->addStretch();

    // double fs = MainWindow::self()->fontScale()*20;
    // QToolButton* closeEditorBtn = new QToolButton( this );
    // closeEditorBtn->setIcon( QIcon(":/remove.svg") );
    // closeEditorBtn->setIconSize( QSize( fs*0.9, fs*0.9 ) );
    // closeEditorBtn->setToolTip( tr("Close Editor") );
    // closeEditorBtn->setAutoRaise( true );
    // connect( closeEditorBtn, &QToolButton::clicked,
    //          [](){ MainWindow::self()->hideEditor(); } );
    // hLayout->addWidget( closeEditorBtn );

    vLayout->addLayout( hLayout );

    QSplitter* splitter0 = new QSplitter( this );
    splitter0->setObjectName("splitter0");
    splitter0->setOrientation( Qt::Vertical );
    vLayout->addWidget( splitter0 );

    m_docWidget = new QTabWidget( this );
    m_docWidget->setObjectName("docWidget");
    m_docWidget->setTabPosition( QTabWidget::North );
    m_docWidget->setTabsClosable ( true );
    m_docWidget->setContextMenuPolicy( Qt::CustomContextMenu );

    double fontScale = MainWindow::self()->fontScale();
    QString fontSize = QString::number( int(10*fontScale) );
    m_docWidget->tabBar()->setStyleSheet("QTabBar { font-size:"+fontSize+"px; }");
    m_docWidget->setMovable( true );

    splitter0->addWidget( m_docWidget );
    splitter0->addWidget( &m_outPane );
    splitter0->setSizes( {500, 0} );

    connect( m_docWidget, SIGNAL( tabCloseRequested(int)),
             this,        SLOT(   closeTab(int)), Qt::UniqueConnection);

    connect( m_docWidget, SIGNAL(currentChanged(int)), this, SLOT(updateDoc(int)), Qt::UniqueConnection);

#ifndef HIDE_SOME_ACTIONS
    m_findRepDialog = new FindReplace( this );
    m_findRepDialog->setModal( false );
#endif
}

void EditorWidget::createActions()
{
#ifndef HIDE_SOME_ACTIONS
    confEditAct = new QAction(QIcon(":/blank.png"), tr("Editor Settings"), this);
    confEditAct->setStatusTip(tr("Editor Settings"));
    connect( confEditAct, SIGNAL(triggered()), this, SLOT(confEditor()), Qt::UniqueConnection);

    confFileAct = new QAction(QIcon(":/blank.png"), tr("File Settings"), this);
    confFileAct->setStatusTip(tr("Compiler Settings"));
    connect( confFileAct, SIGNAL(triggered()), this, SLOT(confFile()), Qt::UniqueConnection);
    confFileAct->setVisible( false );

    confCompAct = new QAction(QIcon(":/blank.png"), tr("Compiler Settings"), this);
    confCompAct->setStatusTip(tr("Compiler Settings"));
    connect( confCompAct, SIGNAL(triggered()), this, SLOT(confCompiler()), Qt::UniqueConnection);
    confCompAct->setVisible( false );

    for( int i=0; i<MaxRecentFiles; i++ )
    {
        recentFileActs[i] = new QAction( this );
        recentFileActs[i]->setVisible( false );
        connect( recentFileActs[i], SIGNAL( triggered() ),
                 this,              SLOT( openRecentFile() ), Qt::UniqueConnection);
    }
#endif

    newAct = new QAction(QIcon(":/new.svg"), tr("&New\tCtrl+N"), this);
    newAct->setStatusTip(tr("Create a new file"));
    connect( newAct, SIGNAL(triggered()), this, SLOT(newFile()), Qt::UniqueConnection);

    openAct = new QAction(QIcon(":/open.svg"), tr("&Open...\tCtrl+O"), this);
    openAct->setStatusTip(tr("Open an existing file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(open()), Qt::UniqueConnection);

    saveAct = new QAction(QIcon(":/save.svg"), tr("&Save\tCtrl+S"), this);
    saveAct->setStatusTip(tr("Save the document to disk"));
    saveAct->setEnabled(false);
    connect(saveAct, SIGNAL(triggered()), this, SLOT(save()), Qt::UniqueConnection);

#ifndef HIDE_SOME_ACTIONS
    saveAsAct = new QAction(QIcon(":/saveas.svg"),tr("Save &As...\tCtrl+Shift+S"), this);
    saveAsAct->setStatusTip(tr("Save the document under a new name"));
    saveAsAct->setEnabled(false);
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAs()), Qt::UniqueConnection);
#endif

    cutAct = new QAction(QIcon(":/cut.svg"), tr("Cu&t\tCtrl+X"), this);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the clipboard"));
    cutAct->setEnabled(false);
    connect(cutAct, SIGNAL(triggered()), this, SLOT(cut()), Qt::UniqueConnection);

    copyAct = new QAction(QIcon(":/copy.svg"), tr("&Copy\tCtrl+C"), this);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the clipboard"));
    copyAct->setEnabled(false);
    connect(copyAct, SIGNAL(triggered()), this, SLOT(copy()), Qt::UniqueConnection);

    pasteAct = new QAction(QIcon(":/paste.svg"), tr("&Paste\tCtrl+V"), this);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current selection"));
    pasteAct->setEnabled(false);
    connect(pasteAct, SIGNAL(triggered()), this, SLOT(paste()), Qt::UniqueConnection);

    undoAct = new QAction(QIcon(":/undo.svg"), tr("Undo\tCtrl+Z"), this);
    undoAct->setStatusTip(tr("Undo the last action"));
    undoAct->setEnabled(false);
    connect(undoAct, SIGNAL(triggered()), this, SLOT(undo()), Qt::UniqueConnection);

    redoAct = new QAction(QIcon(":/redo.svg"), tr("Redo\tCtrl+Shift+Z"), this);
    redoAct->setStatusTip(tr("Redo the last action"));
    redoAct->setEnabled(false);
    connect(redoAct, SIGNAL(triggered()), this, SLOT(redo()));

    runAct =  new QAction(QIcon(":/runtobk.png"),tr("Run To Breakpoint"), this);
    runAct->setStatusTip(tr("Run to next breakpoint"));
    runAct->setEnabled(false);
    connect(runAct, SIGNAL(triggered()), this, SLOT(run()), Qt::UniqueConnection);

    stepAct = new QAction(QIcon(":/step.svg"),tr("Step"), this);
    stepAct->setStatusTip(tr("Step debugger"));
    stepAct->setEnabled(false);
    connect( stepAct, SIGNAL(triggered()), this, SLOT(step()), Qt::UniqueConnection );

    stepOverAct = new QAction(QIcon(":/rotatecw.svg"),tr("StepOver"), this);
    stepOverAct->setStatusTip(tr("Step Over"));
    stepOverAct->setEnabled(false);
    stepOverAct->setVisible(false);
    connect( stepOverAct, SIGNAL(triggered()), this, SLOT(stepOver()), Qt::UniqueConnection );

    pauseAct = new QAction(QIcon(":/pause.svg"),tr("Pause"), this);
    pauseAct->setStatusTip(tr("Pause debugger"));
    pauseAct->setEnabled(false);
    connect( pauseAct, SIGNAL(triggered()), this, SLOT(pause()), Qt::UniqueConnection );

    resetAct = new QAction(QIcon(":/reset.svg"),tr("Reset"), this);
    resetAct->setStatusTip(tr("Reset debugger"));
    resetAct->setEnabled(false);
    connect( resetAct, SIGNAL(triggered()), this, SLOT(reset()), Qt::UniqueConnection );

    stopAct = new QAction(QIcon(":/stop.svg"),tr("Stop Debugger"), this);
    stopAct->setStatusTip(tr("Stop debugger"));
    stopAct->setEnabled(false);
    connect( stopAct, SIGNAL(triggered()), this, SLOT(stop()), Qt::UniqueConnection );

#ifndef HIDE_SOME_ACTIONS
    compileAct = new QAction(QIcon(":/verify.svg"),tr("Compile"), this);
    compileAct->setStatusTip(tr("Compile Source"));
    compileAct->setEnabled(false);
    connect( compileAct, SIGNAL(triggered()), this, SLOT(compile()), Qt::UniqueConnection );
#endif

    loadAct = new QAction(QIcon(":/upload.svg"),tr("UpLoad"), this);
    loadAct->setStatusTip(tr("Load Firmware"));
    loadAct->setEnabled(false);
    connect( loadAct, SIGNAL(triggered()), this, SLOT(upload()), Qt::UniqueConnection );

#ifndef HIDE_SOME_ACTIONS
    findQtAct = new QAction(QIcon(":/find.svg"),tr("Find Replace"), this);
    findQtAct->setStatusTip(tr("Find Replace"));
    findQtAct->setEnabled(false);
    connect(findQtAct, SIGNAL(triggered()), this, SLOT(findReplaceDialog()), Qt::UniqueConnection);
#endif

    debugAct =  new QAction(QIcon(":/debug.svg"),tr("Debug"), this);
    debugAct->setStatusTip(tr("Start Debugger"));
    debugAct->setEnabled(false);
    connect(debugAct, SIGNAL(triggered()), this, SLOT(debug()), Qt::UniqueConnection);
}

void EditorWidget::createToolBars()
{
    double fs = MainWindow::self()->fontScale()*25;

    m_editorToolBar = new QToolBar( this );
    m_editorToolBar->setIconSize( QSize( fs, fs ) );
#ifndef HIDE_SOME_ACTIONS
    m_settingsMenu.addAction( confEditAct );
    m_settingsMenu.addAction( confFileAct );
    m_settingsMenu.addAction( confCompAct );

    QToolButton* settingsButton = new QToolButton( this );
    settingsButton->setToolTip( tr("Settings") );
    settingsButton->setMenu( &m_settingsMenu );
    settingsButton->setIcon( QIcon(":/config.svg") );
    settingsButton->setPopupMode( QToolButton::InstantPopup );

    m_editorToolBar->addWidget( settingsButton );
    QWidget* spacer = new QWidget();
    spacer->setFixedWidth( 15 );
    m_editorToolBar->addWidget( spacer );
    m_editorToolBar->addSeparator();//..........................

    for( int i=0; i<MaxRecentFiles; i++ ) m_fileMenu.addAction( recentFileActs[i] );
    QToolButton* fileButton = new QToolButton( this );
    fileButton->setToolTip( tr("Last Files") );
    fileButton->setMenu( &m_fileMenu );
    fileButton->setIcon( QIcon(":/lastfiles.svg") );
    fileButton->setPopupMode( QToolButton::InstantPopup );

    m_editorToolBar->addWidget( fileButton );

    m_findToolBar = new QToolBar( this );
    m_findToolBar->setIconSize( QSize( fs, fs ) );
    m_findToolBar->addAction(findQtAct);
    m_findToolBar->addSeparator();
#endif
    m_editorToolBar->addAction(newAct);
    m_editorToolBar->addAction(openAct);
    m_editorToolBar->addAction(saveAct);
#ifndef HIDE_SOME_ACTIONS
    m_editorToolBar->addAction(saveAsAct);
#endif
    m_editorToolBar->addSeparator();

    m_compileToolBar = new QToolBar( this );
    m_compileToolBar->setIconSize( QSize( fs, fs ) );
#ifndef HIDE_SOME_ACTIONS
    m_compileToolBar->addAction(compileAct);
#endif
    m_compileToolBar->addAction(loadAct);
    m_compileToolBar->addSeparator();
    m_compileToolBar->addAction(debugAct);

    m_debuggerToolBar = new QToolBar( this );
    m_debuggerToolBar->setIconSize( QSize( fs, fs ) );
    m_debuggerToolBar->addAction(stepAct);
    m_debuggerToolBar->addAction(stepOverAct);
    m_debuggerToolBar->addAction(runAct);
    m_debuggerToolBar->addAction(pauseAct);
    m_debuggerToolBar->addAction(resetAct);
    m_debuggerToolBar->addSeparator();
    m_debuggerToolBar->addAction(stopAct);
    m_debuggerToolBar->setVisible( false );
}

#include  "moc_editorwidget.cpp"
