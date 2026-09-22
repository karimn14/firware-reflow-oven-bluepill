#include "motor.h"

#include <stddef.h>

void ConveyorMotor_Init(ConveyorMotor *motor, TIM_HandleTypeDef *timer,
                        uint32_t channel, uint8_t minimum_duty_percent)
{
  if ((motor == NULL) || (timer == NULL))
  {
    return;
  }
  motor->timer = timer;
  motor->channel = channel;
  motor->minimum_duty_percent = minimum_duty_percent;
  motor->duty_percent = 0U;
  __HAL_TIM_SET_COMPARE(timer, channel, 0U);
  (void)HAL_TIM_PWM_Start(timer, channel);
}

void ConveyorMotor_SetOutput(ConveyorMotor *motor, uint8_t duty_percent)
{
  uint32_t period;
  uint32_t compare;

  if ((motor == NULL) || (motor->timer == NULL))
  {
    return;
  }
  if (duty_percent > 100U)
  {
    duty_percent = 100U;
  }
  if ((duty_percent > 0U)
      && (duty_percent < motor->minimum_duty_percent))
  {
    duty_percent = motor->minimum_duty_percent;
  }
  period = __HAL_TIM_GET_AUTORELOAD(motor->timer) + 1UL;
  compare = period * duty_percent / 100UL;
  __HAL_TIM_SET_COMPARE(motor->timer, motor->channel, compare);
  motor->duty_percent = duty_percent;
}
