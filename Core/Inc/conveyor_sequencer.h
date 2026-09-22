#ifndef CONVEYOR_SEQUENCER_H
#define CONVEYOR_SEQUENCER_H

#include "conveyor.h"
#include "pcb_sensor.h"
#include "servo.h"

#include <stdint.h>

typedef enum
{
  CONVEYOR_SEQ_IDLE = 0,
  CONVEYOR_SEQ_START_REQUESTED,
  CONVEYOR_SEQ_MOVING_TO_HEATER,
  CONVEYOR_SEQ_HEATING_WAIT,
  CONVEYOR_SEQ_MOVING_TO_INSPECTION,
  CONVEYOR_SEQ_INSPECTION,
  CONVEYOR_SEQ_SERVO_RIGHT,
  CONVEYOR_SEQ_SERVO_LEFT,
  CONVEYOR_SEQ_SERVO_CENTER,
  CONVEYOR_SEQ_ESTOP,
  CONVEYOR_SEQ_MOTOR_FAULT,
  CONVEYOR_SEQ_PCB_TIMEOUT,
  CONVEYOR_SEQ_HEATER_FAULT
} ConveyorSequenceState;

typedef enum
{
  CONVEYOR_RESULT_NONE = 0,
  CONVEYOR_RESULT_PASS,
  CONVEYOR_RESULT_FAIL,
  CONVEYOR_RESULT_TIMEOUT
} ConveyorInspectionResult;

typedef struct
{
  uint8_t speed_percent;
  uint32_t heater_position_pulses;
  uint32_t move_timeout_ms;
  uint32_t heater_timeout_ms;
  uint32_t ir_timeout_ms;
  uint32_t inspection_timeout_ms;
  uint16_t servo_left_us;
  uint16_t servo_center_us;
  uint16_t servo_right_us;
  uint32_t servo_step_ms;
} ConveyorSequenceConfig;

typedef struct
{
  ConveyorMotion motion;
  PcbSensor ir_sensor;
  ConveyorServo servo;
  ConveyorSequenceConfig config;
  volatile ConveyorSequenceState state;
  volatile uint8_t start_requested;
  volatile uint8_t abort_requested;
  volatile uint8_t heater_done;
  volatile uint8_t heater_failed;
  volatile uint8_t inspection_done;
  volatile uint8_t inspection_pass;
  uint8_t conveyor_lock_held;
  uint8_t ir_detected;
  ConveyorInspectionResult last_result;
  uint32_t state_started_at;
} ConveyorSequencer;

void ConveyorSequencer_Init(ConveyorSequencer *sequence,
                            const ConveyorSequenceConfig *config);
void ConveyorSequencer_Update(ConveyorSequencer *sequence, uint32_t now);
void ConveyorSequencer_RequestStart(ConveyorSequencer *sequence);
void ConveyorSequencer_RequestAbort(ConveyorSequencer *sequence);
void ConveyorSequencer_NotifyHeaterDone(ConveyorSequencer *sequence,
                                        uint8_t success);
void ConveyorSequencer_NotifyInspection(ConveyorSequencer *sequence,
                                        uint8_t pass);
void ConveyorSequencer_AcknowledgeFault(ConveyorSequencer *sequence);
const char *ConveyorSequencer_StateName(ConveyorSequenceState state);

#endif /* CONVEYOR_SEQUENCER_H */
