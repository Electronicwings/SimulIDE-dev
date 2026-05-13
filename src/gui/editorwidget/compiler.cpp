/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QRegularExpression>
#include <QDomDocument>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>

#ifdef __EMSCRIPTEN__
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <emscripten/fetch.h>
#include <emscripten/val.h>
#include <string.h>
using emscripten::val;
#endif

#include "compiler.h"
#include "mcu.h"
#include "simulator.h"
#include "editorwindow.h"
#ifdef __EMSCRIPTEN__
#include "jsbridge.h"
#endif
#include "outpaneltext.h"
#include "propdialog.h"
#include "mainwindow.h"
#include "utils.h"

#include "stringprop.h"
#ifdef __EMSCRIPTEN__
#include "qzipreader.h"
#endif

Compiler::Compiler( CodeEditor* editor, OutPanelText* outPane )
        : QObject( editor )
        , CompBase( "Compiler", "" )
        , m_compProcess( this )
{
    m_editor = editor;
    m_outPane = outPane;

    m_file     = editor->getFile();
    m_fileDir  = getFileDir( m_file );
    m_fileExt  = getFileExt( m_file );
    m_fileName = getBareName( m_file );
    m_firmware = "";
    m_buildPath = m_fileDir;
    m_fileProps = false;

    clearCompiler();

    addPropGroup( { "Hidden", {
        new StrProp<Compiler>("itemtype","" ,""
                             , this, &Compiler::itemType, &Compiler::setItemType ),

        new StrProp<Compiler>("compilertype","" ,""
                             , this, &Compiler::compName, &Compiler::setCompName ),
    }, groupHidden} );

    addPropGroup( { tr("Compiler Settings"), {
        new ComProperty("", tr("For this compiler type:"),"","",0),

        new StrProp<Compiler>("ToolPath", tr("Tool Path"),""
                             , this, &Compiler::toolPath, &Compiler::setToolPath, 0,"path"),
    }, 0} );
}
Compiler::~Compiler(){}

void Compiler::clearCompiler()
{
    m_toolPath.clear();
    m_command.clear();
    m_arguments.clear();
    m_argsDebug.clear();
    m_compilerType.clear();
}

QString Compiler::replaceData( QString str )
{
    QString filePath  = addQuotes( m_file );
    QString inclPath  = addQuotes( m_inclPath );
    QString buildPath = addQuotes( m_buildPath );

    str = str.replace( "$filePath" , filePath )
             .replace( "$fileDir"  , m_fileDir )
             .replace( "$fileName" , m_fileName )
             .replace( "$fileExt"  , m_fileExt )
             .replace( "$inclPath" , inclPath )
             .replace( "$buildPath", buildPath )
             .replace( "$extraArgs", m_extraArgs );
    return str;
}

void Compiler::loadCompiler( QString file )
{
    clearCompiler();
    if( file.isEmpty() ) return;

    QDomDocument domDoc = fileToDomDoc( file, "Compiler::loadCompiler" );
    if( domDoc.isNull() )
    {
        m_outPane->appendLine( "Error: Compiler file not valid:\n"+file+"\n" );
        return;
    }
    QDomElement compiler = domDoc.documentElement();

    m_uploadHex = compiler.attribute( "uploadhex" ) != "false";
    QString inclPath = "";

    m_compName = compiler.attribute( "name" );
    m_id = m_compName;

    m_compilerType = compiler.attribute( "type" );

    if( compiler.hasAttribute("syntax") ) m_editor->setSyntaxFile( compiler.attribute( "syntax" )+".syntax" );
    if( compiler.hasAttribute("buildPath") )
    {
        QString path = replaceData( compiler.attribute( "buildPath" ) );
        QDir buildDir= QFileInfo( m_file ).absoluteDir();
        if( !buildDir.cd( path ) )
        {
            buildDir.mkpath( m_fileDir+QDir::separator()+path); // Create build Dir
            buildDir.cd( path );
        }
        m_buildPath = buildDir.absolutePath()+QDir::separator();
    }
    if( compiler.hasAttribute("inclPath") ) inclPath = compiler.attribute( "inclPath" );
    if( !inclPath.isEmpty() ) m_inclPath = inclPath;

    QDomNode node = compiler.firstChild();
    while( !node.isNull() )
    {
        QDomElement step = node.toElement();
        if( step.tagName() == "step")
        {
            QString args;
            QString command   = step.attribute("command");
            QString arguments = step.attribute("arguments");
            QString argsDebug = step.attribute("argsDebug");
            if( argsDebug.isEmpty() ) args = argsDebug = arguments ;
            else args = arguments + argsDebug;

            if( args.contains("$inclPath") )
            {
                addProperty( tr("Compiler Settings"),
                new StrProp<Compiler> ("InclPath", tr("Include Path"),"", this
                                       , &Compiler::includePath, &Compiler::setIncludePath, 0) );
            }
            if( args.contains("$extraArgs") )
            {
                addFilePropHead();
                addProperty( tr("Compiler Settings"),
                new StrProp<Compiler> ("extraArgs", tr("Extra build arguments"),"", this
                                       , &Compiler::extraArgs, &Compiler::setextraArgs, 0) );
            }
            if( args.contains("$device") )
            {
                addFilePropHead();
                addProperty( tr("Compiler Settings"),
                new StrProp<Compiler>("Device"  , tr("Device")  ,"", this
                                      , &Compiler::device,   &Compiler::setDevice, 0 ) );
            }
            if( args.contains("$family") )
            {
                addFilePropHead();
                addProperty( tr("Compiler Settings"),
                new StrProp<Compiler>("Family"  , tr("Family")  ,""
                                      , this, &Compiler::family,   &Compiler::setFamily, 0 ) );
            }
            m_command.append( command );
            m_arguments.append( arguments );
            m_argsDebug.append( argsDebug );
        }
        node = node.nextSibling();
    }
    readSettings();
    //m_outPane->appendLine( "-------------------------------------------------------" );
    m_outPane->appendLine( m_compName+tr(" Compiler successfully loaded.\n") );
}

void Compiler::addFilePropHead()
{
    if( m_fileProps ) return;
    m_fileProps = true;

    addProperty( tr("Compiler Settings"),
    new ComProperty("", "separator","","",0) );

    addProperty( tr("Compiler Settings"),
    new ComProperty("", tr("For this file:"),"","",0) );
}

int Compiler::compile( bool debug )
{
    if     ( m_compName == "None" ) m_outPane->appendLine( tr("     No Compiler Defined") );
    else if( m_command.isEmpty() )  m_outPane->appendLine( tr("     No command Defined") );

    int error = 0;
    QApplication::setOverrideCursor( Qt::WaitCursor );

    m_fileList.clear();
    preProcess();

    for( int i=0; i<m_command.size(); ++i )
    {
        QString command = m_toolPath + m_command.at(i);
        if( !checkCommand( command ) )
        {
            m_outPane->appendLine( "ERROR: "+command );
            toolChainNotFound();
            error = -1;
            break;
        }
        command = addQuotes( command );

        QString arguments = debug ? m_argsDebug.at(i) : m_arguments.at(i);
        arguments = replaceData( arguments );

        if( arguments.contains("$family") )
        {
            if( m_family.isEmpty() )
            {
                m_outPane->appendLine( tr("     Error: Family not defined") );
                error = -1;
                break;
            }
            else arguments = arguments.replace( "$family", m_family );
        }
        if( arguments.contains("$device") )
        {
            if( m_device.isEmpty() )
            {
                m_outPane->appendLine( tr("     Error: Device not defined") );
                error = -1;
                break;
            }
            else arguments = arguments.replace( "$device", m_device );
        }
        error = runBuildStep( command + arguments );
        if( error > 0 ) break;
    }
    if( error == 0 ) compiled( m_buildPath+m_fileName+".hex" );

    QApplication::restoreOverrideCursor();
    return error;
}

#ifdef __EMSCRIPTEN__

// Fetch context kept alive across the async call. emscripten_fetch holds
// pointers to requestData/requestHeaders for the duration of the call,
// so the request body must outlive the synchronous emscripten_fetch()
// invocation; we own it here. QPointer<Compiler> auto-nullifies if the
// editor (and thus this Compiler) is destroyed mid-flight, so the
// callbacks won't dereference a dangling pointer.
namespace {
struct CompileFetchCtx {
    QPointer<Compiler> compiler;
    QByteArray         requestBody;
    QString            tmpHex;
    QString            tmpZippedBuild;
};

static void compileFetchDispatch( emscripten_fetch_t* f, bool isError )
{
    auto* ctx = static_cast<CompileFetchCtx*>( f->userData );

    QString err;
    if( isError ){
        err = QString( "HTTP %1 %2" )
                  .arg( f->status )
                  .arg( QString::fromUtf8( f->statusText ) );
    }
    QByteArray body;
    if( f->data && f->numBytes > 0 ){
        body = QByteArray( f->data, static_cast<int>(f->numBytes) );
    }

    QPointer<Compiler> guard = ctx->compiler;
    QString tmpHex = ctx->tmpHex;
    QString tmpZippedBuild = ctx->tmpZippedBuild;


    emscripten_fetch_close( f );
    delete ctx;

    if( !guard ) return;

    // Marshal the response onto the Qt main thread. The callbacks fire
    // from emscripten's main loop already, but the queued invocation is
    // safe under both pthread / non-pthread fetch configurations and
    // auto-cancels if the Compiler is destroyed before delivery.
    QMetaObject::invokeMethod( guard.data(),
        [guard, body, err, tmpHex, tmpZippedBuild](){
            if( guard ) guard->handleRemoteBuildResponse( body, err, tmpHex, tmpZippedBuild );
        }, Qt::QueuedConnection );
}

static void onCompileFetchSuccess( emscripten_fetch_t* f )
{
    compileFetchDispatch( f, false );
}

static void onCompileFetchError( emscripten_fetch_t* f )
{
    compileFetchDispatch( f, true );
}
} // namespace

// Fully async remote build via emscripten_fetch.
//
// Why not QNetworkAccessManager? Qt 5.15 + ASYNCIFY uses internal
// emscripten_sleep() loops to wait on the XHR; the WASM stack is held
// suspended and the QNetworkReply::finished signal never delivers — even
// when the HTTP 200 response is already in browser memory. Symptom is a
// frozen UI for the timeout duration ending in an exit error.
// emscripten_fetch is truly callback-driven and avoids the trap.
//
// Return codes:
//   0  : request dispatched (or already in flight; benign no-op)
//   -1 : pre-flight failure (no compiler, empty buffer)
int Compiler::remoteCompile( bool debug )
{
    if( m_remoteBusy ){
        m_outPane->appendLine( "Build already in progress, ignoring duplicate request." );
        return 0;
    }

    if( m_compName == "None" ){
        m_outPane->appendLine( tr("     No Compiler Defined") );
        return -1;
    }

    QByteArray source = m_editor->toPlainText().toUtf8();
    if( source.isEmpty() ){
        m_outPane->appendLine( "ERROR: editor buffer is empty" );
        return -1;
    }

    // Server contract:
    //   request:  { sketch, board, mcu, family, extra_args, debug, filename }
    //   response: { hex: "<intel-hex text>", stderr: "<verbose log>" }
    // Live MCU clock so the server can pass -DF_CPU=<hz> to avr-gcc.
    // Without this, the firmware uses the board's default F_CPU (Uno =
    // 16 MHz) and any UART/timer/delay code targeting a different
    // simulated clock runs at the wrong rate (e.g. Serial garbage when
    // the user dials the chip down to 8 MHz). Mcu::uiFreq() already
    // returns the value in Hz; the property's "MHz" suffix is just a
    // display label, not a unit conversion.
    qint64 fCpuHz = 0;
    if( Mcu* mcu = Mcu::self() ){
        const double hz = mcu->uiFreq();
        if( hz > 0 ) fCpuHz = static_cast<qint64>( hz );
    }

    QJsonObject reqObj;
    reqObj["sketch"]     = QString::fromUtf8( source );
    reqObj["board"]      = m_boardName;
    reqObj["mcu"]        = m_device;
    reqObj["family"]     = m_family;
    reqObj["f_cpu"]      = fCpuHz;
    reqObj["extra_args"] = m_extraArgs;
    reqObj["debug"]      = debug;
    reqObj["filename"]   = m_fileName + m_fileExt;
    QByteArray body = QJsonDocument( reqObj ).toJson( QJsonDocument::Compact );

    m_outPane->appendLine( "-------------------------------------------------------" );
    m_outPane->appendLine( QString( "Remote build: POST /simulation/arduino/compile  (sketch %1 bytes, %2)" )
                              .arg( source.size() ).arg( m_compName ) );

    // Allocate the per-request context. Owns the body buffer (kept alive
    // for the duration of emscripten_fetch) and a QPointer back to the
    // Compiler so callbacks can safely re-enter the instance.
    auto* ctx = new CompileFetchCtx;
    ctx->compiler    = this;
    ctx->requestBody = body;
    ctx->tmpHex      = MainWindow::self()->getConfigPath( m_fileName + ".hex" );
    ctx->tmpZippedBuild = MainWindow::self()->getConfigPath( m_fileName + ".zip" );

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init( &attr );
    strcpy( attr.requestMethod, "POST" );
    attr.attributes      = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData        = ctx;
    attr.requestData     = ctx->requestBody.constData();
    attr.requestDataSize = ctx->requestBody.size();
    attr.timeoutMSecs    = 60000;
    attr.onsuccess       = onCompileFetchSuccess;
    attr.onerror         = onCompileFetchError;

    // Header array: alternating key/value with a NULL terminator. The
    // strings must outlive the call; static const literals are fine.
    static const char* kHeaders[] = { "Content-Type", "application/json", nullptr };
    attr.requestHeaders = kHeaders;

    m_remoteBusy = true;
    emscripten_fetch( &attr,
        "/esa-api/compile/arduino" );
        // "http://ewskills.local/simulation/arduino/compile" );

    return 0; // dispatched
}

void Compiler::handleRemoteBuildResponse( const QByteArray& respBody,
                                          const QString&    errText,
                                          const QString&    tmpHex, 
                                          const QString&    tmpZippedBuild)
{
    // Consume the upload-chain flag once per build, regardless of
    // outcome. If we succeed below, we trigger upload(); if we fail,
    // the flag is dropped so a subsequent fresh "Compile" click
    // doesn't inadvertently auto-upload.
    const bool doUpload = m_uploadAfterBuild;
    m_uploadAfterBuild  = false;

    bool buildOk = false;

    if( !errText.isEmpty() ){
        m_outPane->appendLine( "ERROR: " + errText );
        if( !respBody.isEmpty() ) m_outPane->appendLine( QString::fromUtf8( respBody ) );
    } else {
        QJsonParseError parseErr;
        QJsonDocument respDoc = QJsonDocument::fromJson( respBody, &parseErr );
        if( parseErr.error != QJsonParseError::NoError ){
            m_outPane->appendLine( "ERROR: invalid JSON response: " + parseErr.errorString() );
            if( !respBody.isEmpty() ) m_outPane->appendLine( QString::fromUtf8( respBody ) );
        } else {
            QJsonObject respObj = respDoc.object();
            QString stderrText  = respObj.value( "stderr" ).toString();
            QString hexText     = respObj.value( "hex" ).toString();
            QString buildlinkText = respObj.value( "buildlink" ).toString();

            if( !stderrText.isEmpty() ) m_outPane->appendLine( stderrText );

            if( hexText.isEmpty() ){
                m_outPane->appendLine( "ERROR: build failed (empty hex)" );
            } else {
                QFile out( tmpHex );
                if( !out.open( QIODevice::WriteOnly ) ){
                    m_outPane->appendLine( "ERROR: cannot stage hex at " + tmpHex );
                } else {
                    QByteArray hexBytes = hexText.toUtf8();
                    out.write( hexBytes );
                    out.close();
                    m_outPane->appendLine( QString( "Build OK: %1 bytes -> %2" )
                                               .arg( hexBytes.size() ).arg( tmpHex ) );
                    m_hexPath = tmpHex;
                    compiled( tmpHex );
                    buildOk = true;
                }

                if( buildOk && !buildlinkText.isEmpty() ){
                    m_outPane->appendLine( "Downloading zipped build from: " + buildlinkText );
                    // Download the zip file and extract it
                    // const QString extractdir = MainWindow::self()->getConfigPath("build");
                    downloadZippedBuild( buildlinkText, tmpZippedBuild );
                }
            }
        }
    }

    m_remoteBusy = false;
    if( doUpload && buildOk ) upload();

    // Notify any JS subscriber on the bridge that the build round-trip is
    // done. Parent webapp can listen via Module.SimulIDEBridge.instance()
    //   .onCompiled(ok => ...). Buffered after upload() so onStateChange
    // notifications from a chained start/upload have a stable ordering.
    SimulIDEBridge::instance()->emitCompiled( buildOk );
}
#endif

int Compiler::runBuildStep( QString fullCommand )
{
    m_outPane->appendLine( "Executing:\n"+fullCommand+"\n" );
    m_compProcess.setWorkingDirectory( m_fileDir );
    m_compProcess.start( fullCommand  );
    m_compProcess.waitForFinished(-1);

    return getErrors();
}

void Compiler::compiled( QString firmware )
{
    //m_fileList.clear();
    //m_fileList.append( m_file ); //( m_fileName+m_fileExt );
    if( m_fileExt == ".hex" ) m_uploadHex = true;
    if( m_uploadHex ) m_firmware = firmware;
    else              m_firmware = "";
}

int Compiler::getErrors()
{
    int error = 0;

    QString p_stdout = m_compProcess.readAllStandardOutput();
    if( !p_stdout.isEmpty() ) error = getErrorLine( p_stdout );
    if( error ) return error;

    QString p_stderr = m_compProcess.readAllStandardError();
    if( !p_stderr.isEmpty() ) error = getErrorLine( p_stderr );
    return error;
}

int Compiler::getErrorLine( QString txt )
{
    m_outPane->appendLine( txt );

    int error = 0;
    for( QString line : txt.split("\n") )
    {
        if( !line.contains( m_fileName+m_fileExt ) ) continue;
        line = line.split( m_fileName+m_fileExt ).last();

        int errorLine = getFirstNumber( line );
        if( errorLine != 0 )
        {
            line = line.toLower();             // Make it case insensitive
            if( line.contains("error")   )
            {
                if( error == 0 ) error = errorLine;
                m_editor->addError( errorLine );
            }
            else if( line.contains("warning") ) m_editor->addWarning( errorLine );
        }
    }
    return error;
}

int Compiler::getFirstNumber( QString txt )
{
    int number = 0;
    QRegularExpression rx("[0-9]+");
    QRegularExpressionMatch match = rx.match( txt );
    if ( match.hasMatch()  ) number = match.captured(0).toInt();
    return number;
}

void Compiler::setToolPath( QString path )
{
    m_toolPath = path;
    MainWindow::self()->settings()->setValue( m_compName+"_toolPath", m_toolPath );
}

/*void Compiler::getIncludePath()
{
    QString oldPath = m_inclPath.isEmpty() ? m_fileDir : m_inclPath;
    QString path = getPath( tr("Select Compiler Include directory"), oldPath );
    if( !path.isEmpty() ) setIncludePath( path );
}*/

void Compiler::setIncludePath( QString path )
{
    m_inclPath = path;
    MainWindow::self()->settings()->setValue( m_compName+"_inclPath", m_inclPath );
}

void Compiler::toolChainNotFound()
{
    m_outPane->appendLine( "     : "+tr("Executable not found") );
    m_outPane->appendLine( "     : "+tr("Check that Tool Path is correct")+"\n" );
}

void Compiler::readSettings()
{
    QSettings* settings = MainWindow::self()->settings();

    QString prop = m_compName+"_toolPath";
    if( settings->contains( prop ) ) setToolPath( settings->value( prop ).toString() );
    prop = m_compName+"_inclPath";
    if( settings->contains( prop ) ) m_inclPath = settings->value( prop ).toString();
}

void Compiler::compilerProps()
{
    if( m_compName == "None" ) return;
    if( !m_propDialog )
    {
        if( m_help == "" ) m_help = MainWindow::self()->getHelp( m_compName );
        m_propDialog = new PropDialog( m_editor, m_help );
        m_propDialog->setComponent( this, false );
    }
    m_propDialog->show();
}

bool Compiler::checkCommand( QString executable )
{
    if( QFile::exists( executable ) ) return true;

    QProcess check;
    check.start( executable  );
    bool started = check.waitForStarted();
    if( started && !check.waitForFinished(1000) ) check.kill();
    check.readAllStandardError();
    check.readAllStandardOutput();

    return started;
}

#ifdef __EMSCRIPTEN__
void Compiler::downloadZippedBuild( const QString& downloadUrl, const QString& tmpZipPath )
{
    m_outPane->appendLine( "===== DOWNLOAD DEBUG =====" );
    m_outPane->appendLine( "URL: " + downloadUrl );
    m_outPane->appendLine( "Target: " + tmpZipPath );
    m_outPane->appendLine( "URL length: " + QString::number(downloadUrl.length()) );

    struct DownloadCtx {
        QPointer<Compiler> compiler;
        QString tmpZipPath;
        QString downloadUrl;
    };

    auto* ctx = new DownloadCtx;
    ctx->compiler = this;
    ctx->tmpZipPath = tmpZipPath;
    ctx->downloadUrl = downloadUrl;

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.userData = ctx;
    attr.timeoutMSecs = 120000;

    attr.onsuccess = [](emscripten_fetch_t* fetch) {
        auto* ctx = static_cast<DownloadCtx*>(fetch->userData);
        QPointer<Compiler> guard = ctx->compiler;
        QString tmpZipPath = ctx->tmpZipPath;

        if( guard ) {
            guard->m_outPane->appendLine( QString("SUCCESS: HTTP %1").arg(fetch->status) );
        }

        if( guard && fetch->data && fetch->numBytes > 0 ) {
            QFile zipFile(tmpZipPath);
            if( zipFile.open(QIODevice::WriteOnly) ) {
                zipFile.write(fetch->data, fetch->numBytes);
                zipFile.close();
                guard->m_outPane->appendLine(QString("Saved: %1 bytes").arg(fetch->numBytes));

                const QString extractdir = MainWindow::self()->getConfigPath("");
                bool extractok = guard->extractZippedBuild(tmpZipPath, extractdir);
                if(extractok){
                    guard->compiled( guard->m_hexPath );
                    guard->upload();
                }
            } else {
                guard->m_outPane->appendLine("ERROR: cannot open file: " + tmpZipPath);
            }
        } else {
            if( guard ) {
                guard->m_outPane->appendLine(QString("ERROR: no data (bytes=%1, ptr=%2)")
                                             .arg(fetch->numBytes)
                                             .arg((long long)fetch->data));
            }
        }

        emscripten_fetch_close(fetch);
        delete ctx;
    };

    attr.onerror = [](emscripten_fetch_t* fetch) {
        auto* ctx = static_cast<DownloadCtx*>(fetch->userData);
        QPointer<Compiler> guard = ctx->compiler;
        QString url = ctx->downloadUrl;

        if( guard ) {
            guard->m_outPane->appendLine( QString("FETCH ERROR:") );
            guard->m_outPane->appendLine( QString("  URL: %1").arg(url) );
            guard->m_outPane->appendLine( QString("  Status Code: %1").arg(fetch->status) );
            guard->m_outPane->appendLine( QString("  Status Text: %1").arg(QString::fromUtf8(fetch->statusText)) );
            guard->m_outPane->appendLine( QString("  Ready State: %1").arg(fetch->readyState) );
            if( fetch->data && fetch->numBytes > 0 ) {
                QString body = QString::fromUtf8(fetch->data, fetch->numBytes);
                if( body.length() > 200 ) body = body.left(200) + "...";
                guard->m_outPane->appendLine( QString("  Response: %1").arg(body) );
            }
        }

        emscripten_fetch_close(fetch);
        delete ctx;
    };

    m_outPane->appendLine( "Starting emscripten_fetch..." );
    emscripten_fetch(&attr, downloadUrl.toStdString().c_str());
}


bool Compiler::extractZippedBuild( const QString& zipPath, const QString extractPath )
{
    // m_outPane->appendLine( "===== ZIP EXTRACTION DEBUG =====" );
    
    QFileInfo zipInfo( zipPath );
    if( !zipInfo.exists() )
    {
        m_outPane->appendLine( "ERROR: zip file not found: " + zipPath );
        return false;
    }

    m_outPane->appendLine( "Build found: " + zipPath );
    m_outPane->appendLine( QString( "File size: %1 bytes" ).arg( zipInfo.size() ) );

    // Verify it's a valid zip by checking magic bytes
    QFile zipFile( zipPath );
    if( !zipFile.open( QIODevice::ReadOnly ) )
    {
        m_outPane->appendLine( "ERROR: cannot open zip file for reading" );
        return false;
    }

    QByteArray header = zipFile.read( 4 );
    zipFile.close();
    
    // ZIP magic number: 0x04034b50 (PK\x03\x04)
    if( header.size() < 4 || header[0] != 'P' || header[1] != 'K' )
    {
        m_outPane->appendLine( QString( "ERROR: invalid zip magic (got: %1,%2,%3,%4)" )
                              .arg( (int)header[0] ).arg( (int)header[1] )
                              .arg( (int)header[2] ).arg( (int)header[3] ) );
        m_outPane->appendLine( "First 4 bytes as hex: " + header.toHex() );
        return false;
    }

    // m_outPane->appendLine( "Zip magic bytes OK (PK signature found)" );

    // Create extraction directory
    QString extractDir = extractPath.isEmpty() ? (zipInfo.absoluteDir().absolutePath() + QDir::separator() 
                         + zipInfo.baseName() + "_extracted") : extractPath;
    QDir dir;
    if( !dir.exists( extractDir ) )
    {
        if( !dir.mkpath( extractDir ) )
        {
            m_outPane->appendLine( "ERROR: cannot create extraction directory: " + extractDir );
            return false;
        }
    }

    // m_outPane->appendLine( "Extract to: " + extractDir );

    // Read zip and manually extract files
    qZipReader zipReader( zipPath );
    if( !zipReader.isReadable() )
    {
        m_outPane->appendLine( "ERROR: qZipReader cannot read zip file" );
        return false;
    }

    QList<qZipReader::FileInfo> fileList = zipReader.fileInfoList();
    // m_outPane->appendLine( QString( "Zip contains %1 entries:" ).arg( fileList.count() ) );
    
    int extractedCount = 0;
    int errorCount = 0;
    
    for( int i = 0; i < fileList.count(); ++i )
    {
        const qZipReader::FileInfo& info = fileList[i];
        QString targetPath = extractDir + QDir::separator() + info.filePath;
        
        // m_outPane->appendLine( QString( "  Extracting: %1 (%2 bytes)" )
        //                       .arg( info.filePath ).arg( info.size ) );

        // Create parent directory if needed
        QFileInfo targetInfo( targetPath );
        QDir targetDir = targetInfo.absoluteDir();
        if( !targetDir.exists() )
        {
            if( !targetDir.mkpath( "." ) )
            {
                m_outPane->appendLine( QString( "    ERROR: cannot create dir: %1" ).arg( targetDir.absolutePath() ) );
                errorCount++;
                continue;
            }
        }

        // Skip directories (they end with /)
        if( info.isDir )
        {
            m_outPane->appendLine( QString( "    (directory, skipped)" ) );
            continue;
        }

        // Extract file
        QByteArray fileData = zipReader.fileData( info.filePath );
        if( fileData.isEmpty() && info.size > 0 )
        {
            m_outPane->appendLine( QString( "    ERROR: fileData() returned empty (%1 bytes expected)" )
                                  .arg( info.size ) );
            errorCount++;
            continue;
        }

        QFile outFile( targetPath );
        if( !outFile.open( QIODevice::WriteOnly ) )
        {
            m_outPane->appendLine( QString( "    ERROR: cannot open for writing" ) );
            errorCount++;
            continue;
        }

        int written = outFile.write( fileData );
        outFile.close();

        if( written != fileData.size() )
        {
            m_outPane->appendLine( QString( "    ERROR: wrote %1 of %2 bytes" )
                                  .arg( written ).arg( fileData.size() ) );
            errorCount++;
        }
        else
        {
            // m_outPane->appendLine( QString( "    OK" ) );
            extractedCount++;
        }
    }

    // m_outPane->appendLine( QString( "Extraction complete: %1 OK, %2 errors" )
    //                       .arg( extractedCount ).arg( errorCount ) );
    
    if( errorCount == 0 && extractedCount > 0 )
    {
        m_outPane->appendLine( "Successfully received all build files!" );
        return true;
    }

    return false;
}
#endif

#include "moc_compiler.cpp"
