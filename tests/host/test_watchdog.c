#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "iwdg.h"
#include "watchdog.h"

IWDG_HandleTypeDef hiwdg;
static TickType_t now_ms;
static unsigned int refresh_count;

TickType_t xTaskGetTickCount(void) { return now_ms; }

HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *handle)
{
  assert(handle == &hiwdg);
  ++refresh_count;
  return 0;
}

static void heartbeat_all_application_tasks(void)
{
  for (int id = WATCHDOG_ID_DEFAULT; id < WATCHDOG_ID_IDLE; ++id)
  {
    Watchdog_Heartbeat((WatchdogId)id);
  }
}

int main(void)
{
  now_ms = 100U;
  Watchdog_IdleHook();
  assert(refresh_count == 0U); /* no application task has reported */

  heartbeat_all_application_tasks();
  now_ms = 200U;
  Watchdog_IdleHook();
  assert(refresh_count == 1U);

  now_ms = 250U;
  heartbeat_all_application_tasks();
  now_ms = 300U;
  Watchdog_IdleHook();
  assert(refresh_count == 2U);

  /* Tick subtraction still works when the 32-bit counter wraps. */
  now_ms = UINT32_MAX - 50U;
  heartbeat_all_application_tasks();
  now_ms = 80U;
  heartbeat_all_application_tasks();
  Watchdog_IdleHook();
  assert(refresh_count == 3U);

  /* Other task reports cannot conceal a stale CDC heartbeat. */
  now_ms = 3300U;
  for (int id = WATCHDOG_ID_DEFAULT; id < WATCHDOG_ID_IDLE; ++id)
  {
    if (id != WATCHDOG_ID_CDC)
    {
      Watchdog_Heartbeat((WatchdogId)id);
    }
  }
  Watchdog_IdleHook();
  assert(refresh_count == 3U);

  heartbeat_all_application_tasks();
  now_ms = 3400U;
  Watchdog_IdleHook();
  assert(refresh_count == 3U); /* a missed deadline stays latched */

  puts("watchdog: PASS");
  return 0;
}
