#ifndef CONVEYOR_MOTOR_H
#define CONVEYOR_MOTOR_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

typedef struct
{
  TIM_HandleTypeDef *timer;
  uint32_t channel;
  uint8_t minimum_duty_percent;
  uint8_t duty_percent;
} ConveyorMotor;

void ConveyorMotor_Init(ConveyorMotor *motor, TIM_HandleTypeDef *timer,
                        uint32_t channel, uint8_t minimum_duty_percent);
void ConveyorMotor_SetOutput(ConveyorMotor *motor, uint8_t duty_percent);

#endif /* CONVEYOR_MOTOR_H */
