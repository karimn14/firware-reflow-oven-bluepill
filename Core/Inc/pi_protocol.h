/* Blue Pill <-> Raspberry Pi protocol frames (sunda_reflow_oven/docs/stm-pi-protocol.md).
 * Plain C: builds and parses $TYPE,key=value,...*CS lines. No HAL, no RTOS. */
#ifndef PI_PROTOCOL_H
#define PI_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define PI_FRAME_MAX   160U   /* bytes including '$', '*CS' and '\n' (protocol limit) */
#define PI_FIELDS_MAX  12U

#define PI_PROTO_OK            0
#define PI_PROTO_ERR_FORMAT   (-1)
#define PI_PROTO_ERR_CHECKSUM (-2)

typedef struct { char *key; char *value; } PiField;
typedef struct { char *type; PiField fields[PI_FIELDS_MAX]; uint8_t count; } PiFrame;
typedef struct { char buf[PI_FRAME_MAX]; size_t len; uint8_t overflow; } PiFrameBuilder;

uint8_t     PiProto_Checksum(const char *body, size_t length);

void        PiProto_Begin(PiFrameBuilder *b, const char *type);
void        PiProto_AddStr(PiFrameBuilder *b, const char *key, const char *value);
void        PiProto_AddUint(PiFrameBuilder *b, const char *key, uint32_t value);
void        PiProto_AddTenths(PiFrameBuilder *b, const char *key, int32_t tenths);
size_t      PiProto_End(PiFrameBuilder *b);          /* length incl. '\n'; 0 if too long */

int         PiProto_Parse(char *line, PiFrame *frame); /* splits line in place */
const char *PiProto_Get(const PiFrame *frame, const char *key);
uint8_t     PiProto_GetUint(const PiFrame *frame, const char *key, uint32_t *value);

#endif /* PI_PROTOCOL_H */
