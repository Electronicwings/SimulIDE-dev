/***************************************************************************
 *   Copyright (C) 2026 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include "ledbase.h"

class LibraryItem;

// A single LED with its cathode permanently tied to the static ground
// eNode. Only the anode pin is exposed on the schematic, so a logic line
// can be probed by attaching a single wire instead of also routing a
// ground return. Behaves as `Led` with `Grounded = true` locked on.
class SingleGroundedLed : public LedBase
{
    public:
        SingleGroundedLed( QString type, QString id );
        ~SingleGroundedLed();

 static Component*   construct( QString type, QString id );
 static LibraryItem* libraryItem();

    protected:
        void drawBackground( QPainter* p ) override;
        void drawForeground( QPainter* p ) override;
};
