/***************************************************************************
 *   Copyright (C) 2018 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#include <QAudioDeviceInfo>
#else
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#endif
#include <QAudioFormat>

#include "e-resistor.h"
#include "component.h"

class LibraryItem;

class AudioOut : public Component, public eResistor
{
    public:
        AudioOut( QString type, QString id );
        ~AudioOut();
    
 static Component* construct( QString type, QString id );
 static LibraryItem* libraryItem();

        virtual void initialize() override;
        virtual void stamp() override;
        virtual void updateStep() override;
        virtual void runEvent() override;

        double impedance() { return m_impedance; }
        void setImpedance( double i );

        bool buzzer() { return m_buzzer; }
        void setBuzzer( bool b );

        double frequency() { return m_frequency; }
        void setFrequency( double f ) { m_frequency = f; }
        
        virtual QPainterPath shape() const override;
        virtual void paint( QPainter* p, const QStyleOptionGraphicsItem* o, QWidget* w ) override;

    protected:
        virtual void slotProperties() override;
        void updtProperties();

    private:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QAudioDeviceInfo m_deviceinfo;
        QAudioOutput*    m_audioOutput;
#else
        QAudioDevice     m_deviceinfo;
        QAudioSink*      m_audioOutput;
#endif
        QAudioFormat     m_format;

        QIODevice*    m_audioBuffer;
        QByteArray    m_dataBuffer;

        double m_frequency;
        double m_impedance;

        int m_dataSize;
        int m_dataCount;

        bool m_started;
        bool m_buzzer;
};
