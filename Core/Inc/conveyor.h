#ifndef CONVEYOR_H
#define CONVEYOR_H

#include "encoder.h"
#include "motor.h"

#include <stdint.h>

typedef enum
{
  CONVEYOR_MOTION_STOPPED = 0,
  CONVEYOR_MOTION_MOVING,
  CONVEYOR_MOTION_TARGET_REACHED,
  CONVEYOR_MOTION_ERROR
} ConveyorMotionState;

typedef struct
{
  ConveyorMotionState state;
  SingleChannelEncoder encoder;
  ConveyorMotor motor;
  uint32_t target_pulses;
  uint32_t current_pulses;
} ConveyorMotion;

void Conveyor_Init(ConveyorMotion *motion, TIM_HandleTypeDef *pwm_timer,
                   uint32_t pwm_channel, uint8_t minimum_duty,
                   TIM_HandleTypeDef *encoder_timer,
                   uint32_t encoder_slots);
void Conveyor_Move(ConveyorMotion *motion, uint8_t duty_percent,
                   uint32_t target_pulses);
void Conveyor_Stop(ConveyorMotion *motion);
void Conveyor_EmergencyStop(ConveyorMotion *motion);
void Conveyor_Reset(ConveyorMotion *motion);
void Conveyor_Update(ConveyorMotion *motion);

#endif /* CONVEYOR_H */
