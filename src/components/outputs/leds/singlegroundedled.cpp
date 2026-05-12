/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QPainter>

#include "singlegroundedled.h"
#include "pin.h"
#include "itemlibrary.h"

#define tr(str) simulideTr("SingleGroundedLed",str)

Component* SingleGroundedLed::construct( QString type, QString id )
{ return new SingleGroundedLed( type, id ); }

LibraryItem* SingleGroundedLed::libraryItem()
{
    return new LibraryItem(
        tr("Grounded Led"),
        "Leds",
        "led.png",
        "GroundedLed",
        SingleGroundedLed::construct);
}

SingleGroundedLed::SingleGroundedLed( QString type, QString id )
                 : LedBase( type, id )
{
    // Match the visible body. LedBase::boundingRect pads m_area outward
    // by kHaloPad on every side so the halo has room to fade.
    m_area = QRectF(-8, -8, 16, 16 );

    // setPos is the OUTER pin tip (wire-attach point); the lead extends
    // INWARD by `length` pixels. With body's flat edge at x = -7, an
    // anode tip at x = -20 plus length 13 puts the lead end exactly at
    // the body's flat back. Matches the longer-anode convention used by
    // Led so the two components share the same visual identity.
    m_pin.resize( 2 );
    m_pin[0] = new Pin( 180, QPoint(-20, 0 ), m_id+"-aPin", 0, this );
    m_pin[1] = new Pin(   0, QPoint( 16, 0 ), m_id+"-kPin", 1, this );
    m_pin[0]->setLength( 13 );

    setEpin( 0, m_pin[0] );
    setEpin( 1, m_pin[1] );

    // Lock the cathode to the simulator's static ground eNode. The actual
    // wiring happens in LedBase::initialize() based on m_grounded.
    m_grounded = true;
    m_pin[1]->setEnabled( false );
    m_pin[1]->setVisible( false );

    // Keep default in vertical shape
    rotateCCW();
}
SingleGroundedLed::~SingleGroundedLed(){}

// Body shape only — LedBase::paint sets up the radial gradient brush, halo,
// and specular pip around this rect. Use the shared bullet silhouette so
// SingleGroundedLed reads as the same component family as Led.
void SingleGroundedLed::drawBackground( QPainter* p )
{
    p->drawPath( LedBase::ledShape( QRectF(-7, -7, 18, 14) ) );
}

void SingleGroundedLed::drawForeground( QPainter* /*p*/ )
{
    // Intentionally empty: no schematic overlay on top of the glow.
}
