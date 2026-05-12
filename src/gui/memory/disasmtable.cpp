/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QHeaderView>
#include <QTableWidgetItem>
#include <QFont>
#include <QColor>
#include <QFileInfo>

#include "disasmtable.h"
#include "mainwindow.h"
#include "utils.h"

DisAsmTable::DisAsmTable( QWidget* parent )
           : QTableWidget( parent )
           , m_highlightedRow( -1 )
{
    setColumnCount( 3 );
    setHorizontalHeaderLabels( QStringList()
                               << tr("PC")
                               << tr("Instruction")
                               << tr("Source") );

    verticalHeader()->setVisible( false );
    setSelectionBehavior( QAbstractItemView::SelectRows );
    setSelectionMode( QAbstractItemView::SingleSelection );
    setEditTriggers( QAbstractItemView::NoEditTriggers );
    setShowGrid( false );
    setAlternatingRowColors( true );

    const float scale = MainWindow::self()->fontScale();
    QFont mono;
    mono.setFamily( "Ubuntu Mono" );
    mono.setPixelSize( static_cast<int>(12.5 * scale) );
    setFont( mono );

    setColumnWidth( 0, static_cast<int>( 70 * scale ) );  // PC
    setColumnWidth( 1, static_cast<int>( 220 * scale ) ); // Instruction
    setColumnWidth( 2, static_cast<int>( 240 * scale ) ); // Source

    setMinimumSize( 500, 300 );
    verticalHeader()->setDefaultSectionSize( static_cast<int>( 28 * scale ) );

    connect( this, &QTableWidget::cellActivated,
             this, &DisAsmTable::onCellActivated );
}

void DisAsmTable::setRows( const QVector<eMcu::DisAsmRow>& rows )
{
    setRowCount( 0 );
    m_pcIndex.clear();
    m_rowSrc.clear();
    m_rowSrc.reserve( rows.size() );
    m_highlightedRow = -1;

    setRowCount( rows.size() );
    int row = 0;
    for( const eMcu::DisAsmRow& r : rows )
    {
        QTableWidgetItem* pcItem = new QTableWidgetItem(
            "0x" + QString::number( r.pc, 16 ).rightJustified( 4, '0' ) );
        pcItem->setForeground( QColor( 0x202090 ) );
        pcItem->setTextAlignment( Qt::AlignCenter );
        setItem( row, 0, pcItem );

        QTableWidgetItem* insItem = new QTableWidgetItem( r.text.trimmed() );
        setItem( row, 1, insItem );

        QString src;
        if( r.line > 0 )
            src = QFileInfo( r.file ).fileName() + ":" + QString::number( r.line );
        QTableWidgetItem* srcItem = new QTableWidgetItem( src );
        srcItem->setForeground( QColor( 0x808080 ) );
        setItem( row, 2, srcItem );

        m_pcIndex.insert( r.pc, row );
        m_rowSrc.append( { r.file, r.line } );
        ++row;
    }
    fitColumnsToHeaders( this );
}

void DisAsmTable::highlightPc( uint32_t pc )
{
    if( m_highlightedRow >= 0 && m_highlightedRow < rowCount() )
    {
        for( int c = 0; c < columnCount(); ++c ){
            QTableWidgetItem* it = item( m_highlightedRow, c );
            if( it ) it->setBackground( QBrush() );
        }
    }
    m_highlightedRow = -1;

    auto found = m_pcIndex.constFind( pc );
    if( found == m_pcIndex.constEnd() ) return;

    m_highlightedRow = found.value();
    const QColor hi( 255, 240, 160 ); // soft yellow
    for( int c = 0; c < columnCount(); ++c ){
        QTableWidgetItem* it = item( m_highlightedRow, c );
        if( it ) it->setBackground( hi );
    }
    scrollToItem( item( m_highlightedRow, 0 ),
                  QAbstractItemView::PositionAtCenter );
}

void DisAsmTable::onCellActivated( int row, int /*col*/ )
{
    if( row < 0 || row >= m_rowSrc.size() ) return;
    const RowSrc& s = m_rowSrc.at( row );
    if( s.line > 0 && !s.file.isEmpty() )
        emit sourceLineClicked( s.file, s.line );
}
