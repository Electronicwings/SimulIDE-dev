/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QString>
#include <QProcess>

#include "compbase.h"

class OutPanelText;
class CodeEditor;

class Compiler : public QObject, public CompBase
{
    Q_OBJECT

    public:
        Compiler( CodeEditor* editor, OutPanelText* outPane );
        ~Compiler();

        QString compName() { return m_compName; }
        void setCompName( QString ) {;}

        virtual QString toolPath() { return m_toolPath; }
        virtual void setToolPath( QString path );

        QString includePath() { return m_inclPath; }
        void setIncludePath( QString path );

        QString extraArgs() { return m_extraArgs; }
        void setextraArgs( QString s ) { m_extraArgs = s; }

        QString family() { return m_family; }
        void setFamily( QString f ) { m_family = f; }

        QString device() { return m_device; }
        void setDevice( QString d ) { m_device = d; }

        // Board variant (e.g. "Uno", "Nano"). Empty for raw MCUs. Sent
        // to the remote build endpoint as X-Board-Name so the server
        // can pick the toolchain from the {device, board} pair.
        QString boardName() { return m_boardName; }
        void setBoardName( QString b ) { m_boardName = b; }

        QString fileName() { return m_fileName; }
        QString buildPath() { return m_buildPath; }
        QString file() { return m_file ; }

        void clearCompiler();
        void loadCompiler( QString file );
        virtual int compile( bool debug );
        // Default upload is a no-op; BaseDebugger overrides to load the
        // generated hex onto the MCU. Declared here so the async build-
        // finished lambda in remoteCompile() can virtually dispatch to
        // chain the upload after a fresh hex is written.
        virtual bool upload() { return false; }

#ifdef __EMSCRIPTEN__
        // WASM build path. Public so CodeEditor can bypass the virtual
        // compile() chain (per-language overrides like InoDebugger don't go
        // through the base implementation).
        int remoteCompile( bool debug );

        // Invoked by the static emscripten_fetch callbacks once the build
        // server reply lands. Public so a free C function can call back
        // into the instance via QPointer<Compiler>; not part of the normal
        // API surface.
        void handleRemoteBuildResponse( const QByteArray& body,
                                        const QString&    error,
                                        const QString&    tmpHex, 
                                        const QString&    tmpZippedBuild);
#endif

        virtual void compilerProps();

        bool isProjectFile( QString file ) { return m_fileList.contains( file ); }

        void readSettings();

        bool checkCommand( QString c );

        OutPanelText* outPane() { return m_outPane; }

    protected:
        void addFilePropHead();

        virtual void preProcess(){;}
        virtual bool postProcess(){return false;}

        int getErrors();
        virtual int getErrorLine( QString txt );
        int getFirstNumber( QString txt );
        void compiled( QString firmware );

        int runBuildStep( QString fullCommand );
        QString replaceData( QString str );
        void toolChainNotFound();
#ifdef __EMSCRIPTEN__
        void downloadZippedBuild( const QString& downloadUrl, const QString& tmpZipPath );
        bool extractZippedBuild( const QString& zipPath, const QString extractPath = "" );
#endif

        CodeEditor* m_editor;

        bool m_uploadHex;
        bool m_fileProps;

        QString m_compName;
        QString m_toolPath;
        QString m_inclPath;
        QString m_buildPath;
        QString m_extraArgs;
        QStringList m_command;
        QStringList m_arguments;
        QStringList m_argsDebug;
        QStringList m_fileList;

        QString m_compilerType;
        QString m_family;
        QString m_device;
        QString m_boardName;
        QString m_firmware;
        QString m_file;
        QString m_fileDir;
        QString m_fileName;
        QString m_fileExt;

        QProcess m_compProcess;

        OutPanelText* m_outPane;

#ifdef __EMSCRIPTEN__
        // Async remoteCompile guard. Set when a build request is in flight;
        // a second build click checks this and short-circuits instead of
        // entering a nested QEventLoop (which corrupts ASYNCIFY state and
        // crashes the wasm module with "memory access out of bounds").
        bool m_remoteBusy = false;

        // One-shot: when true, the async build-finished lambda calls
        // upload() after writing the fresh hex. Used by EditorWindow::
        // uploadFirmware() to chain compile→upload correctly on WASM,
        // since compile() now returns before the hex is on disk.
        bool m_uploadAfterBuild = false;
    public:
        void setUploadAfterBuild( bool v ) { m_uploadAfterBuild = v; }
#endif
};
