/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QTableWidget>
#include <QVector>

#include "e_mcu.h"   // for eMcu::DisAsmRow

// Read-only table that shows the AVR disassembly produced by
// AvrGccDebugger via libopcodes. One row per instruction:
//   PC (hex)  |  Instruction  |  Source (file:line)
// Emits sourceLineClicked when the user activates a row that has a
// known source correspondence — wired in MCUMonitor to scroll the
// editor.
class DisAsmTable : public QTableWidget
{
    Q_OBJECT
    public:
        DisAsmTable( QWidget* parent = nullptr );

        // Wipe the table and populate from the supplied row list.
        // Builds the address-to-row index used by highlightPc.
        void setRows( const QVector<eMcu::DisAsmRow>& rows );

        // Highlight (and scroll to) the row whose PC matches the given
        // flash byte address. No-op if no row matches.
        void highlightPc( uint32_t pc );

    signals:
        // Fired when the user double-clicks a row that has a known
        // source line. EditorWindow can open the file + scroll.
        void sourceLineClicked( QString file, int line );

    private slots:
        void onCellActivated( int row, int col );

    private:
        // Reverse lookup: PC (byte addr) → row index.
        QHash<uint32_t, int> m_pcIndex;

        // Per-row source info, kept alongside the table so click handler
        // can emit without re-parsing cell text.
        struct RowSrc { QString file; int line; };
        QVector<RowSrc> m_rowSrc;

        int m_highlightedRow;
};
