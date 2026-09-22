#include "pcb_sensor.h"

#include <stddef.h>

void PcbSensor_Init(PcbSensor *sensor, GPIO_TypeDef *port, uint16_t pin,
                    uint8_t active_low, uint8_t debounce_ticks)
{
  if (sensor == NULL)
  {
    return;
  }
  sensor->port = port;
  sensor->pin = pin;
  sensor->active_low = active_low;
  sensor->debounce_ticks = (debounce_ticks == 0U) ? 1U : debounce_ticks;
  sensor->stable_count = 0U;
  sensor->detected = 0U;
}

uint8_t PcbSensor_Update(PcbSensor *sensor)
{
  uint8_t active;

  if ((sensor == NULL) || (sensor->port == NULL))
  {
    return 0U;
  }
  active = (HAL_GPIO_ReadPin(sensor->port, sensor->pin) == GPIO_PIN_SET)
           ? 1U : 0U;
  if (sensor->active_low != 0U)
  {
    active ^= 1U;
  }
  if (active != 0U)
  {
    if (sensor->stable_count < sensor->debounce_ticks)
    {
      ++sensor->stable_count;
    }
  }
  else
  {
    sensor->stable_count = 0U;
  }
  sensor->detected = (sensor->stable_count >= sensor->debounce_ticks)
                     ? 1U : 0U;
  return sensor->detected;
}
