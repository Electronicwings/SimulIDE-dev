#ifndef MCUTEMPLATES_H
#define MCUTEMPLATES_H

// Source templates seeded into a fresh editor tab when an MCU/board is
// dropped onto the schematic. Per-family content lives here as constants
// so subclasses of Mcu just decide which one to return.

namespace McuTemplates {

// inline constexpr const char* arduinoBlink =
//     "// Auto-generated for Arduino board\n"
//     "\n"
//     "void setup() {\n"
//     "    pinMode(LED_BUILTIN, OUTPUT);\n"
//     "}\n"
//     "\n"
//     "void loop() {\n"
//     "    digitalWrite(LED_BUILTIN, HIGH);\n"
//     "    delay(500);\n"
//     "    digitalWrite(LED_BUILTIN, LOW);\n"
//     "    delay(500);\n"
//     "}\n";

// inline constexpr const char* genericMain =
//     "// Auto-generated stub for MCU.\n"
//     "\n"
//     "int main(void) {\n"
//     "\n"
//     "    // TODO: setup code\n"
//     "\n"
//     "    while (1) {\n"
//     "        // TODO: loop code\n"
//     "    }\n"
//     "    return 0;\n"
//     "}\n";

inline constexpr const char* arduinoBlink = "";

inline constexpr const char* genericMain = "";

}

#endif // MCUTEMPLATES_H
