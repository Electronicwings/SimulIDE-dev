/***************************************************************************
 *   Copyright (C) 2010 by Santiago González                               *
 *                                                                         *
 ***( see copyright.txt file at root folder )*******************************/

#pragma once

#include <QGraphicsView>

class Component;
class Circuit;
class Mcu;
class SimuProp;
class QPlainTextEdit;

class CircuitView : public QGraphicsView
{
    public:
        CircuitView( QWidget *parent );
        ~CircuitView();

 static CircuitView* self() { return m_pSelf; }
 
        void clear();

        bool showScroll() { return m_showScroll; }
        void setShowScroll( bool show );

        QRectF selectedRect();

        void overrideCursor( const QCursor &cursor );

        qreal getScale() { return m_scale; }

    public slots:
        void saveImage();
        void slotPaste();
        void importCirc();
        void zoomToFit();
        void zoomSelected();
        void zoomOne();
        
    protected:
        void wheelEvent( QWheelEvent* event ) override;
        void dragMoveEvent( QDragMoveEvent* event ) override;
        void dragEnterEvent( QDragEnterEvent* event ) override;
        void dragLeaveEvent( QDragLeaveEvent* event ) override;
        void dropEvent( QDropEvent* event ) override;

    public:
        // Open (or focus) an editor tab keyed to the MCU's id with the
        // right extension/template, seed device/board on the editor's
        // compiler, and reveal the editor pane. Static so non-CircuitView
        // callers (Circuit::loadCircuit / Circuit::paste) can reuse it.
        static void openEditorForMcu( Mcu* mcu, const QString& boardName );

    private:
        void mousePressEvent( QMouseEvent* event ) override;
        void mouseMoveEvent( QMouseEvent *event ) override;
        void mouseReleaseEvent( QMouseEvent* event ) override;
        void contextMenuEvent( QContextMenuEvent* event ) override;
        void drawBackground( QPainter* painter, const QRectF &rect ) override;

    private:
 static CircuitView*  m_pSelf;

        bool m_showScroll;

        qreal m_scale;
        QString m_help;
 
        Component* m_enterItem;
        Circuit*   m_circuit;

        QPointF m_eventpoint;

        QPoint m_mousePressPos;
        bool m_waitForDragStart;
};
