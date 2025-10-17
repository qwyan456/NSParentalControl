#pragma once

#include <switch.h>

u32 debugResult(u32 value) {
    // Enlever les bits du module (bits 0..8)
    return (value >> 9) & 0x1FFF;
}