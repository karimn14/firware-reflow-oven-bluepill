#ifndef CONVEYOR_SERVO_H
#define CONVEYOR_SERVO_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

#define SERVO_PULSE_US_MIN 500U
#define SERVO_PULSE_US_MAX 2500U

typedef struct
{
  TIM_HandleTypeDef *timer;
  uint32_t channel;
  uint16_t pulse_us;
} ConveyorServo;

void ConveyorServo_Init(ConveyorServo *servo, TIM_HandleTypeDef *timer,
                        uint32_t channel);
void ConveyorServo_SetPulseUs(ConveyorServo *servo, uint16_t pulse_us);

#endif /* CONVEYOR_SERVO_H */
