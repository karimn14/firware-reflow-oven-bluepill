#include "encoder.h"

#include <stddef.h>

void SingleEncoder_Init(SingleChannelEncoder *encoder,
                        TIM_HandleTypeDef *timer, uint32_t slots_per_rev)
{
  if ((encoder == NULL) || (timer == NULL))
  {
    return;
  }
  encoder->timer = timer;
  encoder->slots_per_rev = slots_per_rev;
  __HAL_TIM_SET_COUNTER(timer, 0U);
  encoder->last_counter = 0U;
  encoder->last_delta_ticks = 0U;
  (void)HAL_TIM_Base_Start(timer);
}

uint16_t SingleEncoder_ReadDelta(SingleChannelEncoder *encoder)
{
  uint16_t current;
  uint16_t delta;

  if ((encoder == NULL) || (encoder->timer == NULL))
  {
    return 0U;
  }
  current = (uint16_t)__HAL_TIM_GET_COUNTER(encoder->timer);
  /* Unsigned 16-bit subtraction also handles one TIM1 counter wrap between
   * reads. The conveyor task polls every 10 ms, so a second wrap is not
   * physically plausible for this encoder. */
  delta = (uint16_t)(current - encoder->last_counter);
  encoder->last_counter = current;
  encoder->last_delta_ticks = delta;
  return encoder->last_delta_ticks;
}
