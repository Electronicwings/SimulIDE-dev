#ifdef __EMSCRIPTEN__

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QByteArray>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

#include "jsbridge.h"
#include "circuit.h"
#include "circuitwidget.h"
#include "editorwindow.h"
#include "codeeditor.h"
#include "simulator.h"
#include "e-node.h"
#include "component.h"
#include "mcu.h"
#include "meter.h"
#include "probe.h"
#include "freqmeter.h"
#include "mainwindow.h"

// Sources
#include "fixedvolt.h"
#include "battery.h"
#include "rail.h"
#include "varsource.h"
#include "clock.h"
#include "wavegen.h"
#include "csource.h"

// Switches
#include "switch.h"
#include "push.h"
#include "push_base.h"
#include "switchdip.h"
#include "relay.h"

using emscripten::val;

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------

SimulIDEBridge* SimulIDEBridge::instance()
{
    static SimulIDEBridge* s_instance = nullptr;
    if( !s_instance ) s_instance = new SimulIDEBridge();
    return s_instance;
}

SimulIDEBridge::SimulIDEBridge()
    : m_stateCb   ( val::null() )
    , m_errorCb   ( val::null() )
    , m_compiledCb( val::null() )
{}

// ---------------------------------------------------------------------------
// Circuit
// ---------------------------------------------------------------------------

bool SimulIDEBridge::setCircuit( const std::string& sim2Xml )
{
    // The existing Circuit::loadCircuit() reads from a file path. Stage the
    // parent's XML to a known path on MEMFS and reuse the existing loader so
    // we get exactly the same parse path and side effects (recent-files,
    // backup file, etc.). A future cleanup could refactor loadCircuit() to
    // accept a string and skip this step.
    // const QString tmpPath = "/tmp/parent_circuit.sim2";
    const QString tmpPath = MainWindow::self()->getTempPath( "parent_circuit.sim2" );
    QFile f( tmpPath );
    if( !f.open( QIODevice::WriteOnly | QIODevice::Text ) ){
        emitError( std::string("setCircuit: cannot stage XML at ")
                   + tmpPath.toStdString() );
        return false;
    }
    f.write( sim2Xml.c_str(), static_cast<qint64>( sim2Xml.size() ) );
    f.close();

    if( !CircuitWidget::self() ){
        emitError( "setCircuit: CircuitWidget not ready" );
        return false;
    }
    CircuitWidget::self()->loadCirc( tmpPath );
    return true;
}

std::string SimulIDEBridge::getCircuit()
{
    if( !Circuit::self() ) return std::string();
    return Circuit::self()->circuitToString().toStdString();
}

// ---------------------------------------------------------------------------
// Sketch
// ---------------------------------------------------------------------------

bool SimulIDEBridge::setSketch( const std::string& source )
{
    if( !Mcu::self() ){
        emitError( "setSketch: no MCU on circuit" );
        return false;
    }
    EditorWindow* ew = EditorWindow::self();
    if( !ew ){
        emitError( "setSketch: editor not ready" );
        return false;
    }

    // If an editor tab is already open (auto-spawned when the MCU was placed),
    // overwrite its buffer in place. Otherwise stage a file under the MCU's
    // preferred extension and load it via the existing path-based flow so
    // CodeEditor::setFile auto-attaches the right compiler.
    CodeEditor* ce = ew->getCodeEditor();
    if( ce ){
        ce->setPlainText( QString::fromStdString( source ) );
        return true;
    }

    const QString ext  = Mcu::self()->defaultExtension();
    // const QString path = QString( "/tmp/parent_sketch" ) + ext;
    const QString path = MainWindow::self()->getTempPath( "parent_sketch" + ext );
    QFile f( path );
    if( !f.open( QIODevice::WriteOnly | QIODevice::Text ) ){
        emitError( std::string("setSketch: cannot stage source at ")
                   + path.toStdString() );
        return false;
    }
    f.write( source.c_str(), static_cast<qint64>( source.size() ) );
    f.close();
    ew->loadFile( path );
    return true;
}

std::string SimulIDEBridge::getSketch()
{
    EditorWindow* ew = EditorWindow::self();
    if( !ew ) return std::string();
    CodeEditor* ce = ew->getCodeEditor();
    if( !ce ) return std::string();
    return ce->toPlainText().toStdString();
}

// ---------------------------------------------------------------------------
// Build + flash
// ---------------------------------------------------------------------------

void SimulIDEBridge::compileAndFlash()
{
    if( !EditorWindow::self() ){
        emitError( "compileAndFlash: editor not ready" );
        return;
    }
    // Async path: returns immediately, the build-finished lambda inside
    // Compiler::handleRemoteBuildResponse calls emitCompiled() once the
    // server reply lands.
    EditorWindow::self()->upload();
}

// ---------------------------------------------------------------------------
// Simulation lifecycle
// ---------------------------------------------------------------------------

void SimulIDEBridge::startSimulation()
{
    if( CircuitWidget::self() ) CircuitWidget::self()->powerCircOn();
}

void SimulIDEBridge::stopSimulation()
{
    if( CircuitWidget::self() ) CircuitWidget::self()->powerCircOff();
}

void SimulIDEBridge::pauseSimulation()
{
    if( CircuitWidget::self() ) CircuitWidget::self()->pauseCirc();
}

std::string SimulIDEBridge::getSimulationState()
{
    if( !Simulator::self() ) return "stopped";
    switch( Simulator::self()->simState() ){
        case SIM_RUNNING:  return "running";
        case SIM_PAUSED:   return "paused";
        case SIM_DEBUG:    return "debug";
        case SIM_STARTING: return "starting";
        case SIM_WAITING:  return "waiting";
        case SIM_ERROR:    return "error";
        case SIM_STOPPED:
        default:           return "stopped";
    }
}

// ---------------------------------------------------------------------------
// Meter readings
// ---------------------------------------------------------------------------

// Returns true if `c` is one of the recognised meter types and fills
// `value` and `unit` with its current reading. Probe and FreqMeter aren't
// Meter subclasses, so the dispatch is by itemType() string rather than
// dynamic_cast (RTTI is intentionally limited in WASM builds).
static bool readMeterValue( Component* c, double& value, QString& unit )
{
    const QString t = c->itemType();
    if( t == "Voltimeter" || t == "Amperimeter" ){
        Meter* m = static_cast<Meter*>(c);
        value = m->getReading();
        unit  = m->getUnit();
        return true;
    }
    if( t == "Probe" ){
        value = static_cast<Probe*>(c)->getVoltage();
        unit  = "V";
        return true;
    }
    if( t == "FreqMeter" ){
        value = static_cast<FreqMeter*>(c)->getFrequency();
        unit  = "Hz";
        return true;
    }
    return false;
}

static QJsonObject meterDescriptor( Component* c, bool withValue )
{
    double value = 0;
    QString unit;
    bool ok = readMeterValue( c, value, unit );
    if( !ok ) return QJsonObject();

    QJsonObject obj;
    obj["id"]    = c->getUid();
    obj["label"] = c->idLabel();
    obj["type"]  = c->itemType();
    obj["unit"]  = unit;
    if( withValue ) obj["value"] = value;
    return obj;
}

std::string SimulIDEBridge::listMeters()
{
    QJsonArray arr;
    if( Circuit::self() ){
        for( Component* c : *Circuit::self()->compList() ){
            QJsonObject obj = meterDescriptor( c, /*withValue*/ false );
            if( !obj.isEmpty() ) arr.append( obj );
        }
    }
    return QJsonDocument( arr ).toJson( QJsonDocument::Compact ).toStdString();
}

std::string SimulIDEBridge::readMeter( const std::string& id )
{
    if( !Circuit::self() ) return "null";
    Component* c = Circuit::self()->getCompById( QString::fromStdString( id ) );
    if( !c ) return "null";

    QJsonObject obj = meterDescriptor( c, /*withValue*/ true );
    if( obj.isEmpty() ) return "null";
    return QJsonDocument( obj ).toJson( QJsonDocument::Compact ).toStdString();
}

std::string SimulIDEBridge::readAllMeters()
{
    QJsonArray arr;
    if( Circuit::self() ){
        for( Component* c : *Circuit::self()->compList() ){
            QJsonObject obj = meterDescriptor( c, /*withValue*/ true );
            if( !obj.isEmpty() ) arr.append( obj );
        }
    }
    return QJsonDocument( arr ).toJson( QJsonDocument::Compact ).toStdString();
}

// ---------------------------------------------------------------------------
// Node voltages
// ---------------------------------------------------------------------------

std::string SimulIDEBridge::listNodes()
{
    QJsonArray arr;
    if( Simulator::self() ){
        for( eNode* n : *Simulator::self()->getENodeList() ){
            if( !n ) continue;
            QJsonObject obj;
            obj["id"]     = n->itemId();
            obj["number"] = n->getNodeNumber();
            arr.append( obj );
        }
    }
    return QJsonDocument( arr ).toJson( QJsonDocument::Compact ).toStdString();
}

double SimulIDEBridge::getNodeVoltage( const std::string& id )
{
    if( !Simulator::self() ) return std::nan("");
    const QString needle = QString::fromStdString( id );
    for( eNode* n : *Simulator::self()->getENodeList() ){
        if( n && n->itemId() == needle ) return n->getVolt();
    }
    return std::nan("");
}

std::string SimulIDEBridge::readAllNodes()
{
    QJsonArray arr;
    if( Simulator::self() ){
        for( eNode* n : *Simulator::self()->getENodeList() ){
            if( !n ) continue;
            QJsonObject obj;
            obj["id"]      = n->itemId();
            obj["number"]  = n->getNodeNumber();
            obj["voltage"] = n->getVolt();
            arr.append( obj );
        }
    }
    return QJsonDocument( arr ).toJson( QJsonDocument::Compact ).toStdString();
}

// ---------------------------------------------------------------------------
// Sources / switches — controllable property bag
// ---------------------------------------------------------------------------
//
// Dispatch is keyed off itemType() rather than dynamic_cast (RTTI is
// limited in WASM builds and itemType() is the project-wide identity used
// elsewhere on the bridge). Each branch reads/writes the public setters
// already exposed on the component class — see survey notes in the commit
// that introduced this section.

static QString componentKind( const QString& type )
{
    if( type == "Fixed Voltage"  || type == "Battery"        || type == "Rail"
     || type == "Voltage Source" || type == "Current Source" || type == "Clock"
     || type == "WaveGen"        || type == "Csource" )      return "source";
    if( type == "Switch"    || type == "Push"     || type == "SwitchDip"
     || type == "Relay"     || type == "RelaySPST" )         return "switch";
    return QString();
}

static QJsonObject readControllableProps( Component* c )
{
    QJsonObject p;
    const QString t = c->itemType();

    // ---- sources ----
    if( t == "Fixed Voltage" ){
        FixedVolt* x = static_cast<FixedVolt*>(c);
        p["voltage"] = x->volt();
        p["out"]     = x->out();
    }
    else if( t == "Battery" ){
        Battery* x = static_cast<Battery*>(c);
        p["voltage"]    = x->voltage();
        p["resistance"] = x->resistance();
    }
    else if( t == "Rail" ){
        p["voltage"] = static_cast<Rail*>(c)->volt();
    }
    else if( t == "Voltage Source" || t == "Current Source" ){
        VarSource* x = static_cast<VarSource*>(c);
        p["value"]   = x->getVal();
        p["min"]     = x->minValue();
        p["max"]     = x->maxValue();
        p["running"] = x->running();
    }
    else if( t == "Clock" ){
        Clock* x = static_cast<Clock*>(c);
        p["frequency"] = x->freq();
        p["voltage"]   = x->volt();
        p["running"]   = x->running();
        p["alwaysOn"]  = x->alwaysOn();
    }
    else if( t == "WaveGen" ){
        WaveGen* x = static_cast<WaveGen*>(c);
        p["frequency"]  = x->freq();
        p["duty"]       = x->duty();
        p["phaseShift"] = x->phaseShift();
        p["waveType"]   = x->waveType();
        p["semiAmpli"]  = x->semiAmpli();
        p["midVolt"]    = x->midVolt();
        p["bipolar"]    = x->bipolar();
        p["floating"]   = x->floating();
        p["running"]    = x->running();
    }
    else if( t == "Csource" ){
        Csource* x = static_cast<Csource*>(c);
        p["gain"]        = x->gain();
        p["voltage"]     = x->volt();
        p["outCurrent"]  = x->outCurrent();
        p["currSource"]  = x->currSource();
        p["currControl"] = x->currControl();
    }
    // ---- switches ----
    else if( t == "Switch" ){
        Switch* x = static_cast<Switch*>(c);
        p["checked"] = x->checked();
        p["key"]     = x->key();
    }
    else if( t == "Push" ){
        p["key"] = static_cast<Push*>(c)->key();
    }
    else if( t == "SwitchDip" ){
        SwitchDip* x = static_cast<SwitchDip*>(c);
        p["state"]     = x->state();
        p["size"]      = x->size();
        p["exclusive"] = x->exclusive();
        p["commonPin"] = x->commonPin();
    }
    else if( t == "Relay" || t == "RelaySPST" ){
        Relay* x = static_cast<Relay*>(c);
        p["iTrig"] = x->iTrig();
        p["iRel"]  = x->iRel();
    }
    return p;
}

static int applyControllableProps( Component* c, const QJsonObject& props )
{
    int applied = 0;
    const QString t = c->itemType();

    auto getD = [&](const char* k, double& out)->bool {
        QJsonValue v = props.value( QLatin1String(k) );
        if( !v.isDouble() ) return false;
        out = v.toDouble();
        return true;
    };
    auto getB = [&](const char* k, bool& out)->bool {
        QJsonValue v = props.value( QLatin1String(k) );
        if( !v.isBool() ) return false;
        out = v.toBool();
        return true;
    };
    auto getI = [&](const char* k, int& out)->bool {
        QJsonValue v = props.value( QLatin1String(k) );
        if( !v.isDouble() ) return false;
        out = v.toInt();
        return true;
    };
    auto getS = [&](const char* k, QString& out)->bool {
        QJsonValue v = props.value( QLatin1String(k) );
        if( !v.isString() ) return false;
        out = v.toString();
        return true;
    };

    double d; bool b; int i; QString s;

    if( t == "Fixed Voltage" ){
        FixedVolt* x = static_cast<FixedVolt*>(c);
        if( getD("voltage", d) ){ x->setVolt(d); ++applied; }
        if( getB("out", b) )    { x->setOut(b);  ++applied; }
    }
    else if( t == "Battery" ){
        Battery* x = static_cast<Battery*>(c);
        if( getD("voltage", d) )    { x->setVoltage(d);    ++applied; }
        if( getD("resistance", d) ) { x->setResistance(d); ++applied; }
    }
    else if( t == "Rail" ){
        if( getD("voltage", d) ){ static_cast<Rail*>(c)->setVolt(d); ++applied; }
    }
    else if( t == "Voltage Source" || t == "Current Source" ){
        VarSource* x = static_cast<VarSource*>(c);
        if( getD("min", d) )      { x->setMinValue(d); ++applied; }
        if( getD("max", d) )      { x->setMaxValue(d); ++applied; }
        if( getD("value", d) )    { x->setVal(d);      ++applied; }
        if( getB("running", b) )  { x->setRunning(b);  ++applied; }
    }
    else if( t == "Clock" ){
        Clock* x = static_cast<Clock*>(c);
        if( getD("frequency", d) ){ x->setFreq(d);     ++applied; }
        if( getD("voltage", d) )  { x->setVolt(d);     ++applied; }
        if( getB("alwaysOn", b) ) { x->setAlwaysOn(b); ++applied; }
        if( getB("running", b) )  { x->setRunning(b);  ++applied; }
    }
    else if( t == "WaveGen" ){
        WaveGen* x = static_cast<WaveGen*>(c);
        if( getD("frequency", d) ) { x->setFreq(d);       ++applied; }
        if( getD("duty", d) )      { x->setDuty(d);       ++applied; }
        if( getD("phaseShift", d) ){ x->setPhaseShift(d); ++applied; }
        if( getS("waveType", s) )  { x->setWaveType(s);   ++applied; }
        if( getD("semiAmpli", d) ) { x->setSemiAmpli(d);  ++applied; }
        if( getD("midVolt", d) )   { x->setMidVolt(d);    ++applied; }
        if( getB("bipolar", b) )   { x->setBipolar(b);    ++applied; }
        if( getB("floating", b) )  { x->setFloating(b);   ++applied; }
        if( getB("running", b) )   { x->setRunning(b);    ++applied; }
    }
    else if( t == "Csource" ){
        Csource* x = static_cast<Csource*>(c);
        if( getD("gain", d) )       { x->setGain(d);        ++applied; }
        if( getD("voltage", d) )    { x->setVolt(d);        ++applied; }
        if( getD("outCurrent", d) ) { x->setOutCurrent(d);  ++applied; }
        if( getB("currSource", b) ) { x->setCurrSource(b);  ++applied; }
        if( getB("currControl", b)) { x->setCurrControl(b); ++applied; }
    }
    else if( t == "Switch" ){
        Switch* x = static_cast<Switch*>(c);
        if( getB("checked", b) ){ x->setChecked(b); ++applied; }
        if( getS("key", s) )    { x->setKey(s);     ++applied; }
    }
    else if( t == "Push" ){
        if( getS("key", s) ){ static_cast<Push*>(c)->setKey(s); ++applied; }
    }
    else if( t == "SwitchDip" ){
        SwitchDip* x = static_cast<SwitchDip*>(c);
        if( getI("size", i) )      { x->setSize(i);      ++applied; }
        if( getI("state", i) )     { x->setState(i);     ++applied; }
        if( getB("exclusive", b) ) { x->setExclusive(b); ++applied; }
        if( getB("commonPin", b) ) { x->setCommonPin(b); ++applied; }
    }
    else if( t == "Relay" || t == "RelaySPST" ){
        Relay* x = static_cast<Relay*>(c);
        if( getD("iTrig", d) ){ x->setITrig(d); ++applied; }
        if( getD("iRel",  d) ){ x->setIRel(d);  ++applied; }
    }
    return applied;
}

std::string SimulIDEBridge::listControllables()
{
    QJsonArray arr;
    if( Circuit::self() ){
        for( Component* c : *Circuit::self()->compList() ){
            const QString kind = componentKind( c->itemType() );
            if( kind.isEmpty() ) continue;

            QJsonObject obj;
            obj["id"]    = c->getUid();
            obj["label"] = c->idLabel();
            obj["type"]  = c->itemType();
            obj["kind"]  = kind;

            QJsonObject props = readControllableProps( c );
            QJsonArray names;
            for( const QString& k : props.keys() ) names.append( k );
            obj["properties"] = names;
            arr.append( obj );
        }
    }
    return QJsonDocument( arr ).toJson( QJsonDocument::Compact ).toStdString();
}

std::string SimulIDEBridge::getProperties( const std::string& id )
{
    if( !Circuit::self() ) return "null";
    Component* c = Circuit::self()->getCompById( QString::fromStdString(id) );
    if( !c ) return "null";
    const QString kind = componentKind( c->itemType() );
    if( kind.isEmpty() ) return "null";

    QJsonObject obj;
    obj["id"]    = c->getUid();
    obj["label"] = c->idLabel();
    obj["type"]  = c->itemType();
    obj["kind"]  = kind;
    obj["props"] = readControllableProps( c );
    return QJsonDocument( obj ).toJson( QJsonDocument::Compact ).toStdString();
}

bool SimulIDEBridge::setProperty( const std::string& id, const std::string& jsonProps )
{
    if( !Circuit::self() ){ emitError("setProperty: no circuit"); return false; }
    Component* c = Circuit::self()->getCompById( QString::fromStdString(id) );
    if( !c ){ emitError("setProperty: component not found: "+id); return false; }
    if( componentKind( c->itemType() ).isEmpty() ){
        emitError("setProperty: not a controllable type: "+c->itemType().toStdString());
        return false;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString( jsonProps ), &err );
    if( err.error != QJsonParseError::NoError || !doc.isObject() ){
        emitError("setProperty: invalid JSON object");
        return false;
    }
    return applyControllableProps( c, doc.object() ) > 0;
}

bool SimulIDEBridge::pressButton( const std::string& id, bool down )
{
    if( !Circuit::self() ) return false;
    Component* c = Circuit::self()->getCompById( QString::fromStdString(id) );
    if( !c || c->itemType() != "Push" ) return false;
    PushBase* pb = static_cast<Push*>(c);
    if( down ) pb->onbuttonPressed();
    else       pb->onbuttonReleased();
    return true;
}

// ---------------------------------------------------------------------------
// Event subscriptions
// ---------------------------------------------------------------------------

void SimulIDEBridge::onStateChange( val cb ) { m_stateCb    = cb; }
void SimulIDEBridge::onError      ( val cb ) { m_errorCb    = cb; }
void SimulIDEBridge::onCompiled   ( val cb ) { m_compiledCb = cb; }

// ---------------------------------------------------------------------------
// Event emission (called from C++ at the point the event happens)
// ---------------------------------------------------------------------------

static inline bool isCallable( const val& v )
{
    return !v.isNull() && !v.isUndefined();
}

void SimulIDEBridge::emitStateChange( const std::string& state )
{
    if( isCallable( m_stateCb ) ) m_stateCb( state );
}

void SimulIDEBridge::emitError( const std::string& message )
{
    if( isCallable( m_errorCb ) ) m_errorCb( message );
}

void SimulIDEBridge::emitCompiled( bool ok )
{
    if( isCallable( m_compiledCb ) ) m_compiledCb( ok );
}

// ---------------------------------------------------------------------------
// Embind: expose the singleton to JavaScript
// ---------------------------------------------------------------------------

EMSCRIPTEN_BINDINGS(simulide_bridge)
{
    using namespace emscripten;

    class_<SimulIDEBridge>( "SimulIDEBridge" )
        .class_function( "instance", &SimulIDEBridge::instance, allow_raw_pointers() )

        .function( "setCircuit", &SimulIDEBridge::setCircuit )
        .function( "getCircuit", &SimulIDEBridge::getCircuit )

        .function( "setSketch",  &SimulIDEBridge::setSketch )
        .function( "getSketch",  &SimulIDEBridge::getSketch )

        .function( "compileAndFlash", &SimulIDEBridge::compileAndFlash )

        .function( "startSimulation",    &SimulIDEBridge::startSimulation )
        .function( "stopSimulation",     &SimulIDEBridge::stopSimulation )
        .function( "pauseSimulation",    &SimulIDEBridge::pauseSimulation )
        .function( "getSimulationState", &SimulIDEBridge::getSimulationState )

        .function( "listMeters",     &SimulIDEBridge::listMeters )
        .function( "readMeter",      &SimulIDEBridge::readMeter )
        .function( "readAllMeters",  &SimulIDEBridge::readAllMeters )

        .function( "listNodes",      &SimulIDEBridge::listNodes )
        .function( "getNodeVoltage", &SimulIDEBridge::getNodeVoltage )
        .function( "readAllNodes",   &SimulIDEBridge::readAllNodes )

        .function( "listControllables", &SimulIDEBridge::listControllables )
        .function( "getProperties",     &SimulIDEBridge::getProperties )
        .function( "setProperty",       &SimulIDEBridge::setProperty )
        .function( "pressButton",       &SimulIDEBridge::pressButton )

        .function( "onStateChange", &SimulIDEBridge::onStateChange )
        .function( "onError",       &SimulIDEBridge::onError )
        .function( "onCompiled",    &SimulIDEBridge::onCompiled );
}

#endif // __EMSCRIPTEN__
