#ifndef CDC_CONSOLE_H
#define CDC_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void CDC_Console_OnReceive(const uint8_t *data, uint32_t length);
void CDC_Console_OnControlLineState(uint8_t dtr_active);
void CDC_Console_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* CDC_CONSOLE_H */
