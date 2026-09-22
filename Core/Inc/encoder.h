#ifndef CONVEYOR_ENCODER_H
#define CONVEYOR_ENCODER_H

#include <stdint.h>

typedef struct
{
  uint32_t slots_per_rev;
  uint32_t last_counter;
  uint16_t last_delta_ticks;
} SingleChannelEncoder;

void SingleEncoder_Init(SingleChannelEncoder *encoder, uint32_t slots_per_rev);
uint16_t SingleEncoder_ReadDelta(SingleChannelEncoder *encoder);
void ConveyorEncoder_OnPulseISR(void);
uint32_t ConveyorEncoder_GetTotal(void);

#endif /* CONVEYOR_ENCODER_H */
