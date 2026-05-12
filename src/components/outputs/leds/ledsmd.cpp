/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QPainter>

#include "ledsmd.h"
#include "e-pin.h"
#include "label.h"

LedSmd::LedSmd( QString type, QString id, QRectF area, ePin* pin0, ePin* pin1 )
      : LedBase( type, id )
{
    m_area = area;
    m_valLabel->setEnabled( false );

    if( !pin0 ) pin0 = new ePin( m_elmId+"-ePin"+QString::number(0), 0 );
    if( !pin1 ) pin1 = new ePin( m_elmId+"-ePin"+QString::number(1), 1 );
    setEpin( 0, pin0 );
    setEpin( 1, pin1 );
}
LedSmd::~LedSmd(){}

void LedSmd::drawBackground( QPainter* p )
{
    // Body shape — gets filled with the radial gradient brush LedBase::paint
    // sets up before calling us.
    p->drawRoundedRect( m_area, 0, 0);
}

void LedSmd::drawForeground( QPainter* /*p*/ )
{
    // Intentionally empty: redrawing the same rect with the flat foreColor
    // brush would overwrite the gradient body painted in drawBackground.
}
