#ifndef JSBRIDGE_H
#define JSBRIDGE_H

#ifdef __EMSCRIPTEN__

#include <emscripten/val.h>
#include <string>

// SimulIDEBridge — same-origin JavaScript control surface for the SimulIDE
// WASM build. Exposes a small singleton to embind so a parent web app can
// drive the simulator from `iframe.contentWindow.Module.SimulIDEBridge.instance()`:
//
//   const bridge = iframe.contentWindow.Module.SimulIDEBridge.instance();
//   bridge.onStateChange(s => console.log("sim:", s));
//   bridge.setCircuit(savedSim2Xml);
//   bridge.setSketch(arduinoSource);
//   bridge.compileAndFlash();
//   bridge.startSimulation();
//
// Phase 1 surface is intentionally narrow — circuit/sketch get/set and
// simulation lifecycle. Per-module monitoring (voltages, currents, serial,
// registers) will be added as separate bridge classes later.
class SimulIDEBridge
{
    public:
        static SimulIDEBridge* instance();

        // Circuit (symmetric — set takes a .sim2 XML string, get returns one).
        bool        setCircuit( const std::string& sim2Xml );
        std::string getCircuit();

        // Sketch (symmetric). setSketch requires an MCU on the schematic;
        // returns false and emits an error event otherwise.
        bool        setSketch( const std::string& source );
        std::string getSketch();

        // Build + flash. Fire-and-forget: outcome is delivered via
        // onCompiled / onStateChange events when the remote build replies.
        void compileAndFlash();

        // Simulation lifecycle.
        void        startSimulation();
        void        stopSimulation();
        void        pauseSimulation();
        std::string getSimulationState();

        // Meter readings — Voltimeter / Amperimeter / Probe / FreqMeter
        // components placed on the schematic. Components are addressed by
        // their unique id (Component::getUid()). All return JSON strings:
        //   listMeters()   -> [{id,label,type,unit}, ...]
        //   readMeter(id)  -> {id,label,type,unit,value}    (or "null")
        //   readAllMeters()-> [{id,label,type,unit,value}, ...]
        std::string listMeters();
        std::string readMeter( const std::string& id );
        std::string readAllMeters();

        // Node voltages — every electrical node the simulator created from
        // the active circuit. Nodes are addressed by eNode::itemId().
        //   listNodes()           -> [{id,number}, ...]
        //   getNodeVoltage(id)    -> double (NaN if id not found)
        //   readAllNodes()        -> [{id,number,voltage}, ...]
        std::string listNodes();
        double      getNodeVoltage( const std::string& id );
        std::string readAllNodes();

        // Sources and switches — generic property bag for runtime control.
        // Each supported type exposes a fixed set of named properties (e.g.
        // FixedVolt -> {voltage:double, out:bool}, Switch -> {checked:bool,
        // key:string}, WaveGen -> {frequency,duty,waveType,...}). All values
        // are pushed/pulled through JSON so the C++ side can stay narrow.
        //   listControllables()                   -> [{id,label,type,kind,properties:[names]}, ...]
        //   getProperties(id)                     -> {id,label,type,kind,props:{name:value,...}}  or "null"
        //   setProperty(id, jsonProps)            -> true if at least one prop applied
        //   pressButton(id, down)                 -> true on Push components only
        std::string listControllables();
        std::string getProperties( const std::string& id );
        bool        setProperty( const std::string& id, const std::string& jsonProps );
        bool        pressButton( const std::string& id, bool down );

        // Event subscriptions. Each takes a JS function; passing a fresh
        // callback replaces the previous one (we don't multicast yet).
        void onStateChange( emscripten::val cb );
        void onError      ( emscripten::val cb );
        void onCompiled   ( emscripten::val cb );

        // Internal — invoked from C++ when the corresponding event happens.
        // Safe to call when no callback has been registered (no-op).
        void emitStateChange( const std::string& state );
        void emitError      ( const std::string& message );
        void emitCompiled   ( bool ok );

    private:
        SimulIDEBridge();

        emscripten::val m_stateCb;
        emscripten::val m_errorCb;
        emscripten::val m_compiledCb;
};

#endif // __EMSCRIPTEN__
#endif // JSBRIDGE_H
