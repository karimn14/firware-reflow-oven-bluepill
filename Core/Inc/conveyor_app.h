#ifndef CONVEYOR_APP_H
#define CONVEYOR_APP_H

#include "conveyor_sequencer.h"
#include "process_interlock.h"

#include <stdint.h>

typedef struct
{
  ConveyorSequenceState state;
  ConveyorInspectionResult last_result;
  ProcessOwner process_owner;
  uint32_t current_pulses;
  uint32_t target_pulses;
  uint32_t state_elapsed_ms;
  uint16_t servo_pulse_us;
  uint8_t speed_percent;
  uint8_t motor_percent;
  uint8_t manual_test_active;
  uint8_t ir_detected;
} ConveyorAppStatus;

void ConveyorApp_Init(void);
void ConveyorApp_Task(void *argument);
uint8_t ConveyorApp_RequestStart(void);
void ConveyorApp_RequestAbort(void);
void ConveyorApp_AcknowledgeFault(void);
void ConveyorApp_AdjustSpeed(int8_t steps);
uint8_t ConveyorApp_ManualMotorStart(uint8_t duty_percent);
void ConveyorApp_ManualMotorSetDuty(uint8_t duty_percent);
void ConveyorApp_ManualMotorStop(void);
void ConveyorApp_ManualInspectionResult(uint8_t pass);
void ConveyorApp_CenterServo(void);
void ConveyorApp_GetStatus(ConveyorAppStatus *status);
ConveyorSequencer *ConveyorApp_GetSequencer(void);

#endif /* CONVEYOR_APP_H */
