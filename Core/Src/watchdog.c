#include "watchdog.h"

#include "FreeRTOS.h"
#include "iwdg.h"
#include "task.h"

#include <stdint.h>

/* FreeRTOS uses a 1 ms tick. A task reports only after completing its work. */
static const TickType_t heartbeat_deadline[WATCHDOG_ID_COUNT] = {
  1500U, /* defaultTask: OLED transactions can take up to 8 x 100 ms */
   100U, /* inputTask */
  3000U, /* cdcTask: USB writes can wait up to 500 ms each */
   500U, /* thermalTask */
   100U, /* conveyorTask */
  1000U, /* inspectionTask: lower priority than display work */
   500U  /* idle task: also detects CPU starvation */
};

static volatile TickType_t last_heartbeat[WATCHDOG_ID_COUNT];
static volatile uint32_t seen_mask;

void Watchdog_Heartbeat(WatchdogId id)
{
  if ((unsigned int)id >= (unsigned int)WATCHDOG_ID_COUNT)
  {
    return;
  }

  taskENTER_CRITICAL();
  last_heartbeat[id] = xTaskGetTickCount();
  seen_mask |= (uint32_t)1U << (unsigned int)id;
  taskEXIT_CRITICAL();
}

void Watchdog_IdleHook(void)
{
  static TickType_t last_check;
  static uint8_t failed;
  TickType_t now = xTaskGetTickCount();
  uint8_t all_seen;
  const uint32_t required_mask = ((uint32_t)1U << WATCHDOG_ID_COUNT) - 1U;

  /* Idle is the sole IWDG supervisor. If it cannot run, refresh stops. */
  if ((now - last_check) < pdMS_TO_TICKS(100U))
  {
    return;
  }
  Watchdog_Heartbeat(WATCHDOG_ID_IDLE);

  /* Snapshot the clock and heartbeat timestamps without task preemption.
   * Otherwise a newer timestamp could appear after an older 'now'. */
  taskENTER_CRITICAL();
  now = xTaskGetTickCount();
  last_check = now;
  all_seen = (seen_mask == required_mask) ? 1U : 0U;
  if ((all_seen != 0U) && (failed == 0U))
  {
    for (unsigned int id = 0U; id < (unsigned int)WATCHDOG_ID_COUNT; ++id)
    {
      if ((now - last_heartbeat[id]) > heartbeat_deadline[id])
      {
        failed = 1U;
        break;
      }
    }
  }
  taskEXIT_CRITICAL();

  if ((all_seen == 0U) || (failed != 0U))
  {
    return;
  }

  (void)HAL_IWDG_Refresh(&hiwdg);
}
