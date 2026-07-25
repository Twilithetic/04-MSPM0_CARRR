/*
 *  ======== line.h ========
 *  Line sensor (5-channel gray-scale) hardware proxy.
 *
 *  sync_from_device(): GPIO read → compute weighted position → write shadow
 */

#ifndef LINE_H
#define LINE_H

#include "include/line_reg.h"

/// Line proxy — carries only sensor configuration (no state)
typedef struct {
    uint8_t         black_level;   // threshold: 0 = black (on line)
    const uint32_t *pins;          // GPIO pin masks for S1-S5
    const int16_t  *positions;     // weighted positions for S1-S5
} LineProxy;

/// sync_from_device: read GPIO → compute position → write g_line_reg
void sync_from_device(const LineProxy *p, LineReg *r);

#endif /* LINE_H */
