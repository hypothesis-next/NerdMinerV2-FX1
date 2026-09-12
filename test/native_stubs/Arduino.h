#ifndef TEST_ARDUINO_H
#define TEST_ARDUINO_H

// Attributes used by the ESP32 implementation have no meaning in the native
// correctness harness. Keep the production declarations intact while allowing
// the same source to be compiled by MSVC.
#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif
#ifndef DRAM_ATTR
#define DRAM_ATTR
#endif

#if defined(_MSC_VER) && !defined(__attribute__)
#define __attribute__(value)
#endif

#endif
