/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include "cdebugger.h"

class AvrGccDebugger : public cDebugger
{
    public:
        AvrGccDebugger( CodeEditor* parent, OutPanelText* outPane );
        ~AvrGccDebugger();

    protected:
        virtual bool postProcess() override;

        bool getVariables();
        bool getFunctions();
        bool mapFlashToSource();

#ifdef SIM_HAS_LIBBFD
        // libbfd-based equivalents used in the wasm build, where QProcess
        // can't spawn avr-objdump / readelf / addr2line. Same outcome, all
        // done in-process.
        bool getVariablesBfd();
        bool getFunctionsBfd();
        bool mapFlashToSourceBfd();
        bool disassembleAllBfd();   // uses libopcodes' AVR disassembler;
                                    // pushes rows into eMcu::setDisAsm.
#endif

        int m_addrBytes;

        QString m_elfPath;
};
