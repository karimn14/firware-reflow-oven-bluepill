#ifndef PCB_SENSOR_H
#define PCB_SENSOR_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t active_low;
  uint8_t debounce_ticks;
  uint8_t stable_count;
  uint8_t detected;
} PcbSensor;

void PcbSensor_Init(PcbSensor *sensor, GPIO_TypeDef *port, uint16_t pin,
                    uint8_t active_low, uint8_t debounce_ticks);
uint8_t PcbSensor_Update(PcbSensor *sensor);

#endif /* PCB_SENSOR_H */
