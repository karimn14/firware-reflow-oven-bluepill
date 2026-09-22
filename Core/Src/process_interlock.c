#include "process_interlock.h"

#include "semphr.h"

static StaticSemaphore_t process_mutex_buffer;
static SemaphoreHandle_t process_mutex;
static volatile ProcessOwner process_owner;

void ProcessInterlock_Init(void)
{
  process_owner = PROCESS_OWNER_NONE;
  process_mutex = xSemaphoreCreateMutexStatic(&process_mutex_buffer);
}

uint8_t ProcessInterlock_Take(ProcessOwner owner, TickType_t wait_ticks)
{
  if ((process_mutex == NULL) || (owner == PROCESS_OWNER_NONE))
  {
    return 0U;
  }
  if (xSemaphoreTake(process_mutex, wait_ticks) != pdTRUE)
  {
    return 0U;
  }
  taskENTER_CRITICAL();
  process_owner = owner;
  taskEXIT_CRITICAL();
  return 1U;
}

void ProcessInterlock_Give(ProcessOwner owner)
{
  if ((process_mutex == NULL) || (process_owner != owner))
  {
    return;
  }
  taskENTER_CRITICAL();
  process_owner = PROCESS_OWNER_NONE;
  taskEXIT_CRITICAL();
  (void)xSemaphoreGive(process_mutex);
}

ProcessOwner ProcessInterlock_GetOwner(void)
{
  return process_owner;
}
