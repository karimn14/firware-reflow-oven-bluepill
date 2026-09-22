#ifndef CONVEYOR_ENCODER_H
#define CONVEYOR_ENCODER_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  TIM_HandleTypeDef *timer;
  uint32_t slots_per_rev;
  uint16_t last_counter;
  uint16_t last_delta_ticks;
} SingleChannelEncoder;

void SingleEncoder_Init(SingleChannelEncoder *encoder,
                        TIM_HandleTypeDef *timer, uint32_t slots_per_rev);
uint16_t SingleEncoder_ReadDelta(SingleChannelEncoder *encoder);

#endif /* CONVEYOR_ENCODER_H */
