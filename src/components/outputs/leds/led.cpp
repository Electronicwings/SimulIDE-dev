/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QPainter>
#include <QMenu>

#include "led.h"
#include "pin.h"
#include "itemlibrary.h"

#include "stringprop.h"

#define tr(str) simulideTr("Led",str)

Component* Led::construct( QString type, QString id )
{ return new Led( type, id ); }

LibraryItem* Led::libraryItem()
{
    return new LibraryItem(
        tr("Led"),
        "Leds",
        "led.png",
        "Led",
        Led::construct);
}

Led::Led( QString type, QString id )
   : LedBase( type, id )
{
    m_area = QRect(-8, -10, 20, 20 );

    m_isLinker = true;

    // Both leads emerge from the flat back of the LED body. Pin's setPos
    // is the OUTER tip (where wires attach); the visible lead extends
    // back toward the body for `length` pixels. So with the body's flat
    // edge at x = -7, we want tip_x + length = -7.
    //   anode  (pin 0): length 13, tip at -20  → lead from -20 to -7
    //   cathode (pin 1): length  9, tip at -16  → lead from -16 to -7
    // Anode tip sits 4 px further out than cathode → anode lead reads
    // 4 px longer (matches the real-LED "longer-lead anode" convention).
    // Pin ids stay "lPin"/"rPin" so saved .sim2 files still find them.
    m_pin.resize( 2 );
    m_pin[0] = new Pin( 180, QPoint(-20, -4 ), m_id+"-lPin", 0, this);
    m_pin[1] = new Pin( 180, QPoint(-16,  4 ), m_id+"-rPin", 1, this);
    m_pin[0]->setLength( 13 ); // anode (longer in real LEDs)
    m_pin[1]->setLength(  9 ); // cathode

    setEpin( 0, m_pin[0] );
    setEpin( 1, m_pin[1] );

    addPropGroup( { "Hidden", {
        new StrProp<Led>("Links", "Links",""
                        , this, &Led::getLinks , &Led::setLinks )
    }, groupHidden} );

    // Keep default in vertical shape
    rotateCCW();
}
Led::~Led(){}

void Led::voltChanged()
{
    eLed::voltChanged();
    if( !m_converged ) return;

    for( Component* comp : m_linkedComp ) comp->setLinkedValue( m_current );
}

void Led::contextMenuEvent( QGraphicsSceneContextMenuEvent* event )
{
    if( !acceptedMouseButtons() ) { event->ignore(); return; }

    event->accept();
    QMenu* menu = new QMenu();

    if( !parentItem() )
    {
        QAction* linkCompAction = menu->addAction( QIcon(":/subcl.png"),tr("Link to Component") );
        QObject::connect( linkCompAction, &QAction::triggered, [=](){ slotLinkComp(); } );

        menu->addSeparator();
    }
    Component::contextMenu( event, menu );
    menu->deleteLater();
}

void Led::drawBackground( QPainter* p )
{
    // Same bullet/stadium silhouette used by SingleGroundedLed and any
    // other LedBase subclass — see LedBase::ledShape for the shape.
    p->drawPath( LedBase::ledShape( QRectF(-7, -7, 18, 14) ) );
}

void Led::drawForeground( QPainter* /*p*/ )
{
    // Intentionally empty — the bullet body + halo from LedBase is the full
    // LED appearance. The old diode-arrow schematic overlay was sized for
    // the legacy circle body and is no longer relevant.
}
