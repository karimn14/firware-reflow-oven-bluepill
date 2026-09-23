#include "conveyor_app.h"

#include "FreeRTOS.h"
#include "conveyor_config.h"
#include "hardware_test.h"
#include "heater_characterization.h"
#include "reflow.h"
#include "task.h"
#include "watchdog.h"

#include <stddef.h>

static ConveyorSequencer conveyor_sequence;
static uint8_t heating_seen_running;
static volatile uint8_t full_reflow_requested;
static volatile uint8_t manual_motor_test_active;
static volatile uint8_t manual_motor_test_duty;
static volatile uint8_t manual_servo_test_active;

void ConveyorApp_Init(void)
{
  const ConveyorSequenceConfig config = {
    .speed_percent = CONVEYOR_DEFAULT_SPEED_PWM,
    .heater_position_pulses = CONVEYOR_HEATER_POSITION_PULSES,
    .move_timeout_ms = CONVEYOR_MOVE_TIMEOUT_MS,
    .heater_timeout_ms = CONVEYOR_HEATER_TIMEOUT_MS,
    .ir_timeout_ms = CONVEYOR_IR_TIMEOUT_MS,
    .inspection_timeout_ms = CONVEYOR_INSPECTION_FALLBACK_MS,
    .servo_left_us = CONVEYOR_SERVO_LEFT_US,
    .servo_center_us = CONVEYOR_SERVO_CENTER_US,
    .servo_right_us = CONVEYOR_SERVO_RIGHT_US,
    .servo_step_ms = CONVEYOR_SERVO_STEP_MS
  };

  ConveyorSequencer_Init(&conveyor_sequence, &config);
  heating_seen_running = 0U;
  full_reflow_requested = 0U;
  manual_motor_test_active = 0U;
  manual_motor_test_duty = 0U;
  manual_servo_test_active = 0U;
}

static void update_heater_handshake(ConveyorSequenceState previous_state)
{
  ReflowStatus reflow;

  if ((conveyor_sequence.state == CONVEYOR_SEQ_HEATING_WAIT)
      && (previous_state != CONVEYOR_SEQ_HEATING_WAIT))
  {
    heating_seen_running = 0U;
    if (full_reflow_requested != 0U)
    {
      Reflow_RequestStart();
    }
    else if (Reflow_RequestTimedTest(CONVEYOR_E2E_HEATING_DURATION_MS,
                                     CONVEYOR_E2E_HEATER_DUTY_PERCENT) == 0U)
    {
      ConveyorSequencer_NotifyHeaterDone(&conveyor_sequence, 0U);
    }
  }
  if (conveyor_sequence.state != CONVEYOR_SEQ_HEATING_WAIT)
  {
    return;
  }

  if (Reflow_IsRunning() != 0U)
  {
    heating_seen_running = 1U;
    return;
  }
  Reflow_GetStatus(&reflow);
  if (heating_seen_running != 0U)
  {
    heating_seen_running = 0U;
    ConveyorSequencer_NotifyHeaterDone(&conveyor_sequence,
                                       (reflow.fault == 0U) ? 1U : 0U);
  }
  else if ((HAL_GetTick() - conveyor_sequence.state_started_at > 1000UL)
           && (reflow.fault != 0U))
  {
    ConveyorSequencer_NotifyHeaterDone(&conveyor_sequence, 0U);
  }
}

void ConveyorApp_Task(void *argument)
{
  TickType_t last_wake = xTaskGetTickCount();
  uint32_t sequence_elapsed = CONVEYOR_SEQUENCE_INTERVAL_MS;

  (void)argument;
  for (;;)
  {
    taskENTER_CRITICAL();
    if (manual_motor_test_active != 0U)
    {
      ConveyorMotor_SetOutput(&conveyor_sequence.motion.motor,
                              manual_motor_test_duty);
      conveyor_sequence.motion.current_pulses +=
          SingleEncoder_ReadDelta(&conveyor_sequence.motion.encoder);
    }
    else
    {
      Conveyor_Update(&conveyor_sequence.motion);
    }
    taskEXIT_CRITICAL();

    if (manual_motor_test_active == 0U)
    {
      sequence_elapsed += CONVEYOR_TASK_INTERVAL_MS;
      if (sequence_elapsed >= CONVEYOR_SEQUENCE_INTERVAL_MS)
      {
        ConveyorSequenceState before_update = conveyor_sequence.state;
        sequence_elapsed = 0U;
        ConveyorSequencer_Update(&conveyor_sequence, HAL_GetTick());
        update_heater_handshake(before_update);
        if ((before_update == CONVEYOR_SEQ_HEATING_WAIT)
            && (conveyor_sequence.state == CONVEYOR_SEQ_HEATER_FAULT))
        {
          Reflow_RequestStop();
          HardwareTest_HeaterStop();
        }
      }
    }
    Watchdog_Heartbeat(WATCHDOG_ID_CONVEYOR);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CONVEYOR_TASK_INTERVAL_MS));
  }
}

static uint8_t request_start(uint8_t full_reflow)
{
  HardwareTestStatus hardware;

  HardwareTest_GetStatus(&hardware);
  if ((conveyor_sequence.state != CONVEYOR_SEQ_IDLE)
      || (ProcessInterlock_GetOwner() != PROCESS_OWNER_NONE)
      || (hardware.heater_enabled != 0U)
      || (hardware.pid_running != 0U)
      || (Reflow_IsRunning() != 0U)
      || (HeaterCharacterization_IsRunning() != 0U))
  {
    return 0U;
  }
  full_reflow_requested = full_reflow;
  conveyor_sequence.config.heater_timeout_ms = (full_reflow != 0U)
      ? CONVEYOR_REFLOW_HEATER_TIMEOUT_MS : CONVEYOR_HEATER_TIMEOUT_MS;
  ConveyorSequencer_RequestStart(&conveyor_sequence);
  return 1U;
}

uint8_t ConveyorApp_RequestStart(void)
{
  return request_start(0U);
}

uint8_t ConveyorApp_RequestStartProfile(void)
{
  return request_start(1U);
}

void ConveyorApp_RequestAbort(void)
{
  Reflow_RequestStop();
  HardwareTest_HeaterStop();
  ConveyorSequencer_RequestAbort(&conveyor_sequence);
}

void ConveyorApp_AcknowledgeFault(void)
{
  ConveyorSequencer_AcknowledgeFault(&conveyor_sequence);
}

void ConveyorApp_AdjustSpeed(int8_t steps)
{
  int16_t adjusted;

  if (conveyor_sequence.state != CONVEYOR_SEQ_IDLE)
  {
    return;
  }
  adjusted = (int16_t)conveyor_sequence.config.speed_percent
             + (int16_t)steps * CONVEYOR_SPEED_STEP_PWM;
  if (adjusted < CONVEYOR_MIN_SPEED_PWM)
  {
    adjusted = CONVEYOR_MIN_SPEED_PWM;
  }
  else if (adjusted > 100)
  {
    adjusted = 100;
  }
  conveyor_sequence.config.speed_percent = (uint8_t)adjusted;
}

uint8_t ConveyorApp_ManualMotorStart(uint8_t duty_percent)
{
  HardwareTestStatus hardware;

  HardwareTest_GetStatus(&hardware);
  if ((manual_motor_test_active != 0U)
      || (conveyor_sequence.state != CONVEYOR_SEQ_IDLE)
      || (ProcessInterlock_GetOwner() != PROCESS_OWNER_NONE)
      || (hardware.heater_enabled != 0U)
      || (hardware.pid_running != 0U)
      || (Reflow_IsRunning() != 0U)
      || (HeaterCharacterization_IsRunning() != 0U)
      || (duty_percent == 0U) || (duty_percent > 100U)
      || (ProcessInterlock_Take(PROCESS_OWNER_CONVEYOR, 0U) == 0U))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  conveyor_sequence.motion.current_pulses = 0U;
  conveyor_sequence.motion.target_pulses = 0U;
  (void)SingleEncoder_ReadDelta(&conveyor_sequence.motion.encoder);
  conveyor_sequence.motion.state = CONVEYOR_MOTION_MOVING;
  manual_motor_test_duty = duty_percent;
  manual_motor_test_active = 1U;
  ConveyorMotor_SetOutput(&conveyor_sequence.motion.motor, duty_percent);
  taskEXIT_CRITICAL();
  return 1U;
}

void ConveyorApp_ManualMotorSetDuty(uint8_t duty_percent)
{
  if (manual_motor_test_active == 0U)
  {
    return;
  }
  taskENTER_CRITICAL();
  manual_motor_test_duty = duty_percent;
  ConveyorMotor_SetOutput(&conveyor_sequence.motion.motor, duty_percent);
  taskEXIT_CRITICAL();
}

void ConveyorApp_ManualMotorStop(void)
{
  if (manual_motor_test_active == 0U)
  {
    return;
  }
  taskENTER_CRITICAL();
  ConveyorMotor_SetOutput(&conveyor_sequence.motion.motor, 0U);
  conveyor_sequence.motion.state = CONVEYOR_MOTION_STOPPED;
  manual_motor_test_duty = 0U;
  manual_motor_test_active = 0U;
  taskEXIT_CRITICAL();
  ProcessInterlock_Give(PROCESS_OWNER_CONVEYOR);
}

uint8_t ConveyorApp_ManualServoStart(uint16_t pulse_us)
{
  HardwareTestStatus hardware;

  HardwareTest_GetStatus(&hardware);
  if ((manual_servo_test_active != 0U)
      || (conveyor_sequence.state != CONVEYOR_SEQ_IDLE)
      || (ProcessInterlock_GetOwner() != PROCESS_OWNER_NONE)
      || (hardware.heater_enabled != 0U)
      || (hardware.pid_running != 0U)
      || (Reflow_IsRunning() != 0U)
      || (HeaterCharacterization_IsRunning() != 0U)
      || (ProcessInterlock_Take(PROCESS_OWNER_CONVEYOR, 0U) == 0U))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  manual_servo_test_active = 1U;
  ConveyorServo_SetPulseUs(&conveyor_sequence.servo, pulse_us);
  taskEXIT_CRITICAL();
  return 1U;
}

void ConveyorApp_ManualServoSetPulse(uint16_t pulse_us)
{
  if (manual_servo_test_active == 0U)
  {
    return;
  }
  taskENTER_CRITICAL();
  ConveyorServo_SetPulseUs(&conveyor_sequence.servo, pulse_us);
  taskEXIT_CRITICAL();
}

void ConveyorApp_ManualServoStop(void)
{
  if (manual_servo_test_active == 0U)
  {
    return;
  }
  taskENTER_CRITICAL();
  ConveyorServo_SetPulseUs(&conveyor_sequence.servo,
                           conveyor_sequence.config.servo_center_us);
  manual_servo_test_active = 0U;
  taskEXIT_CRITICAL();
  ProcessInterlock_Give(PROCESS_OWNER_CONVEYOR);
}

void ConveyorApp_ManualInspectionResult(uint8_t pass)
{
  ConveyorSequencer_NotifyInspection(&conveyor_sequence, pass);
}

void ConveyorApp_CenterServo(void)
{
  if ((conveyor_sequence.state == CONVEYOR_SEQ_IDLE)
      && (manual_servo_test_active == 0U))
  {
    ConveyorServo_SetPulseUs(&conveyor_sequence.servo,
                             conveyor_sequence.config.servo_center_us);
  }
}

void ConveyorApp_GetStatus(ConveyorAppStatus *status)
{
  if (status == NULL)
  {
    return;
  }
  taskENTER_CRITICAL();
  status->state = conveyor_sequence.state;
  status->last_result = conveyor_sequence.last_result;
  status->process_owner = ProcessInterlock_GetOwner();
  status->current_pulses = conveyor_sequence.motion.current_pulses;
  status->target_pulses = conveyor_sequence.motion.target_pulses;
  status->state_elapsed_ms = HAL_GetTick()
                             - conveyor_sequence.state_started_at;
  status->servo_pulse_us = conveyor_sequence.servo.pulse_us;
  status->speed_percent = conveyor_sequence.config.speed_percent;
  status->motor_percent = conveyor_sequence.motion.motor.duty_percent;
  status->manual_test_active = manual_motor_test_active;
  status->manual_servo_test_active = manual_servo_test_active;
  status->ir_detected = conveyor_sequence.ir_detected;
  taskEXIT_CRITICAL();
}

ConveyorSequencer *ConveyorApp_GetSequencer(void)
{
  return &conveyor_sequence;
}
