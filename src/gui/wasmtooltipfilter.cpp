#ifdef __EMSCRIPTEN__

#include <QApplication>
#include <QEvent>
#include <QHelpEvent>
#include <QLabel>
#include <QWidget>

#include "wasmtooltipfilter.h"

// Qt 5.15 WASM cannot reliably show/hide the QTipLabel popup window
// (top-level popups go through the emscripten platform plugin, which
// tears them down on the first leave/focus event and then refuses to
// recreate them). Instead of using QToolTip we render a plain QLabel
// parented to the main window — same WASM canvas, no popup window.
static constexpr int kTooltipDurationMs = 3000;
static constexpr int kCursorOffsetX     = 14;
static constexpr int kCursorOffsetY     = 18;

WasmTooltipFilter::WasmTooltipFilter( QObject* parent )
    : QObject( parent )
    , m_tip( nullptr )
{
    m_hideTimer.setSingleShot( true );
    QObject::connect( &m_hideTimer, &QTimer::timeout, this, [this](){ hideTip(); });
}

WasmTooltipFilter::~WasmTooltipFilter(){}

void WasmTooltipFilter::ensureTip( QWidget* topLevel )
{
    if( m_tip && m_tip->parent() == topLevel ) return;

    if( m_tip ) m_tip->deleteLater();
    m_tip = new QLabel( topLevel );
    m_tip->setObjectName( "wasmToolTip" );
    m_tip->setAttribute( Qt::WA_TransparentForMouseEvents );
    m_tip->setStyleSheet(
        "QLabel#wasmToolTip {"
        " background-color: #1F2328;"
        " color: #FFFFFF;"
        " padding: 4px 8px;"
        " border: 1px solid #3A434F;"
        " border-radius: 4px;"
        "}" );
    m_tip->hide();
}

void WasmTooltipFilter::showTip( QWidget* target, const QPoint& globalPos, const QString& text )
{
    QWidget* topLevel = target->window();
    if( !topLevel ) topLevel = QApplication::activeWindow();
    if( !topLevel ) return;

    ensureTip( topLevel );
    m_currentTarget = target;

    m_tip->setText( text );
    m_tip->adjustSize();

    QPoint local = topLevel->mapFromGlobal( globalPos );
    int x = local.x() + kCursorOffsetX;
    int y = local.y() + kCursorOffsetY;

    // Keep the tip inside the top-level rect so it isn't clipped off-screen.
    int maxX = topLevel->width()  - m_tip->width()  - 4;
    int maxY = topLevel->height() - m_tip->height() - 4;
    if( x > maxX ) x = maxX;
    if( y > maxY ) y = maxY;
    if( x < 4 ) x = 4;
    if( y < 4 ) y = 4;

    m_tip->move( x, y );
    m_tip->show();
    m_tip->raise();

    m_hideTimer.start( kTooltipDurationMs );
}

void WasmTooltipFilter::hideTip()
{
    if( m_tip ) m_tip->hide();
    m_hideTimer.stop();
    m_currentTarget.clear();
}

bool WasmTooltipFilter::eventFilter( QObject* obj, QEvent* ev )
{
    switch( ev->type() ){
    case QEvent::ToolTip:
    {
        QWidget* w = qobject_cast<QWidget*>( obj );
        if( w ){
            QString tip = w->toolTip();
            if( !tip.isEmpty() ){
                QHelpEvent* he = static_cast<QHelpEvent*>( ev );
                showTip( w, he->globalPos(), tip );
                return true; // suppress Qt's default popup tooltip
            }
        }
        break;
    }
    case QEvent::Leave:
    case QEvent::HoverLeave:
    case QEvent::FocusOut:
    case QEvent::WindowDeactivate:
    {
        if( m_currentTarget && obj == m_currentTarget.data() ) hideTip();
        break;
    }
    case QEvent::MouseButtonPress:
    case QEvent::Wheel:
        hideTip();
        break;
    default: break;
    }
    return QObject::eventFilter( obj, ev );
}

#endif // __EMSCRIPTEN__
