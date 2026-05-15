/***************************************************************************
 *   Copyright (C) 2021 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include "ui_enumval.h"
#include "propval.h"

class Component;
class PropDialog;

class EnumVal : public PropVal, private Ui::EnumVal
{
    Q_OBJECT
    
    public:
        EnumVal( PropDialog* parent, CompBase* comp, ComProperty* prop );
        ~EnumVal();

        virtual void setup( bool isComp ) override;
        virtual void updtValues() override;

        void setEnums( QString e );

    public slots:
        void on_showVal_toggled( bool checked );
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        void on_valueBox_currentIndexChanged( QString val );
#else
        void on_valueBox_currentIndexChanged( int index );
#endif

    protected:
        QStringList m_enums;
};
