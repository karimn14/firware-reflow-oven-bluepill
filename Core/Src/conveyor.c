#include "conveyor.h"

#include <stddef.h>
#include <string.h>

void Conveyor_Init(ConveyorMotion *motion, TIM_HandleTypeDef *pwm_timer,
                   uint32_t pwm_channel, uint8_t minimum_duty,
                   TIM_HandleTypeDef *encoder_timer,
                   uint32_t encoder_slots)
{
  if (motion == NULL)
  {
    return;
  }
  memset(motion, 0, sizeof(*motion));
  SingleEncoder_Init(&motion->encoder, encoder_timer, encoder_slots);
  ConveyorMotor_Init(&motion->motor, pwm_timer, pwm_channel, minimum_duty);
  motion->state = CONVEYOR_MOTION_STOPPED;
}

void Conveyor_Move(ConveyorMotion *motion, uint8_t duty_percent,
                   uint32_t target_pulses)
{
  if (motion == NULL)
  {
    return;
  }
  motion->target_pulses = target_pulses;
  motion->current_pulses = 0U;
  (void)SingleEncoder_ReadDelta(&motion->encoder);
  motion->state = CONVEYOR_MOTION_MOVING;
  ConveyorMotor_SetOutput(&motion->motor, duty_percent);
}

void Conveyor_Stop(ConveyorMotion *motion)
{
  if (motion == NULL)
  {
    return;
  }
  ConveyorMotor_SetOutput(&motion->motor, 0U);
  if (motion->state == CONVEYOR_MOTION_MOVING)
  {
    motion->state = CONVEYOR_MOTION_TARGET_REACHED;
  }
}

void Conveyor_EmergencyStop(ConveyorMotion *motion)
{
  if (motion == NULL)
  {
    return;
  }
  ConveyorMotor_SetOutput(&motion->motor, 0U);
  motion->state = CONVEYOR_MOTION_ERROR;
}

void Conveyor_Reset(ConveyorMotion *motion)
{
  if (motion == NULL)
  {
    return;
  }
  ConveyorMotor_SetOutput(&motion->motor, 0U);
  motion->target_pulses = 0U;
  motion->current_pulses = 0U;
  motion->state = CONVEYOR_MOTION_STOPPED;
}

void Conveyor_Update(ConveyorMotion *motion)
{
  if (motion == NULL)
  {
    return;
  }
  if (motion->state == CONVEYOR_MOTION_MOVING)
  {
    motion->current_pulses += SingleEncoder_ReadDelta(&motion->encoder);
    if ((motion->target_pulses != 0U)
        && (motion->current_pulses >= motion->target_pulses))
    {
      Conveyor_Stop(motion);
    }
  }
  else
  {
    ConveyorMotor_SetOutput(&motion->motor, 0U);
  }
}
