#include "heater_characterization.h"

#include "FreeRTOS.h"
#include "hardware_test.h"
#include "main.h"
#include "task.h"

#include <stddef.h>

#define CHARACTERIZATION_DEFAULT_DUTY_PERCENT       25U
#define CHARACTERIZATION_DUTY_STEP_PERCENT          25U
#define CHARACTERIZATION_CUTOFF_TENTHS            1100
#define CHARACTERIZATION_MAX_START_TENTHS          500
#define CHARACTERIZATION_COOL_END_DELTA_TENTHS      30
#define CHARACTERIZATION_RATE_INTERVAL_MS        10000UL
#define CHARACTERIZATION_LOG_INTERVAL_MS          1000UL
#define CHARACTERIZATION_HEATING_TIMEOUT_MS     900000UL
#define CHARACTERIZATION_COOLING_TIMEOUT_MS    1800000UL

static volatile HeaterCharacterizationState characterization_state;
static volatile HeaterCharacterizationFault characterization_fault;
static volatile uint8_t duty_percent;
static volatile uint8_t start_requested;
static volatile uint8_t stop_requested;
static uint32_t session_started_at;
static uint32_t state_started_at;
static uint32_t rate_reference_at;
static uint32_t last_log_at;
static int16_t rate_reference_temperature_tenths;
static int16_t start_temperature_tenths;
static int16_t peak_temperature_tenths;
static int16_t overshoot_tenths;
static int32_t rate_milli_c_per_s;
static int32_t average_rate_milli_c_per_s;
static volatile uint32_t log_session;
static volatile uint32_t log_sequence;
static HeaterCharacterizationLogSample latest_log;

const char *HeaterCharacterization_StateName(
    HeaterCharacterizationState state)
{
  switch (state)
  {
    case HEATER_CHARACTERIZATION_HEATING:
      return "HEATING";
    case HEATER_CHARACTERIZATION_COOLING:
      return "COOLING";
    case HEATER_CHARACTERIZATION_COMPLETE:
      return "COMPLETE";
    case HEATER_CHARACTERIZATION_FAULT:
      return "FAULT";
    case HEATER_CHARACTERIZATION_IDLE:
    default:
      return "IDLE";
  }
}

static int32_t calculate_rate(int16_t current_tenths,
                              int16_t reference_tenths,
                              uint32_t elapsed_ms)
{
  if (elapsed_ms == 0U)
  {
    return 0;
  }
  return ((int32_t)current_tenths - reference_tenths) * 100000L
         / (int32_t)elapsed_ms;
}

static void update_peak(int16_t temperature_tenths)
{
  if (temperature_tenths > peak_temperature_tenths)
  {
    peak_temperature_tenths = temperature_tenths;
  }
  overshoot_tenths = (peak_temperature_tenths > CHARACTERIZATION_CUTOFF_TENTHS)
                     ? (int16_t)(peak_temperature_tenths
                                 - CHARACTERIZATION_CUTOFF_TENTHS)
                     : 0;
}

static void publish_log(const HardwareTestStatus *hardware, uint32_t now)
{
  HeaterCharacterizationLogSample sample;

  sample.session = log_session;
  sample.sequence = log_sequence + 1U;
  sample.elapsed_ms = now - session_started_at;
  sample.rate_milli_c_per_s = rate_milli_c_per_s;
  sample.temperature_tenths = hardware->temperature_tenths;
  sample.peak_temperature_tenths = peak_temperature_tenths;
  sample.overshoot_tenths = overshoot_tenths;
  sample.state = characterization_state;
  sample.fault = characterization_fault;
  sample.duty_percent = duty_percent;
  sample.temperature_valid = hardware->temperature_valid;
  taskENTER_CRITICAL();
  latest_log = sample;
  log_sequence = sample.sequence;
  taskEXIT_CRITICAL();
  last_log_at = now;
}

static void enter_fault(HeaterCharacterizationFault fault,
                        const HardwareTestStatus *hardware,
                        uint32_t now)
{
  HardwareTest_HeaterStop();
  characterization_fault = fault;
  characterization_state = HEATER_CHARACTERIZATION_FAULT;
  publish_log(hardware, now);
}

static void start_characterization(const HardwareTestStatus *hardware,
                                   uint32_t now)
{
  ++log_session;
  log_sequence = 0U;
  session_started_at = now;
  state_started_at = now;
  last_log_at = now - CHARACTERIZATION_LOG_INTERVAL_MS;
  rate_reference_at = now;
  rate_reference_temperature_tenths = hardware->temperature_tenths;
  start_temperature_tenths = hardware->temperature_tenths;
  peak_temperature_tenths = hardware->temperature_tenths;
  overshoot_tenths = 0;
  rate_milli_c_per_s = 0;
  average_rate_milli_c_per_s = 0;
  characterization_fault = HEATER_CHARACTERIZATION_FAULT_NONE;

  if (hardware->temperature_valid == 0U)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_SENSOR, hardware, now);
  }
  else if (hardware->thermistor_calibrated == 0U)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_CALIBRATION, hardware, now);
  }
  else if (hardware->temperature_tenths > CHARACTERIZATION_MAX_START_TENTHS)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_START_HOT, hardware, now);
  }
  else if (HardwareTest_HeaterStartAtDuty(
             duty_percent, CHARACTERIZATION_CUTOFF_TENTHS) == 0U)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_SENSOR, hardware, now);
  }
  else
  {
    characterization_state = HEATER_CHARACTERIZATION_HEATING;
    publish_log(hardware, now);
  }
}

void HeaterCharacterization_Init(void)
{
  characterization_state = HEATER_CHARACTERIZATION_IDLE;
  characterization_fault = HEATER_CHARACTERIZATION_FAULT_NONE;
  duty_percent = CHARACTERIZATION_DEFAULT_DUTY_PERCENT;
  start_requested = 0U;
  stop_requested = 0U;
  log_session = 0U;
  log_sequence = 0U;
}

void HeaterCharacterization_RequestStart(void)
{
  if (HeaterCharacterization_IsRunning() == 0U)
  {
    start_requested = 1U;
  }
}

void HeaterCharacterization_RequestStop(void)
{
  if (HeaterCharacterization_IsRunning() != 0U)
  {
    stop_requested = 1U;
  }
}

void HeaterCharacterization_AdjustDuty(int8_t steps)
{
  int16_t adjusted;

  if (HeaterCharacterization_IsRunning() != 0U)
  {
    return;
  }
  adjusted = (int16_t)duty_percent
             + (int16_t)steps * CHARACTERIZATION_DUTY_STEP_PERCENT;
  if (adjusted < CHARACTERIZATION_DUTY_STEP_PERCENT)
  {
    adjusted = CHARACTERIZATION_DUTY_STEP_PERCENT;
  }
  else if (adjusted > 100)
  {
    adjusted = 100;
  }
  duty_percent = (uint8_t)adjusted;
}

uint8_t HeaterCharacterization_IsRunning(void)
{
  return ((characterization_state == HEATER_CHARACTERIZATION_HEATING)
          || (characterization_state == HEATER_CHARACTERIZATION_COOLING))
         ? 1U : 0U;
}

void HeaterCharacterization_GetStatus(HeaterCharacterizationStatus *status)
{
  if (status == NULL)
  {
    return;
  }
  status->state = characterization_state;
  status->fault = characterization_fault;
  status->elapsed_ms = HeaterCharacterization_IsRunning()
                       ? HAL_GetTick() - session_started_at
                       : latest_log.elapsed_ms;
  status->rate_milli_c_per_s = rate_milli_c_per_s;
  status->average_rate_milli_c_per_s = average_rate_milli_c_per_s;
  status->start_temperature_tenths = start_temperature_tenths;
  status->peak_temperature_tenths = peak_temperature_tenths;
  status->overshoot_tenths = overshoot_tenths;
  status->duty_percent = duty_percent;
}

uint8_t HeaterCharacterization_GetLatestLog(
    HeaterCharacterizationLogSample *sample)
{
  if ((sample == NULL) || (log_sequence == 0U))
  {
    return 0U;
  }
  taskENTER_CRITICAL();
  *sample = latest_log;
  taskEXIT_CRITICAL();
  return 1U;
}

void HeaterCharacterization_Process(void)
{
  HardwareTestStatus hardware;
  uint32_t now = HAL_GetTick();

  HardwareTest_GetStatus(&hardware);

  if (stop_requested != 0U)
  {
    stop_requested = 0U;
    start_requested = 0U;
    HardwareTest_HeaterStop();
    characterization_fault = HEATER_CHARACTERIZATION_FAULT_ABORTED;
    characterization_state = HEATER_CHARACTERIZATION_COMPLETE;
    publish_log(&hardware, now);
  }
  else if (start_requested != 0U)
  {
    start_requested = 0U;
    start_characterization(&hardware, now);
  }

  if (HeaterCharacterization_IsRunning() == 0U)
  {
    return;
  }
  if (hardware.temperature_valid == 0U)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_SENSOR, &hardware, now);
    return;
  }
  if (hardware.thermistor_calibrated == 0U)
  {
    enter_fault(HEATER_CHARACTERIZATION_FAULT_CALIBRATION, &hardware, now);
    return;
  }

  update_peak(hardware.temperature_tenths);
  if ((now - rate_reference_at) >= CHARACTERIZATION_RATE_INTERVAL_MS)
  {
    rate_milli_c_per_s = calculate_rate(
        hardware.temperature_tenths, rate_reference_temperature_tenths,
        now - rate_reference_at);
    rate_reference_at = now;
    rate_reference_temperature_tenths = hardware.temperature_tenths;
  }

  if (characterization_state == HEATER_CHARACTERIZATION_HEATING)
  {
    average_rate_milli_c_per_s = calculate_rate(
        hardware.temperature_tenths, start_temperature_tenths,
        now - session_started_at);
    if (hardware.temperature_tenths >= CHARACTERIZATION_CUTOFF_TENTHS)
    {
      HardwareTest_HeaterStop();
      characterization_state = HEATER_CHARACTERIZATION_COOLING;
      state_started_at = now;
      rate_reference_at = now;
      rate_reference_temperature_tenths = hardware.temperature_tenths;
      publish_log(&hardware, now);
    }
    else if ((now - state_started_at)
             >= CHARACTERIZATION_HEATING_TIMEOUT_MS)
    {
      HardwareTest_HeaterStop();
      characterization_fault = HEATER_CHARACTERIZATION_FAULT_HEATING_TIMEOUT;
      characterization_state = HEATER_CHARACTERIZATION_COOLING;
      state_started_at = now;
      rate_reference_at = now;
      rate_reference_temperature_tenths = hardware.temperature_tenths;
      publish_log(&hardware, now);
    }
  }
  else if (characterization_state == HEATER_CHARACTERIZATION_COOLING)
  {
    if (hardware.temperature_tenths
        <= (start_temperature_tenths
            + CHARACTERIZATION_COOL_END_DELTA_TENTHS))
    {
      characterization_state = HEATER_CHARACTERIZATION_COMPLETE;
      publish_log(&hardware, now);
    }
    else if ((now - state_started_at)
             >= CHARACTERIZATION_COOLING_TIMEOUT_MS)
    {
      characterization_fault = HEATER_CHARACTERIZATION_FAULT_COOLING_TIMEOUT;
      characterization_state = HEATER_CHARACTERIZATION_COMPLETE;
      publish_log(&hardware, now);
    }
  }

  if ((HeaterCharacterization_IsRunning() != 0U)
      && ((now - last_log_at) >= CHARACTERIZATION_LOG_INTERVAL_MS))
  {
    publish_log(&hardware, now);
  }
}
