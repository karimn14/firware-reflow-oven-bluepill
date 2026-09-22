#pragma once
#include <stdint.h>
typedef enum { CONVEYOR_SEQ_IDLE = 0, CONVEYOR_SEQ_START_REQUESTED, CONVEYOR_SEQ_MOVING_TO_HEATER,
  CONVEYOR_SEQ_HEATING_WAIT, CONVEYOR_SEQ_MOVING_TO_INSPECTION, CONVEYOR_SEQ_INSPECTION,
  CONVEYOR_SEQ_SERVO_RIGHT, CONVEYOR_SEQ_SERVO_LEFT, CONVEYOR_SEQ_SERVO_CENTER, CONVEYOR_SEQ_ESTOP,
  CONVEYOR_SEQ_MOTOR_FAULT, CONVEYOR_SEQ_PCB_TIMEOUT, CONVEYOR_SEQ_HEATER_FAULT } ConveyorSequenceState;
typedef enum { CONVEYOR_RESULT_NONE = 0, CONVEYOR_RESULT_PASS, CONVEYOR_RESULT_FAIL, CONVEYOR_RESULT_TIMEOUT } ConveyorInspectionResult;
typedef struct { ConveyorSequenceState state; ConveyorInspectionResult last_result; uint8_t motor_percent; uint8_t ir_detected; } ConveyorAppStatus;
void ConveyorApp_GetStatus(ConveyorAppStatus *status);
void ConveyorApp_ManualInspectionResult(uint8_t pass);
