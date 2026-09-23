#ifndef WATCHDOG_H
#define WATCHDOG_H

/* Stable IDs for the tasks supervised by the independent watchdog. */
typedef enum
{
  WATCHDOG_ID_DEFAULT = 0,
  WATCHDOG_ID_INPUT,
  WATCHDOG_ID_CDC,
  WATCHDOG_ID_THERMAL,
  WATCHDOG_ID_CONVEYOR,
  WATCHDOG_ID_INSPECTION,
  WATCHDOG_ID_IDLE,
  WATCHDOG_ID_COUNT
} WatchdogId;

void Watchdog_Heartbeat(WatchdogId id);
void Watchdog_IdleHook(void);

#endif /* WATCHDOG_H */
