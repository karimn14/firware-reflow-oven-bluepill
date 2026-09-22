#include "servo.h"

#include <stddef.h>

void ConveyorServo_Init(ConveyorServo *servo, TIM_HandleTypeDef *timer,
                        uint32_t channel)
{
  if ((servo == NULL) || (timer == NULL))
  {
    return;
  }
  servo->timer = timer;
  servo->channel = channel;
  servo->pulse_us = 0U;
  __HAL_TIM_SET_COMPARE(timer, channel, 0U);
  (void)HAL_TIM_PWM_Start(timer, channel);
}

void ConveyorServo_SetPulseUs(ConveyorServo *servo, uint16_t pulse_us)
{
  if ((servo == NULL) || (servo->timer == NULL))
  {
    return;
  }
  if (pulse_us < SERVO_PULSE_US_MIN)
  {
    pulse_us = SERVO_PULSE_US_MIN;
  }
  else if (pulse_us > SERVO_PULSE_US_MAX)
  {
    pulse_us = SERVO_PULSE_US_MAX;
  }
  __HAL_TIM_SET_COMPARE(servo->timer, servo->channel, pulse_us);
  servo->pulse_us = pulse_us;
}
