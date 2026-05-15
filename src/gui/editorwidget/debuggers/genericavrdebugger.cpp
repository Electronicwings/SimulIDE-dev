/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include "genericavrdebugger.h"
#include "mainwindow.h"

GenericAvrDebugger::GenericAvrDebugger( CodeEditor* parent, OutPanelText* outPane )
                  : AvrGccDebugger( parent, outPane )
{
#ifdef __EMSCRIPTEN__
    // WASM: the build zip is extracted into the config root, so the .elf
    // lands at <configRoot>/build/<name>.elf — same scheme InoDebugger uses.
    m_buildPath = MainWindow::self()->getConfigPath("");
#else
    m_buildPath = MainWindow::self()->getConfigPath("codeeditor/buildAvr");
#endif
}

GenericAvrDebugger::~GenericAvrDebugger(){}

bool GenericAvrDebugger::postProcess()
{
    QString oldBuildPath = m_buildPath;

    m_buildPath = m_buildPath+"/build/";

    bool ok = AvrGccDebugger::postProcess();

    m_buildPath = oldBuildPath;
    return ok;
}
