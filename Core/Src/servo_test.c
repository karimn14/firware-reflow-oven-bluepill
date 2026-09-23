#include "servo.h"
#include "tim.h"

/* Standalone servo check: PA2 / TIM2 CH3, 50 Hz. */
void ServoTest_Run(void)
{
  ConveyorServo servo;

  ConveyorServo_Init(&servo, &htim2, TIM_CHANNEL_3);
  ConveyorServo_SetPulseUs(&servo, 1500U);
  HAL_Delay(2000U);

  for (;;)
  {
    ConveyorServo_SetPulseUs(&servo, 1000U);
    HAL_Delay(2000U);
    ConveyorServo_SetPulseUs(&servo, 1500U);
    HAL_Delay(2000U);
    ConveyorServo_SetPulseUs(&servo, 2000U);
    HAL_Delay(2000U);
    ConveyorServo_SetPulseUs(&servo, 1500U);
    HAL_Delay(2000U);
  }
}
