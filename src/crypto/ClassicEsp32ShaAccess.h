/*
 * SPDX-FileCopyrightText: 2010-2021 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * DPORT workaround derived from ESP-IDF v4.4.6, components/esp_hw_support/
 * port/esp32/dport_access.c. The APB pre-read and interrupt protection are
 * retained; only the per-poll function call is removed.
 */
#pragma once

#include <stdint.h>
#include <sdkconfig.h>
#include <soc/dport_access.h>
#include <soc/hwcrypto_reg.h>

namespace classic_sha {

static inline __attribute__((always_inline)) void waitIdle()
{
#if defined(CONFIG_ESP32_DPORT_WORKAROUND) && defined(ESP_PLATFORM)
    uint32_t saved_ps, scratch, busy;
    const uint32_t apb = 0x3ff40078;
    const uint32_t status = SHA_256_BUSY_REG;
    // The same protected APB pre-read used by ESP-IDF, before EVERY DPORT
    // read. MEMW completes the preceding START/CONTINUE/LOAD store before
    // reading BUSY. A compiler memory clobber alone does not drain Xtensa's
    // write buffer: without MEMW a fast IRAM poll can observe pre-command idle.
    // No SHA input or state register is written while BUSY is set.
    __asm__ __volatile__(
        "memw\n"
        "rsil %[ps], " XTSTR(CONFIG_ESP32_DPORT_DIS_INTERRUPT_LVL) "\n"
        "1: l32i %[scratch], %[apb], 0\n"
        "l32i %[busy], %[status], 0\n"
        "bnez %[busy], 1b\n"
        "wsr %[ps], ps\n"
        "rsync\n"
        : [ps] "=&a" (saved_ps), [scratch] "=&a" (scratch),
          [busy] "=&a" (busy)
        : [apb] "a" (apb), [status] "a" (status)
        : "memory");
#else
    while (DPORT_REG_READ(SHA_256_BUSY_REG)) {}
#endif
}

static inline __attribute__((always_inline)) uint32_t waitIdleAndReadFinalWord()
{
#if defined(CONFIG_ESP32_DPORT_WORKAROUND) && defined(ESP_PLATFORM)
    uint32_t saved_ps, scratch, value;
    const uint32_t apb = 0x3ff40078;
    const uint32_t status = SHA_256_BUSY_REG;
    const uint32_t digest = SHA_TEXT_BASE + 7 * sizeof(uint32_t);
    __asm__ __volatile__(
        "memw\n"
        "rsil %[ps], " XTSTR(CONFIG_ESP32_DPORT_DIS_INTERRUPT_LVL) "\n"
        "1: l32i %[scratch], %[apb], 0\n"
        "l32i %[value], %[status], 0\n"
        "bnez %[value], 1b\n"
        "l32i %[scratch], %[apb], 0\n"
        "l32i %[value], %[digest], 0\n"
        "wsr %[ps], ps\n"
        "rsync\n"
        : [ps] "=&a" (saved_ps), [scratch] "=&a" (scratch),
          [value] "=&a" (value)
        : [apb] "a" (apb), [status] "a" (status), [digest] "a" (digest)
        : "memory");
    return value;
#else
    waitIdle();
    return DPORT_REG_READ(SHA_TEXT_BASE + 7 * sizeof(uint32_t));
#endif
}

static inline __attribute__((always_inline)) uint32_t byteSwap(uint32_t value)
{
    // Avoid the Xtensa libgcc __bswapsi2 call for every nonce. These are
    // ordinary integer instructions; they do not access the SHA peripheral.
    // Keep the conversion after START, before polling, so it overlaps only
    // CPU work with compression. The barrier prevents hoisting past MMIO.
    uint32_t low, high;
    const uint32_t mask = 0x00ff00ff;
    __asm__ __volatile__(
        "srli %[low], %[value], 8\n"
        "slli %[high], %[value], 8\n"
        "and %[low], %[low], %[mask]\n"
        "and %[high], %[high], %[inverse]\n"
        "or %[low], %[low], %[high]\n"
        "srli %[high], %[low], 16\n"
        "slli %[low], %[low], 16\n"
        "or %[low], %[low], %[high]\n"
        : [low] "=&a" (low), [high] "=&a" (high)
        : [value] "a" (value), [mask] "a" (mask), [inverse] "a" (~mask)
        : "memory");
    return low;
}

} // namespace classic_sha
