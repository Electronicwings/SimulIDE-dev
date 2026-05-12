/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include "e-led.h"
#include "component.h"
#include "e-node.h"

class eNode;

class LedBase : public Component, public eLed
{
    public:
        LedBase( QString type, QString id );
        ~LedBase();

        enum ledColor_t {
            yellow = 0,
            red,
            green,
            blue,
            orange,
            purple,
            white
        };

        QString colorStr() { return m_ledColorStr; }
        void setColorStr( QString color );

        bool grounded() { return m_grounded; }
        void setGrounded( bool grounded );

        virtual void initialize() override;
        virtual void updateStep() override;

        // Wider than Component's m_area+2 so the radial halo painted in
        // LedBase::paint can fade out past the LED body without being
        // clipped. Padding (in pixels) is shared across all LED-derived
        // classes; bump kHaloPad if you want even more bloom.
        QRectF boundingRect() const override;

        // Hit-testing shape kept tight to the visible body so the halo's
        // wider boundingRect doesn't swallow clicks in the pin-lead area
        // (which would block users from connecting wires to the lead).
        QPainterPath shape() const override;

        // Stadium / bullet outline of a real LED viewed from the side:
        // rectangular body with one rounded "dome" end (the lens, on the
        // right in local coords) and one flat end (where leads attach).
        // Subclasses call this from drawBackground so Led, SingleGrounded-
        // Led etc. share the same visual identity.
 static QPainterPath ledShape( const QRectF& body );

 static QColor getColor( ledColor_t c, int bright );
 static QString getColorList();

        virtual void paint( QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w ) override;

    protected:
        virtual void drawBackground( QPainter* p )=0;
        virtual void drawForeground( QPainter* p )=0;
        
        bool   m_grounded;

 static int m_overBright;
        
        QString m_ledColorStr;
        ledColor_t m_ledColor;

        static eNode m_gndEnode;
};
