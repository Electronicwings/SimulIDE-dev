/***************************************************************************
 *   Copyright (C) 2012 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QtMath>
#include <QList>

class QDomDocument;
class QByteArray;
class QString;
class QPointF;
class QPoint;
class Pin;
class QTableWidget;

#define unitToVal( val, mult ) \
    if     ( mult == " n" ) val *= 1e3; \
    else if( mult == " u")  val *= 1e6; \
    else if( mult == " m" ) val *= 1e9; \
    else if( mult == " ")   val *= 1e12;

#define valToUnit( val, mult, decimals ) \
    mult = " p";\
    if( qFabs( val ) > 999 ) { \
        val /= 1e3; mult = " n"; \
        if( qFabs( val ) > 999 ) { \
            val /= 1e3; mult = " u"; \
            if( qFabs( val ) > 999 ) { \
                val /= 1e3; mult = " m"; \
                if( qFabs( val ) > 999 ) { \
                    val /= 1e3; mult = " "; \
                    if( qFabs( val ) > 999 ) { \
                        val /= 1e3; mult = " k"; \
                        if( qFabs( val ) > 999 ) { \
                            val /= 1e3; mult = " M"; \
                            if( qFabs( val ) > 999 ) { \
                                val /= 1e3; mult = " G"; \
    }}}}}}} \
    if     ( qFabs( val ) < 10)   decimals = 3; \
    else if( qFabs( val ) < 100)  decimals = 2; \
    else if( qFabs( val ) < 1000) decimals = 1;

double getMultiplier( QString mult );
QString multToValStr( double value, QString mult );

QString toHex32( uint32_t d );
QString val2hex( int d );
QString toDigit( int d );
QString decToBase( int value, int base, int digits );

//---------------------------------------------------

void MessageBoxNB( QString title, QString message );

//---------------------------------------------------

QString addQuotes( QString string );
QString remQuotes( QString string );
QString getBareName( QString filepath );
QString getFileName( QString filepath );
QString getFileDir( QString filepath );
QString getFileExt( QString filepath );
QString changeExt( QString filepath, QString ext );

QString getDirDialog( QString msg, QString oldPath );

QString findFile( QString dir, QString fileName );

//---------------------------------------------------

QDomDocument fileToDomDoc( QString fileName, QString caller );
QString      fileToString( QString fileName, QString caller );
QStringList  fileToStringList( QString fileName, QString caller );
QByteArray   fileToByteArray( QString fileName, QString caller );

//---------------------------------------------------

int roundDown( int x, int roundness );
int snapToGrid4( int x );
int snapToGrid8( int x );
QPointF toGrid( QPointF point );
QPoint  toGrid( QPoint point );
QPointF toCompGrid( QPointF point );

bool lessPinX( Pin* pinA, Pin* pinB );
bool lessPinY( Pin* pinA, Pin* pinB );

// Expand each column of a QTableWidget so its header label fits without
// truncation. Only widens — columns already wider than the header text
// keep their existing width. Cheap (O(columns)); does not scan cells, so
// safe to use on large memory tables.
void fitColumnsToHeaders( QTableWidget* table, int extraPadding = 22 );

// Set the QTableWidget's vertical-header width to fit the widest row-
// header label. For row-titled tables (e.g. STATUS / PC display widgets)
// where the row label is the table's identity. O(rows); only call once
// during setup, not per redraw.
void fitVerticalHeaderWidth( QTableWidget* table, int extraPadding = 14 );

//QPointF getPointF( QString p );
//QString getStrPointF( QPointF p );

//---------------------------------------------------

template <typename T>
QList<T> substract( QList<T> &l0, QList<T> &l1 ) // returns l0-l1
{
    QList<T> list;
    for( T el : l0 ) if( !l1.contains( el ) ) list.append( el );
    return list;
}
