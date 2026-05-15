/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include "avrgccdebugger.h"

/*
    Debugger attached to non-Arduino AVR MCU drops (.c / .cpp). Reuses the
    full AvrGccDebugger pipeline (DWARF variables, function map, flash→source
    mapping, libbfd disassembly on WASM) but adjusts the path conventions to
    match the build zip produced by the server for bare-metal sources:
    <buildPath>/build/<name>.elf  (no .ino suffix, unlike InoDebugger).
*/
class GenericAvrDebugger : public AvrGccDebugger
{
    public:
        GenericAvrDebugger( CodeEditor* parent, OutPanelText* outPane );
        ~GenericAvrDebugger();

    protected:
        virtual bool postProcess() override;
};
