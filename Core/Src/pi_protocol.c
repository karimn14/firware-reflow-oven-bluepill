/* Blue Pill <-> Raspberry Pi protocol frames (sunda_reflow_oven/docs/stm-pi-protocol.md).
 * Plain C: builds and parses $TYPE,key=value,...*CS lines. No HAL, no RTOS. */
#include "pi_protocol.h"
#include <string.h>

uint8_t PiProto_Checksum(const char *body, size_t length)
{
  uint8_t cs = 0U;
  for (size_t i = 0U; i < length; ++i) { cs ^= (uint8_t)body[i]; }
  return cs;
}

static void put_char(PiFrameBuilder *b, char c)
{
  if (b->len + 1U < PI_FRAME_MAX) { b->buf[b->len++] = c; }  /* keep room for the NUL */
  else                            { b->overflow = 1U; }
}

static void put_str(PiFrameBuilder *b, const char *s)
{
  while (*s != '\0') { put_char(b, *s++); }
}

static void put_uint(PiFrameBuilder *b, uint32_t v)
{
  char digits[10];
  uint8_t n = 0U;
  do { digits[n++] = (char)('0' + (v % 10U)); v /= 10U; } while (v != 0U);
  while (n > 0U) { put_char(b, digits[--n]); }
}

static void put_key(PiFrameBuilder *b, const char *key)
{
  put_char(b, ',');
  put_str(b, key);
  put_char(b, '=');
}

void PiProto_Begin(PiFrameBuilder *b, const char *type)
{
  b->len = 0U;
  b->overflow = 0U;
  put_char(b, '$');
  put_str(b, type);
}

void PiProto_AddStr(PiFrameBuilder *b, const char *key, const char *value)
{
  put_key(b, key);
  put_str(b, value);
}

void PiProto_AddUint(PiFrameBuilder *b, const char *key, uint32_t value)
{
  put_key(b, key);
  put_uint(b, value);
}

void PiProto_AddTenths(PiFrameBuilder *b, const char *key, int32_t tenths)
{
  uint32_t magnitude = (tenths < 0) ? (uint32_t)(-tenths) : (uint32_t)tenths;
  put_key(b, key);
  if (tenths < 0) { put_char(b, '-'); }
  put_uint(b, magnitude / 10U);
  put_char(b, '.');
  put_char(b, (char)('0' + (magnitude % 10U)));
}

size_t PiProto_End(PiFrameBuilder *b)
{
  static const char hex[] = "0123456789ABCDEF";
  uint8_t cs = PiProto_Checksum(&b->buf[1], b->len - 1U);   /* between '$' and '*' */
  put_char(b, '*');
  put_char(b, hex[cs >> 4]);
  put_char(b, hex[cs & 0x0FU]);
  put_char(b, '\n');
  if (b->overflow != 0U) { return 0U; }
  b->buf[b->len] = '\0';
  return b->len;
}

static uint8_t hex_value(char c, uint8_t *v)
{
  if (c >= '0' && c <= '9') { *v = (uint8_t)(c - '0');        return 1U; }
  if (c >= 'A' && c <= 'F') { *v = (uint8_t)(c - 'A' + 10);   return 1U; }
  if (c >= 'a' && c <= 'f') { *v = (uint8_t)(c - 'a' + 10);   return 1U; }
  return 0U;
}

static uint8_t valid_type(const char *s)
{
  size_t n = strlen(s);
  if (n < 2U || n > 8U) { return 0U; }
  for (size_t i = 0U; i < n; ++i) { if (s[i] < 'A' || s[i] > 'Z') { return 0U; } }
  return 1U;
}

static uint8_t valid_key(const char *s)
{
  size_t n = strlen(s);
  if (n < 1U || n > 12U || s[0] < 'a' || s[0] > 'z') { return 0U; }
  for (size_t i = 1U; i < n; ++i) {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) { return 0U; }
  }
  return 1U;
}

int PiProto_Parse(char *line, PiFrame *frame)
{
  size_t len = strlen(line);
  uint8_t hi, lo;
  char *cursor, *comma;

  while (len > 0U && (line[len - 1U] == '\r' || line[len - 1U] == '\n')) { line[--len] = '\0'; }
  if (len < 5U || line[0] != '$' || line[len - 3U] != '*') { return PI_PROTO_ERR_FORMAT; }
  if (!hex_value(line[len - 2U], &hi) || !hex_value(line[len - 1U], &lo)) { return PI_PROTO_ERR_FORMAT; }
  if (PiProto_Checksum(&line[1], len - 4U) != (uint8_t)((hi << 4) | lo)) { return PI_PROTO_ERR_CHECKSUM; }
  line[len - 3U] = '\0';                                   /* cut off "*CS" */

  cursor = &line[1];
  comma = strchr(cursor, ',');
  if (comma != NULL) { *comma = '\0'; }
  frame->type = cursor;
  frame->count = 0U;
  if (!valid_type(frame->type)) { return PI_PROTO_ERR_FORMAT; }

  while (comma != NULL) {
    char *eq;
    cursor = comma + 1;
    comma = strchr(cursor, ',');
    if (comma != NULL) { *comma = '\0'; }
    eq = strchr(cursor, '=');
    if (eq == NULL || frame->count >= PI_FIELDS_MAX) { return PI_PROTO_ERR_FORMAT; }
    *eq = '\0';
    if (!valid_key(cursor)) { return PI_PROTO_ERR_FORMAT; }
    frame->fields[frame->count].key = cursor;
    frame->fields[frame->count].value = eq + 1;
    frame->count++;
  }
  return PI_PROTO_OK;
}

const char *PiProto_Get(const PiFrame *frame, const char *key)
{
  for (uint8_t i = 0U; i < frame->count; ++i) {
    if (strcmp(frame->fields[i].key, key) == 0) { return frame->fields[i].value; }
  }
  return NULL;
}

uint8_t PiProto_GetUint(const PiFrame *frame, const char *key, uint32_t *value)
{
  const char *s = PiProto_Get(frame, key);
  uint32_t v = 0U;
  if (s == NULL || *s == '\0') { return 0U; }
  for (; *s != '\0'; ++s) {
    if (*s < '0' || *s > '9' || v > 429496728U) { return 0U; }   /* digits only, no overflow */
    v = v * 10U + (uint32_t)(*s - '0');
  }
  *value = v;
  return 1U;
}
