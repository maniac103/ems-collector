/*
 * Buderus EMS frame grabber
 *
 * receives data from the EMS via UART, validates them,
 * adds proper framing and sends those frames out via UART
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

/* all fields are little endian */
typedef struct {
    uint32_t id;
    uint32_t total_bytes;
    uint32_t good_bytes;
    uint32_t dropped_bytes;
    uint32_t onebyte_packets;
    uint32_t good_packets;
    uint32_t bad_packets;
    uint32_t dropped_packets;
} rx_stats_t;

#endif /* __COMMON_H__ */
