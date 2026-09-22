#include "conveyor_sequencer.h"

#include "conveyor_config.h"
#include "process_interlock.h"

#include <stddef.h>
#include <string.h>

#define CONVEYOR_IR_DEBOUNCE_TICKS 3U

static uint8_t is_fault_state(ConveyorSequenceState state)
{
  return ((state == CONVEYOR_SEQ_ESTOP)
          || (state == CONVEYOR_SEQ_MOTOR_FAULT)
          || (state == CONVEYOR_SEQ_PCB_TIMEOUT)
          || (state == CONVEYOR_SEQ_HEATER_FAULT)) ? 1U : 0U;
}

static void enter_state(ConveyorSequencer *sequence,
                        ConveyorSequenceState state, uint32_t now)
{
  sequence->state = state;
  sequence->state_started_at = now;
}

static uint8_t timed_out(const ConveyorSequencer *sequence, uint32_t now,
                         uint32_t timeout_ms)
{
  return ((timeout_ms != 0U)
          && ((now - sequence->state_started_at) >= timeout_ms)) ? 1U : 0U;
}

static void release_conveyor_lock(ConveyorSequencer *sequence)
{
  if (sequence->conveyor_lock_held != 0U)
  {
    ProcessInterlock_Give(PROCESS_OWNER_CONVEYOR);
    sequence->conveyor_lock_held = 0U;
  }
}

static uint8_t take_conveyor_lock(ConveyorSequencer *sequence)
{
  if (sequence->conveyor_lock_held != 0U)
  {
    return 1U;
  }
  if (ProcessInterlock_Take(PROCESS_OWNER_CONVEYOR, 0U) == 0U)
  {
    return 0U;
  }
  sequence->conveyor_lock_held = 1U;
  return 1U;
}

static void enter_fault(ConveyorSequencer *sequence,
                        ConveyorSequenceState fault, uint32_t now)
{
  Conveyor_EmergencyStop(&sequence->motion);
  ConveyorServo_SetPulseUs(&sequence->servo,
                           sequence->config.servo_center_us);
  release_conveyor_lock(sequence);
  enter_state(sequence, fault, now);
}

void ConveyorSequencer_Init(ConveyorSequencer *sequence,
                            const ConveyorSequenceConfig *config)
{
  if ((sequence == NULL) || (config == NULL))
  {
    return;
  }
  memset(sequence, 0, sizeof(*sequence));
  sequence->config = *config;
  Conveyor_Init(&sequence->motion, CONVEYOR_PWM_TIM, CONVEYOR_PWM_CHANNEL,
                CONVEYOR_MOTOR_MIN_PWM, CONVEYOR_ENCODER_SLOTS);
  PcbSensor_Init(&sequence->ir_sensor, CONVEYOR_IR_PORT, CONVEYOR_IR_PIN,
                 CONVEYOR_IR_ACTIVE_LOW, CONVEYOR_IR_DEBOUNCE_TICKS);
  ConveyorServo_Init(&sequence->servo, CONVEYOR_SERVO_TIM,
                     CONVEYOR_SERVO_CHANNEL);
  ConveyorServo_SetPulseUs(&sequence->servo, config->servo_center_us);
  sequence->state = CONVEYOR_SEQ_IDLE;
}

void ConveyorSequencer_RequestStart(ConveyorSequencer *sequence)
{
  if ((sequence != NULL) && (sequence->state == CONVEYOR_SEQ_IDLE))
  {
    sequence->start_requested = 1U;
  }
}

void ConveyorSequencer_RequestAbort(ConveyorSequencer *sequence)
{
  if ((sequence != NULL) && (sequence->state != CONVEYOR_SEQ_IDLE))
  {
    sequence->abort_requested = 1U;
  }
}

void ConveyorSequencer_NotifyHeaterDone(ConveyorSequencer *sequence,
                                        uint8_t success)
{
  if (sequence == NULL)
  {
    return;
  }
  if (success != 0U)
  {
    sequence->heater_done = 1U;
  }
  else
  {
    sequence->heater_failed = 1U;
  }
}

void ConveyorSequencer_NotifyInspection(ConveyorSequencer *sequence,
                                        uint8_t pass)
{
  if ((sequence == NULL) || (sequence->state != CONVEYOR_SEQ_INSPECTION))
  {
    return;
  }
  sequence->inspection_pass = (pass != 0U) ? 1U : 0U;
  sequence->inspection_done = 1U;
}

void ConveyorSequencer_AcknowledgeFault(ConveyorSequencer *sequence)
{
  if ((sequence == NULL) || (is_fault_state(sequence->state) == 0U))
  {
    return;
  }
  Conveyor_Reset(&sequence->motion);
  sequence->abort_requested = 0U;
  sequence->start_requested = 0U;
  sequence->heater_done = 0U;
  sequence->heater_failed = 0U;
  sequence->inspection_done = 0U;
  sequence->last_result = CONVEYOR_RESULT_NONE;
  enter_state(sequence, CONVEYOR_SEQ_IDLE, HAL_GetTick());
}

void ConveyorSequencer_Update(ConveyorSequencer *sequence, uint32_t now)
{
  if (sequence == NULL)
  {
    return;
  }
  sequence->ir_detected = PcbSensor_Update(&sequence->ir_sensor);

  if (sequence->abort_requested != 0U)
  {
    sequence->abort_requested = 0U;
    enter_fault(sequence, CONVEYOR_SEQ_ESTOP, now);
    return;
  }

  switch (sequence->state)
  {
    case CONVEYOR_SEQ_IDLE:
      if (sequence->start_requested != 0U)
      {
        sequence->start_requested = 0U;
        sequence->last_result = CONVEYOR_RESULT_NONE;
        enter_state(sequence, CONVEYOR_SEQ_START_REQUESTED, now);
      }
      break;

    case CONVEYOR_SEQ_START_REQUESTED:
      if (take_conveyor_lock(sequence) != 0U)
      {
        Conveyor_Move(&sequence->motion, sequence->config.speed_percent,
                      sequence->config.heater_position_pulses);
        enter_state(sequence, CONVEYOR_SEQ_MOVING_TO_HEATER, now);
      }
      break;

    case CONVEYOR_SEQ_MOVING_TO_HEATER:
      if (sequence->motion.state == CONVEYOR_MOTION_TARGET_REACHED)
      {
        release_conveyor_lock(sequence);
        enter_state(sequence, CONVEYOR_SEQ_HEATING_WAIT, now);
      }
      else if (sequence->motion.state == CONVEYOR_MOTION_ERROR)
      {
        enter_fault(sequence, CONVEYOR_SEQ_MOTOR_FAULT, now);
      }
      else if (timed_out(sequence, now, sequence->config.move_timeout_ms))
      {
        enter_fault(sequence, CONVEYOR_SEQ_PCB_TIMEOUT, now);
      }
      break;

    case CONVEYOR_SEQ_HEATING_WAIT:
      if (sequence->heater_failed != 0U)
      {
        sequence->heater_failed = 0U;
        enter_fault(sequence, CONVEYOR_SEQ_HEATER_FAULT, now);
      }
      else if ((sequence->heater_done != 0U)
               && (take_conveyor_lock(sequence) != 0U))
      {
        sequence->heater_done = 0U;
        Conveyor_Move(&sequence->motion, sequence->config.speed_percent, 0U);
        enter_state(sequence, CONVEYOR_SEQ_MOVING_TO_INSPECTION, now);
      }
      else if (timed_out(sequence, now, sequence->config.heater_timeout_ms))
      {
        enter_fault(sequence, CONVEYOR_SEQ_HEATER_FAULT, now);
      }
      break;

    case CONVEYOR_SEQ_MOVING_TO_INSPECTION:
      if (sequence->motion.state == CONVEYOR_MOTION_ERROR)
      {
        enter_fault(sequence, CONVEYOR_SEQ_MOTOR_FAULT, now);
      }
      else if (sequence->ir_detected != 0U)
      {
        Conveyor_Stop(&sequence->motion);
        release_conveyor_lock(sequence);
        sequence->inspection_done = 0U;
        enter_state(sequence, CONVEYOR_SEQ_INSPECTION, now);
      }
      else if (timed_out(sequence, now, sequence->config.ir_timeout_ms))
      {
        enter_fault(sequence, CONVEYOR_SEQ_PCB_TIMEOUT, now);
      }
      break;

    case CONVEYOR_SEQ_INSPECTION:
      if (sequence->inspection_done != 0U)
      {
        sequence->inspection_done = 0U;
        if (sequence->inspection_pass != 0U)
        {
          sequence->last_result = CONVEYOR_RESULT_PASS;
          ConveyorServo_SetPulseUs(&sequence->servo,
                                   sequence->config.servo_right_us);
          enter_state(sequence, CONVEYOR_SEQ_SERVO_RIGHT, now);
        }
        else
        {
          sequence->last_result = CONVEYOR_RESULT_FAIL;
          ConveyorServo_SetPulseUs(&sequence->servo,
                                   sequence->config.servo_left_us);
          enter_state(sequence, CONVEYOR_SEQ_SERVO_LEFT, now);
        }
      }
      else if (timed_out(sequence, now,
                         sequence->config.inspection_timeout_ms))
      {
        sequence->last_result = CONVEYOR_RESULT_TIMEOUT;
        ConveyorServo_SetPulseUs(&sequence->servo,
                                 sequence->config.servo_left_us);
        enter_state(sequence, CONVEYOR_SEQ_SERVO_LEFT, now);
      }
      break;

    case CONVEYOR_SEQ_SERVO_RIGHT:
    case CONVEYOR_SEQ_SERVO_LEFT:
      if (timed_out(sequence, now, sequence->config.servo_step_ms))
      {
        ConveyorServo_SetPulseUs(&sequence->servo,
                                 sequence->config.servo_center_us);
        enter_state(sequence, CONVEYOR_SEQ_SERVO_CENTER, now);
      }
      break;

    case CONVEYOR_SEQ_SERVO_CENTER:
      if (timed_out(sequence, now, sequence->config.servo_step_ms))
      {
        enter_state(sequence, CONVEYOR_SEQ_IDLE, now);
      }
      break;

    case CONVEYOR_SEQ_ESTOP:
    case CONVEYOR_SEQ_MOTOR_FAULT:
    case CONVEYOR_SEQ_PCB_TIMEOUT:
    case CONVEYOR_SEQ_HEATER_FAULT:
    default:
      break;
  }
}

const char *ConveyorSequencer_StateName(ConveyorSequenceState state)
{
  switch (state)
  {
    case CONVEYOR_SEQ_START_REQUESTED: return "START";
    case CONVEYOR_SEQ_MOVING_TO_HEATER: return "TO-MID";
    case CONVEYOR_SEQ_HEATING_WAIT: return "HEAT-5S";
    case CONVEYOR_SEQ_MOVING_TO_INSPECTION: return "TO-END";
    case CONVEYOR_SEQ_INSPECTION: return "INSPECT";
    case CONVEYOR_SEQ_SERVO_RIGHT: return "SWIPE-R";
    case CONVEYOR_SEQ_SERVO_LEFT: return "SWIPE-L";
    case CONVEYOR_SEQ_SERVO_CENTER: return "CENTER";
    case CONVEYOR_SEQ_ESTOP: return "ESTOP";
    case CONVEYOR_SEQ_MOTOR_FAULT: return "MOTOR-ERR";
    case CONVEYOR_SEQ_PCB_TIMEOUT: return "PCB-TIME";
    case CONVEYOR_SEQ_HEATER_FAULT: return "HEAT-ERR";
    case CONVEYOR_SEQ_IDLE:
    default: return "IDLE";
  }
}
