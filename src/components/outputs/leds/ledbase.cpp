/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#include <QPainter>
#include <QRadialGradient>

#include "ledbase.h"
#include "circuitwidget.h"
#include "connector.h"
#include "simulator.h"
#include "e-node.h"
#include "pin.h"

#include "doubleprop.h"
#include "stringprop.h"
#include "boolprop.h"

#define tr(str) simulideTr("LedBase",str)

eNode LedBase::m_gndEnode("");
int   LedBase::m_overBright = 0;

// Extra space around m_area for the halo to fade out before being clipped
// by boundingRect. Increasing this makes the bloom extend further past
// the LED body for every LED-derived class at once.
static constexpr qreal kHaloPad = 10.0;

QRectF LedBase::boundingRect() const
{
    return QRectF( m_area.x()      - kHaloPad,
                   m_area.y()      - kHaloPad,
                   m_area.width()  + kHaloPad * 2,
                   m_area.height() + kHaloPad * 2 );
}

QPainterPath LedBase::shape() const
{
    QPainterPath path;
    path.addRect( m_area );
    return path;
}

QPainterPath LedBase::ledShape( const QRectF& body )
{
    // A real LED viewed from the side: rectangular cylindrical body with
    // a hemispherical lens on one end (the dome) and a flat back on the
    // other. Dome faces RIGHT in the local item frame; rotate the
    // component for vertical / other orientations.
    const qreal h = body.height();
    const qreal r = h / 2.0;                  // dome radius = half body height
    const qreal flatR = qMin( 1.5, r * 0.25 );// tiny corner round on the flat side

    QPainterPath path;
    path.moveTo( body.left() + flatR, body.top() );

    // Top edge from flat side to start of dome
    path.lineTo( body.right() - r, body.top() );

    // Dome: 180° arc swept clockwise from top to bottom
    path.arcTo( body.right() - r * 2, body.top(), r * 2, h, 90.0, -180.0 );

    // Bottom edge back to the flat side
    path.lineTo( body.left() + flatR, body.bottom() );

    // Soft corner on the bottom-left of the flat back
    path.arcTo( body.left(), body.bottom() - flatR * 2,
                flatR * 2,  flatR * 2,
                270.0, -90.0 );

    // Left edge up
    path.lineTo( body.left(), body.top() + flatR );

    // Soft corner on the top-left
    path.arcTo( body.left(), body.top(),
                flatR * 2, flatR * 2,
                180.0, -90.0 );

    path.closeSubpath();
    return path;
}
QString LedBase::getColorList() {
    return "Yellow,Red,Green,Blue,Orange,Purple,White;"
           +tr("Yellow")+","+tr("Red")+","+tr("Green")+","+tr("Blue")
          +","+tr("Orange")+","+tr("Purple")+","+tr("White");}

LedBase::LedBase( QString type, QString id )
       : Component( type, id )
       , eLed( id )
{
    m_graphical = true;
    m_grounded  = false;
    m_intensity = 0;

    m_color = QColor( Qt::black );
    setColorStr("Yellow");

    Simulator::self()->addToUpdateList( this );

    addPropGroup( { tr("Main"), {
        new StrProp <LedBase>("Color", tr("Color"), getColorList()
                             , this, &LedBase::colorStr, &LedBase::setColorStr,0,"enum" ),

        new BoolProp<LedBase>("Grounded", tr("Grounded"), ""
                             , this, &LedBase::grounded, &LedBase::setGrounded, propNoCopy ),
    }, 0} );

    addPropGroup( { tr("Electric"), {
        new DoubProp<LedBase>("Threshold", tr("Forward Voltage"), "V"
                             , this, &LedBase::threshold, &LedBase::setThreshold ),

        new DoubProp<LedBase>("MaxCurrent", tr("Max Current"), "mA"
                             , this, &LedBase::maxCurrent, &LedBase::setMaxCurrent ),

        new DoubProp<LedBase>("Resistance", tr("Resistance"), "Ω"
                             , this, &LedBase::resistance, &LedBase::setResistance ),
    }, 0} );
}
LedBase::~LedBase(){}

void LedBase::initialize()
{
    m_crashed = false;
    m_warning = false;

    if( m_grounded ) m_ePin[1]->setEnode( &m_gndEnode );

    eLed::initialize();
    update();
}

void LedBase::updateStep()
{
    uint32_t intensity = m_intensity;
    eLed::updateBright();

    if( overCurrent() > 1.5 )
    {
        m_warning = true;
        m_crashed = overCurrent() > 2;
        update();
    }else{
        if( m_warning ) update();
        m_warning = false;
        m_crashed = false;
    }
    if( intensity != m_intensity ) update();
    if( m_changed )
    {
        m_changed = false;
        voltChanged();
    }
}

void LedBase::setGrounded( bool grounded )
{
    if( grounded == m_grounded ) return;
    m_grounded = grounded;

    if( Simulator::self()->isRunning() )  CircuitWidget::self()->powerCircOff();

    Pin* pin1 = static_cast<Pin*>(m_ePin[1]);
    pin1->setEnabled( !grounded );
    pin1->setVisible( !grounded );
    if( grounded ) pin1->removeConnector();
    else           pin1->setEnode( nullptr );
}

void LedBase::setColorStr( QString color )
{
    m_ledColorStr = color;
    double thr = 2.4;

    if     ( color == "Red"   ) { thr = 1.8; m_ledColor = red; }
    else if( color == "Green" ) { thr = 3.5; m_ledColor = green; }
    else if( color == "Blue"  ) { thr = 3.6; m_ledColor = blue;}
    else if( color == "Orange") { thr = 2.0; m_ledColor = orange; }
    else if( color == "Purple") { thr = 3.5; m_ledColor = purple; }
    else if( color == "White" ) { thr = 4.0; m_ledColor = white; }
    else                        { thr = 2.4; m_ledColor = yellow; }  // Yellow and undefined

    eLed::setThreshold( thr );

    if( m_showVal && (m_showProperty == "Color") )
        setValLabelText( color );

    update(); // paint() reads m_ledColor — force repaint so the swatch
              // changes immediately when the property is edited (sim
              // running or not).
}

QColor LedBase::getColor( ledColor_t c, int bright )
{
    m_overBright = 0;
    int secL = bright/3;
    int secH = bright/2;
    int secX = bright*2/3;
    QColor foreColor;

    if( bright > 255 )
    {
        m_overBright = (bright-255);
        secL += m_overBright;
        bright = 255;
    }
    switch( c ) {
        case yellow: foreColor = QColor( bright, bright, secL ); break;
        case red:    foreColor = QColor( bright, secH  , secH ); break;
        case green:  foreColor = QColor( secL  , bright, secL ); break;
        case blue:   foreColor = QColor( secH  , secH  , bright ); break;
        case orange: foreColor = QColor( bright, secX  , secL ); break;
        case purple: foreColor = QColor( bright, secL  , bright ); break;
        case white:  foreColor = QColor( bright, bright, bright ); break;
    }
    return foreColor;
}
void LedBase::paint( QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w )
{
    Component::paint( p, o, w );
    p->setRenderHint( QPainter::Antialiasing, true );

    const int  rawI = static_cast<int>( m_intensity );
    const bool isOn = rawI > 0;

    // Brightness boost. eLed::updateBright reports m_intensity = sqrt(I/Imax)*255,
    // and getColor() multiplies channels by this — so a typical operating
    // current renders as a muddy half-bright colour. Lift the value passed
    // to getColor so partially-lit LEDs still read as vivid, and keep an
    // off-state value high enough that the configured colour is recognisable
    // even when the simulation isn't running.
    int viz;
    if( m_crashed )      viz = 0;
    else if( isOn )      viz = qMin( 255, 140 + rawI * 2 / 3 );
    else                 viz = 150;  // off-state: clearly identifiable colour
    const QColor base = m_crashed ? QColor(60, 50, 40)
                                  : getColor( m_ledColor, viz );

    const QRectF br     = boundingRect();
    const QPointF cBR   = br.center();
    const QPointF cBody = m_area.center();
    const qreal bodyR   = qMin( m_area.width(), m_area.height() ) * 0.55;

    // ---- Halo: soft radial gradient around the LED -----------------------
    // Sized to fit inside boundingRect (which is m_area + 2px). For LED
    // classes that want a wider halo, set m_area larger than the visible
    // body in the constructor — see SingleGroundedLed.
    if( isOn && !m_crashed )
    {
        // Halo extends well past the visible body. Three-stop gradient:
        // a hot core where the body sits, a softer mid-falloff that
        // bleeds onto the surrounding scene, and full transparency at
        // the outer edge so the halo blends seamlessly.
        const qreal haloR = qMin( br.width(), br.height() ) / 2.0 - 1;
        const qreal coreFrac = qBound( 0.18, bodyR / haloR, 0.40 );

        QColor core  = base; core .setAlpha( qMin( 235, 110 + rawI ) );
        QColor inner = base; inner.setAlpha( qMin( 170, 70  + rawI * 2 / 3 ) );
        QColor mid   = base; mid  .setAlpha( qMin( 90,  25  + rawI / 3 ) );
        QColor edge  = base; edge .setAlpha( 0 );

        QRadialGradient halo( cBR, haloR );
        halo.setColorAt( 0.0,            core );
        halo.setColorAt( coreFrac,       inner );
        halo.setColorAt( coreFrac + 0.28, mid );
        halo.setColorAt( 1.0,            edge );

        p->setPen( Qt::NoPen );
        p->setBrush( halo );
        p->drawEllipse( cBR, haloR, haloR );
    }

    // ---- Body brush: radial gradient with bright centre ------------------
    // The pen+brush set here is consumed by the subclass's drawBackground
    // (which paints its own shape — circle, rounded rect, etc.).
    QRadialGradient body( cBody + QPointF(-1, -1), bodyR );
    if( m_crashed ){
        body.setColorAt( 0.0, QColor(60, 50, 40) );
        body.setColorAt( 1.0, QColor(20, 18, 15) );
    }
    else if( isOn ){
        QColor hot(
            qMin( 255, base.red()   + 110 ),
            qMin( 255, base.green() + 110 ),
            qMin( 255, base.blue()  + 110 ) );
        body.setColorAt( 0.0, hot );
        body.setColorAt( 0.5, base );
        body.setColorAt( 1.0, base.darker( 145 ) );
    }
    else { // off — use the configured colour at moderate brightness so the
           // LED is clearly identifiable even when the sim isn't running.
           // The hot white-ish centre is reserved for the on state, so a
           // running LED is still visually distinct from a stopped one.
        body.setColorAt( 0.0, base.lighter( 125 ) );
        body.setColorAt( 0.6, base );
        body.setColorAt( 1.0, base.darker( 130 ) );
    }
    p->setBrush( body );
    p->setPen( QPen( QColor(0, 0, 0, 180), 0.8 ) );
    drawBackground( p );

    // ---- Foreground: subclass schematic overlay (e.g. Led's diode mark) --
    // Keep a slightly softened pen so overlays sit on top of the glow
    // without screaming. Skip for crashed (the burnt body says enough).
    if( !m_crashed )
    {
        QColor overlay = base.darker( 130 );
        p->setPen( QPen( overlay, 1 ) );
        p->setBrush( base );
        drawForeground( p );
    }

    // ---- Specular pip: tiny white highlight when the LED is on -----------
    if( isOn && !m_crashed )
    {
        const qreal pipR  = bodyR * 0.25;
        const QPointF pip = cBody + QPointF( -bodyR * 0.4, -bodyR * 0.4 );
        QRadialGradient pipG( pip, pipR );
        pipG.setColorAt( 0.0, QColor(255, 255, 255, 200) );
        pipG.setColorAt( 1.0, QColor(255, 255, 255,   0) );
        p->setPen( Qt::NoPen );
        p->setBrush( pipG );
        p->drawEllipse( pip, pipR, pipR );
    }

    Component::paintSelected( p );
}
