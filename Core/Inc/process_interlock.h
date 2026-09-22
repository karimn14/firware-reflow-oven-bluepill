#ifndef PROCESS_INTERLOCK_H
#define PROCESS_INTERLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"

#include <stdint.h>

typedef enum
{
  PROCESS_OWNER_NONE = 0,
  PROCESS_OWNER_CONVEYOR,
  PROCESS_OWNER_HEATER
} ProcessOwner;

void ProcessInterlock_Init(void);
uint8_t ProcessInterlock_Take(ProcessOwner owner, TickType_t wait_ticks);
void ProcessInterlock_Give(ProcessOwner owner);
ProcessOwner ProcessInterlock_GetOwner(void);

#ifdef __cplusplus
}
#endif

#endif /* PROCESS_INTERLOCK_H */
