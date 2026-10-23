/* CAN frame type, controller access and the 17-byte record used by can.bridge. */
#ifndef UCXAPP_CAN_H
#define UCXAPP_CAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Record: ts_ms u32 LE, id u32 LE (above 0x7FF = extended), dlc u8, data[8]. */
#define UCXAPP_CAN_REC_LEN 17
/* Records per datagram: 32 * 17 = 544 bytes, well below the module UDP payload limit. */
#define UCXAPP_CAN_BATCH   32

typedef struct {
    uint32_t id;
    uint8_t len;
    uint8_t data[8];
} ucxapp_can_frame_t;

/* Packs one record into out (UCXAPP_CAN_REC_LEN bytes). len above 8 is clamped to 8. */
size_t ucxapp_can_pack(uint32_t ts_ms, const ucxapp_can_frame_t *f, uint8_t *out);
/* Returns false if the record has a DLC above 8. */
bool ucxapp_can_unpack(const uint8_t *in, uint32_t *ts_ms, ucxapp_can_frame_t *f);

/* CAN controller (emulated loopback until a bxCAN/FDCAN backend is added). Thread safe. */
bool ucxapp_can_send(const ucxapp_can_frame_t *f);
bool ucxapp_can_recv(ucxapp_can_frame_t *f);

#endif
