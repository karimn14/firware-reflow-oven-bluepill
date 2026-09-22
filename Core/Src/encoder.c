#include "encoder.h"

#include <stddef.h>

static volatile uint32_t encoder_pulse_count;

void SingleEncoder_Init(SingleChannelEncoder *encoder, uint32_t slots_per_rev)
{
  if (encoder == NULL)
  {
    return;
  }
  encoder->slots_per_rev = slots_per_rev;
  encoder->last_counter = encoder_pulse_count;
  encoder->last_delta_ticks = 0U;
}

uint16_t SingleEncoder_ReadDelta(SingleChannelEncoder *encoder)
{
  uint32_t current;
  uint32_t delta;

  if (encoder == NULL)
  {
    return 0U;
  }
  current = encoder_pulse_count;
  delta = current - encoder->last_counter;
  encoder->last_counter = current;
  if (delta > UINT16_MAX)
  {
    delta = UINT16_MAX;
  }
  encoder->last_delta_ticks = (uint16_t)delta;
  return encoder->last_delta_ticks;
}

void ConveyorEncoder_OnPulseISR(void)
{
  ++encoder_pulse_count;
}

uint32_t ConveyorEncoder_GetTotal(void)
{
  return encoder_pulse_count;
}
