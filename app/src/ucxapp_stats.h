#ifndef UCXAPP_STATS_H
#define UCXAPP_STATS_H

/* Payload byte counters per data channel, shown by the stats command (ucxapp_cmds_sys.c). */

#include <stddef.h>

typedef enum {
    UCXAPP_STATS_TCP,
    UCXAPP_STATS_UDP,
    UCXAPP_STATS_SPS,
    UCXAPP_STATS_GATT,
    UCXAPP_STATS_HTTP,
    UCXAPP_STATS_MQTT,
    UCXAPP_STATS_WEB,
    UCXAPP_STATS_COUNT
} ucxapp_stats_ch_t;

void ucxapp_stats_add(ucxapp_stats_ch_t ch, size_t tx, size_t rx);

#endif /* UCXAPP_STATS_H */
