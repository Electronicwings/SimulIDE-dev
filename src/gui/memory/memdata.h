/***************************************************************************
 *   Copyright (C) 2018 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QVector>
#include <functional>

class MemTable;
class eMcu;

class MemData
{
    public:
        MemData();
        ~MemData();

        // On WebAssembly loadData is asynchronous: the return value is always
        // false and the real result is delivered via onDone. On desktop it
        // behaves synchronously and also invokes onDone if supplied.
        static bool loadData( QVector<int>* toData, bool resize=false, int bits=8,
                              std::function<void(bool)> onDone = nullptr );
        static void saveData( QVector<int>* data, int bits=8 );

        static bool loadFile( QVector<int>* toData, QString file, bool resize, int bits, eMcu* eMcu=nullptr );
        static bool loadDat( QVector<int>* toData, QString file, bool resize );
        static bool loadHex( QVector<int>* toData, QString file, bool resize, int bits );
        static bool loadBin( QVector<int>* toData, QString file, bool resize, int bits );

        static QString getMem( QVector<int>* data );
        static void setMem( QVector<int>* data, QString m );

        virtual void showTable( int dataSize=256, int wordBytes=1 );

    protected:
        MemTable* m_memTable;
        static eMcu* m_eMcu;

        static void saveDat( QVector<int>* data, int bits );
        static void saveHex( QVector<int>* data, int bits ); /// TODO
        static void saveBin( QVector<int>* data, int bits );
};
