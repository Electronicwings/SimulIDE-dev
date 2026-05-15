/***************************************************************************
 *   Copyright (C) 2024 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QPainter>
#include <QPixmap>

#include "treeitem.h"
#include "mainwindow.h"

TreeItem::TreeItem( TreeItem* parent, QString name, QString nameTr, QString compType, treItemType_t itemType, const QIcon &icon, bool custom  )
        : QTreeWidgetItem()
{
    m_parent   = parent;
    m_name     = name;
    m_nameTr   = nameTr;
    m_compType = compType;
    m_isCustom = custom;
    m_itemType = type_NONE;

    m_shortcut = "";
    m_expanded = false;
    //m_hidden   = false;

    setIcon( 0, icon );
    setItemType( itemType );
}
TreeItem::~TreeItem(){}

void TreeItem::setItemType( treItemType_t itemType )
{
    if( m_itemType == itemType ) return;
    m_itemType = itemType;

    float scale = MainWindow::self()->fontScale();
    QFont font;
    font.setFamily( MainWindow::self()->defaultFontName() );
    font.setBold( true );

    if( itemType == component )
    {
        setFlags( QFlag( Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled ) );

        if( icon( 0 ).isNull() ) setSizeHint( 0, QSize( 100, 30*scale ) );

        if( m_isCustom ) setForeground( 0, QColor( 80, 90, 110 ) );
        else             setForeground( 0, QColor( 100, 90, 60 ) );

        font.setPixelSize( 13*scale );
    }
    else   // Is Category
    {
        setChildIndicatorPolicy( TreeItem::ShowIndicator );
        setFlags( QFlag( Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsDragEnabled ) );

        if( itemType == categ_MAIN )
        {
            if( m_isCustom ) setForeground( 0, QColor( 50, 60, 80 ) );
            else             setForeground( 0, QColor( 75, 70, 10 ) );
            setSizeHint( 0, QSize(100, 42*scale) );
            font.setPixelSize( 15*scale );
            // scaleIcon( 28*scale );
        }
        else if( itemType == categ_CHILD )
        {
            if( m_isCustom ){
                setForeground( 0, QColor( 70, 80, 100 ) );
                // setBackground( 0, QBrush(QColor( 230, 245, 250)) );
            }else{
                setForeground( 0, QColor( 90, 80, 50 ) );
                // setBackground( 0, QBrush(QColor( 230, 250, 245)) );
            }
            if( icon( 0 ).isNull() ) setSizeHint( 0, QSize(100, 30*scale) );
            else                     setSizeHint( 0, QSize(100, 36*scale) );
            font.setPixelSize( 14*scale );
            // scaleIcon( 26*scale );
        }
    }
    setFont( 0, font );
}

void TreeItem::scaleIcon( int targetH )
{
    // Re-render the icon onto a fresh pixmap at targetH px tall, so category
    // rows can show a larger icon than the view's iconSize would normally
    // produce from a small source pixmap. QIcon::pixmap() upsamples the best
    // available source for us; we then store the result back as the icon.
    QIcon src = icon( 0 );
    if( src.isNull() ) return;

    QSize avail = src.actualSize( QSize( targetH * 4, targetH * 4 ) );
    if( avail.isEmpty() ) return;

    qreal ratio = (qreal)avail.width() / (qreal)avail.height();
    int targetW = qRound( targetH * ratio );

    QPixmap pm( targetW, targetH );
    pm.fill( Qt::transparent );
    QPainter p( &pm );
    p.setRenderHint( QPainter::SmoothPixmapTransform, true );
    src.paint( &p, QRect( 0, 0, targetW, targetH ) );
    p.end();

    setIcon( 0, QIcon( pm ) );
}

void TreeItem::setItemExpanded( bool e )
{
    m_expanded = e;
    setExpanded( e );
}

//void TreeItem::setItemHidden( bool h )
//{
//    m_hidden = h;
//    setHidden( h );
//}

QString TreeItem::toString( QString indent )
{
    QString catStr;

    if( m_itemType > component ) catStr = indent+"<category ";
    else                         catStr = indent+"<component ";
    catStr += "name=\""    +m_name+"\" ";
    //catStr += "nametr=\""  +m_nameTr+"\" ";
    //catStr += "comptype=\""+m_compType+"\" ";
    catStr += "hidden=\""  +QString::number( isHidden() ? 1 : 0 )+"\" ";

    if( m_itemType > component )
        catStr += "expanded=\""+QString::number( isExpanded() ? 1 : 0 )+"\" > \n";
    else catStr += "shortcut=\""+m_shortcut+"\" />\n";

    for( int i=0; i<childCount(); ++i )
    {
        TreeItem* childItem = (TreeItem*)child( i );
        catStr += childItem->toString( indent+"  " );
    }
    if( m_itemType > component ) catStr += indent+"</category>\n";

    return catStr;
}
