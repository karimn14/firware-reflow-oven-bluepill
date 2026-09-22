#include "reflow.h"

#include "FreeRTOS.h"
#include "hardware_test.h"
#include "heater_characterization.h"
#include "main.h"
#include "process_interlock.h"
#include "task.h"

#include <string.h>

#define REFLOW_DEFAULT_PREHEAT_TENTHS  600
#define REFLOW_DEFAULT_SOAKING_TENTHS  900
#define REFLOW_DEFAULT_REFLOW_TENTHS  1200
#define REFLOW_MIN_PREHEAT_TENTHS      400
#define REFLOW_MAX_PEAK_TENTHS        1200
#define REFLOW_MIN_STAGE_GAP_TENTHS     50
#define REFLOW_PREHEAT_DURATION_MS   90000UL
#define REFLOW_SOAKING_DURATION_MS   75000UL
#define REFLOW_REFLOW_DURATION_MS    75000UL
#define REFLOW_COOLING_END_TENTHS      500
#define REFLOW_OVERTEMP_TENTHS        1250
#define REFLOW_TEST_SAFETY_TENTHS     1100
#define REFLOW_TEST_MAX_DURATION_MS  60000UL
#define REFLOW_GRAPH_INTERVAL_MS      2000UL
#define REFLOW_TASK_INTERVAL_MS        100U

static volatile ReflowState reflow_state;
static volatile ReflowProfileSelection selected_profile;
static volatile int16_t preheat_tenths;
static volatile int16_t soaking_tenths;
static volatile int16_t reflow_tenths;
static volatile uint8_t start_requested;
static volatile uint8_t timed_test_requested;
static volatile uint8_t stop_requested;
static volatile uint8_t reflow_fault;
static uint32_t profile_started_at;
static uint32_t stage_started_at;
static uint32_t last_graph_sample_at;
static uint32_t last_elapsed_seconds;
static uint8_t graph_temperature_degrees[REFLOW_GRAPH_SAMPLES];
static uint8_t graph_count;
static uint8_t heater_lock_held;
static uint32_t timed_test_duration_ms;
static uint8_t timed_test_duty_percent;

static void record_graph_sample(const HardwareTestStatus *hardware,
                                uint32_t now)
{
  uint8_t sample = 0xffU;

  if (hardware->temperature_valid != 0U)
  {
    int16_t degrees = (int16_t)((hardware->temperature_tenths + 5) / 10);
    if (degrees < 0)
    {
      degrees = 0;
    }
    else if (degrees > 254)
    {
      degrees = 254;
    }
    sample = (uint8_t)degrees;
  }

  if ((now - last_graph_sample_at) < REFLOW_GRAPH_INTERVAL_MS)
  {
    return;
  }
  last_graph_sample_at = now;

  if (graph_count < REFLOW_GRAPH_SAMPLES)
  {
    graph_temperature_degrees[graph_count++] = sample;
  }
  else
  {
    memmove(&graph_temperature_degrees[0], &graph_temperature_degrees[1],
            REFLOW_GRAPH_SAMPLES - 1U);
    graph_temperature_degrees[REFLOW_GRAPH_SAMPLES - 1U] = sample;
  }
}

static void enter_heating_stage(ReflowState state, int16_t target_tenths,
                                uint32_t now)
{
  reflow_state = state;
  stage_started_at = now;
  HardwareTest_PIDSetSetpoint(target_tenths);
}

static void enter_cooling(uint8_t fault, uint32_t now)
{
  HardwareTest_PIDStop();
  reflow_fault = fault;
  reflow_state = REFLOW_STATE_COOLING;
  stage_started_at = now;
}

static void stop_profile(uint8_t fault, uint32_t now)
{
  if (reflow_state == REFLOW_STATE_TIMED_TEST)
  {
    HardwareTest_HeaterStop();
  }
  else
  {
    HardwareTest_PIDStop();
  }
  reflow_fault = fault;
  if (reflow_state != REFLOW_STATE_IDLE)
  {
    last_elapsed_seconds = (now - profile_started_at) / 1000UL;
  }
  reflow_state = REFLOW_STATE_IDLE;
  if (heater_lock_held != 0U)
  {
    ProcessInterlock_Give(PROCESS_OWNER_HEATER);
    heater_lock_held = 0U;
  }
}

static void start_profile(uint32_t now)
{
  HardwareTestStatus hardware;

  if (ProcessInterlock_Take(PROCESS_OWNER_HEATER, 0U) == 0U)
  {
    reflow_fault = 2U;
    return;
  }
  heater_lock_held = 1U;
  HardwareTest_GetStatus(&hardware);
  if ((hardware.temperature_valid == 0U)
      || (hardware.thermistor_calibrated == 0U)
      || (hardware.temperature_tenths >= REFLOW_OVERTEMP_TENTHS)
      || (HardwareTest_PIDStartAt(preheat_tenths) == 0U))
  {
    stop_profile(1U, now);
    return;
  }

  reflow_fault = 0U;
  graph_count = 0U;
  profile_started_at = now;
  stage_started_at = now;
  last_graph_sample_at = now - REFLOW_GRAPH_INTERVAL_MS;
  last_elapsed_seconds = 0U;
  reflow_state = REFLOW_STATE_PREHEAT;
}

static void start_timed_test(uint32_t now)
{
  HardwareTestStatus hardware;

  if (ProcessInterlock_Take(PROCESS_OWNER_HEATER, 0U) == 0U)
  {
    reflow_fault = 2U;
    return;
  }
  heater_lock_held = 1U;
  HardwareTest_GetStatus(&hardware);
  if ((hardware.temperature_valid == 0U)
      || (hardware.thermistor_calibrated == 0U)
      || (hardware.temperature_tenths >= REFLOW_TEST_SAFETY_TENTHS)
      || (HardwareTest_HeaterStartAtDuty(
            timed_test_duty_percent, REFLOW_TEST_SAFETY_TENTHS) == 0U))
  {
    stop_profile(1U, now);
    return;
  }

  reflow_fault = 0U;
  graph_count = 0U;
  profile_started_at = now;
  stage_started_at = now;
  last_graph_sample_at = now - REFLOW_GRAPH_INTERVAL_MS;
  last_elapsed_seconds = 0U;
  reflow_state = REFLOW_STATE_TIMED_TEST;
}

void Reflow_Init(void)
{
  reflow_state = REFLOW_STATE_IDLE;
  selected_profile = REFLOW_PROFILE_PREHEAT;
  preheat_tenths = REFLOW_DEFAULT_PREHEAT_TENTHS;
  soaking_tenths = REFLOW_DEFAULT_SOAKING_TENTHS;
  reflow_tenths = REFLOW_DEFAULT_REFLOW_TENTHS;
  start_requested = 0U;
  timed_test_requested = 0U;
  stop_requested = 0U;
  reflow_fault = 0U;
  graph_count = 0U;
  last_elapsed_seconds = 0U;
  heater_lock_held = 0U;
  timed_test_duration_ms = 0U;
  timed_test_duty_percent = 0U;
}

void Reflow_RequestStart(void)
{
  if (reflow_state == REFLOW_STATE_IDLE)
  {
    start_requested = 1U;
  }
}

uint8_t Reflow_RequestTimedTest(uint32_t duration_ms, uint8_t duty_percent)
{
  if ((reflow_state != REFLOW_STATE_IDLE)
      || (timed_test_requested != 0U)
      || (duration_ms == 0U)
      || (duration_ms > REFLOW_TEST_MAX_DURATION_MS)
      || (duty_percent == 0U) || (duty_percent > 100U))
  {
    return 0U;
  }

  taskENTER_CRITICAL();
  timed_test_duration_ms = duration_ms;
  timed_test_duty_percent = duty_percent;
  start_requested = 0U;
  timed_test_requested = 1U;
  taskEXIT_CRITICAL();
  return 1U;
}

void Reflow_RequestStop(void)
{
  stop_requested = 1U;
}

void Reflow_SelectNextProfile(void)
{
  if (reflow_state != REFLOW_STATE_IDLE)
  {
    return;
  }

  selected_profile = (ReflowProfileSelection)
      (((uint8_t)selected_profile + 1U) % 3U);
}

void Reflow_AdjustSelectedTemperature(int16_t change_tenths)
{
  int32_t adjusted;

  if (reflow_state != REFLOW_STATE_IDLE)
  {
    return;
  }

  if (selected_profile == REFLOW_PROFILE_PREHEAT)
  {
    adjusted = (int32_t)preheat_tenths + change_tenths;
    if (adjusted < REFLOW_MIN_PREHEAT_TENTHS)
    {
      adjusted = REFLOW_MIN_PREHEAT_TENTHS;
    }
    if (adjusted > ((int32_t)soaking_tenths - REFLOW_MIN_STAGE_GAP_TENTHS))
    {
      adjusted = (int32_t)soaking_tenths - REFLOW_MIN_STAGE_GAP_TENTHS;
    }
    preheat_tenths = (int16_t)adjusted;
  }
  else if (selected_profile == REFLOW_PROFILE_SOAKING)
  {
    adjusted = (int32_t)soaking_tenths + change_tenths;
    if (adjusted < ((int32_t)preheat_tenths + REFLOW_MIN_STAGE_GAP_TENTHS))
    {
      adjusted = (int32_t)preheat_tenths + REFLOW_MIN_STAGE_GAP_TENTHS;
    }
    if (adjusted > ((int32_t)reflow_tenths - REFLOW_MIN_STAGE_GAP_TENTHS))
    {
      adjusted = (int32_t)reflow_tenths - REFLOW_MIN_STAGE_GAP_TENTHS;
    }
    soaking_tenths = (int16_t)adjusted;
  }
  else
  {
    adjusted = (int32_t)reflow_tenths + change_tenths;
    if (adjusted < ((int32_t)soaking_tenths + REFLOW_MIN_STAGE_GAP_TENTHS))
    {
      adjusted = (int32_t)soaking_tenths + REFLOW_MIN_STAGE_GAP_TENTHS;
    }
    if (adjusted > REFLOW_MAX_PEAK_TENTHS)
    {
      adjusted = REFLOW_MAX_PEAK_TENTHS;
    }
    reflow_tenths = (int16_t)adjusted;
  }
}

uint8_t Reflow_IsRunning(void)
{
  return (reflow_state != REFLOW_STATE_IDLE) ? 1U : 0U;
}

void Reflow_GetStatus(ReflowStatus *status)
{
  if (status == NULL)
  {
    return;
  }

  status->state = reflow_state;
  status->selected = selected_profile;
  status->preheat_tenths = preheat_tenths;
  status->soaking_tenths = soaking_tenths;
  status->reflow_tenths = reflow_tenths;
  status->elapsed_seconds = (reflow_state == REFLOW_STATE_IDLE)
                            ? last_elapsed_seconds
                            : (HAL_GetTick() - profile_started_at) / 1000UL;
  status->fault = reflow_fault;
  status->graph_count = graph_count;
  memcpy(status->graph_temperature_degrees, graph_temperature_degrees,
         graph_count);
}

void Reflow_Task(void *argument)
{
  (void)argument;
  for (;;)
  {
    HardwareTestStatus hardware;
    uint32_t now = HAL_GetTick();

    if (stop_requested != 0U)
    {
      uint8_t was_running = (reflow_state != REFLOW_STATE_IDLE) ? 1U : 0U;
      stop_requested = 0U;
      start_requested = 0U;
      timed_test_requested = 0U;
      stop_profile(was_running ? 2U : 0U, now);
    }
    else if ((timed_test_requested != 0U)
             && (reflow_state == REFLOW_STATE_IDLE))
    {
      timed_test_requested = 0U;
      start_timed_test(now);
    }
    else if ((start_requested != 0U)
             && (reflow_state == REFLOW_STATE_IDLE))
    {
      start_requested = 0U;
      start_profile(now);
    }

    HardwareTest_GetStatus(&hardware);
    if (reflow_state != REFLOW_STATE_IDLE)
    {
      record_graph_sample(&hardware, now);

      if ((hardware.temperature_valid == 0U)
          || (hardware.thermistor_calibrated == 0U))
      {
        stop_profile(1U, now);
      }
      else if ((reflow_state == REFLOW_STATE_TIMED_TEST)
               && ((hardware.heater_enabled == 0U)
                   || (hardware.temperature_tenths
                       >= REFLOW_TEST_SAFETY_TENTHS)))
      {
        stop_profile(1U, now);
      }
      else if ((reflow_state == REFLOW_STATE_TIMED_TEST)
               && ((now - stage_started_at) >= timed_test_duration_ms))
      {
        stop_profile(0U, now);
      }
      else if ((reflow_state != REFLOW_STATE_COOLING)
               && (reflow_state != REFLOW_STATE_TIMED_TEST)
               && ((hardware.pid_fault != 0U)
                   || (hardware.temperature_tenths
                       >= REFLOW_OVERTEMP_TENTHS)))
      {
        enter_cooling(1U, now);
      }
      else if ((reflow_state == REFLOW_STATE_PREHEAT)
               && ((now - stage_started_at)
                   >= REFLOW_PREHEAT_DURATION_MS))
      {
        enter_heating_stage(REFLOW_STATE_SOAKING, soaking_tenths, now);
      }
      else if ((reflow_state == REFLOW_STATE_SOAKING)
               && ((now - stage_started_at)
                   >= REFLOW_SOAKING_DURATION_MS))
      {
        enter_heating_stage(REFLOW_STATE_REFLOW, reflow_tenths, now);
      }
      else if ((reflow_state == REFLOW_STATE_REFLOW)
               && ((now - stage_started_at)
                   >= REFLOW_REFLOW_DURATION_MS))
      {
        enter_cooling(0U, now);
      }
      else if ((reflow_state == REFLOW_STATE_COOLING)
               && (hardware.temperature_tenths
                   <= REFLOW_COOLING_END_TENTHS))
      {
        stop_profile(reflow_fault, now);
      }
    }

    HeaterCharacterization_Process();

    vTaskDelay(pdMS_TO_TICKS(REFLOW_TASK_INTERVAL_MS));
  }
}
