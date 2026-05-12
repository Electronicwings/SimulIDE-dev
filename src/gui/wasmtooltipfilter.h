#ifndef WASMTOOLTIPFILTER_H
#define WASMTOOLTIPFILTER_H

#ifdef __EMSCRIPTEN__

#include <QObject>
#include <QPointer>
#include <QTimer>

class QLabel;
class QWidget;

class WasmTooltipFilter : public QObject
{
    public:
        WasmTooltipFilter( QObject* parent = nullptr );
        ~WasmTooltipFilter() override;

    protected:
        bool eventFilter( QObject* obj, QEvent* ev ) override;

    private:
        void ensureTip( QWidget* topLevel );
        void showTip( QWidget* target, const QPoint& globalPos, const QString& text );
        void hideTip();

        QLabel*             m_tip;
        QTimer              m_hideTimer;
        QPointer<QWidget>   m_currentTarget;
};

#endif // __EMSCRIPTEN__
#endif // WASMTOOLTIPFILTER_H
