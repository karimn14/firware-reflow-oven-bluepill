#include "hardware_test.h"

#include "adc.h"
#include "conveyor_app.h"
#include "gpio.h"
#include "heater_characterization.h"
#include "i2c.h"
#include "inspection.h"
#include "main.h"
#include "process_interlock.h"
#include "reflow.h"
#include "ssd1306.h"
#include "text_format.h"
#include "thermistor.h"
#include "tim.h"


#define OLED_ADDRESS_7BIT       0x3cU
#define ADC_FULL_SCALE          4095U
#define ADC_REFERENCE_MV        3300U
#define ADC_AVERAGE_SAMPLES     32U
#define BUTTON_DEBOUNCE_SAMPLES 2U
#define DISPLAY_UPDATE_MS       50U
#define HEATER_PWM_CHANNEL      TIM_CHANNEL_1
#define HEATER_DEFAULT_DUTY     50U
#define HEATER_DUTY_STEP        25U
#define CALIBRATION_EXIT_HOLD_MS 1500U
#define CALIBRATION_MAX_TEMP_TENTHS 1500
#define REFERENCE_REPEAT_DELAY_MS 400U
#define REFERENCE_REPEAT_RATE_MS  50U
#define PID_REPEAT_RATE_MS         100U
#define PID_CONTROL_INTERVAL_MS    250U
#define PID_SETPOINT_STEP_TENTHS   5
#define PID_SETPOINT_MIN_TENTHS    200
#define PID_SETPOINT_MAX_TENTHS    1400
#define PID_SAFETY_LIMIT_TENTHS    1500
#define PID_KP                      3.0f
#define PID_KI                      0.05f
#define PID_KD                      15.0f
#define PID_SETPOINT_RAMP_C_PER_S   0.5f
#define PID_DERIVATIVE_FILTER_S     1.0f
#define REFLOW_EXIT_HOLD_MS         1500U
#define REFLOW_REPEAT_RATE_MS        100U
#define REFLOW_TEMPERATURE_STEP_TENTHS 10
#define CHARACTERIZATION_EXIT_HOLD_MS 1500U
#define CONVEYOR_EXIT_HOLD_MS         1500U
#define MOTOR_TEST_DEFAULT_DUTY          40U
#define MOTOR_TEST_MIN_DUTY              30U
#define MOTOR_TEST_DUTY_STEP             10U

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t raw_pressed;
  uint8_t stable_pressed;
  uint8_t stable_samples;
  uint16_t press_count;
  uint32_t pressed_at;
  uint32_t last_repeat_at;
} ButtonState;

typedef enum
{
  SCREEN_HEATER = 0,
  SCREEN_THERMISTOR_CALIBRATION,
  SCREEN_PID,
  SCREEN_REFLOW,
  SCREEN_CHARACTERIZATION,
  SCREEN_MOTOR_TEST,
  SCREEN_CONVEYOR
} Screen;

static SSD1306_HandleTypeDef oled;
static uint8_t oled_ready;
static volatile uint8_t hardware_initialized;
static uint32_t last_display_update;
static uint8_t heater_enabled;
static uint8_t heater_duty_percent = HEATER_DEFAULT_DUTY;
static uint8_t heater_manual_duty_percent = HEATER_DEFAULT_DUTY;
static volatile int16_t heater_safety_limit_tenths;
static uint16_t handled_press_count[4];
static Screen current_screen;
static uint16_t latest_adc;
static int16_t latest_temperature_tenths;
static uint32_t latest_resistance_ohm;
static uint8_t latest_temperature_valid;
static uint8_t calibration_point_index;
static uint8_t calibration_captured_mask;
static uint8_t calibration_reference_tracks_measurement;
static int16_t calibration_reference_tenths;
static uint8_t calibration_d_armed;
static uint8_t calibration_d_press_active;
static uint8_t calibration_d_long_handled;
static uint32_t calibration_d_pressed_at;
static int16_t pid_setpoint_tenths = 700;
static uint8_t pid_running;
static uint8_t pid_fault;
static uint32_t pid_last_update;
static float pid_ramped_setpoint;
static float pid_integral;
static float pid_filtered_temperature;
static float pid_previous_filtered_temperature;
static float pid_output_percent;
static float pid_p_term;
static float pid_i_term;
static float pid_d_term;
static uint8_t reflow_d_armed;
static uint8_t reflow_d_press_active;
static uint8_t reflow_d_long_handled;
static uint32_t reflow_d_pressed_at;
static uint8_t characterization_d_armed;
static uint8_t characterization_d_press_active;
static uint8_t characterization_d_long_handled;
static uint32_t characterization_d_pressed_at;
static uint8_t motor_test_duty_percent = MOTOR_TEST_DEFAULT_DUTY;
static uint8_t conveyor_d_armed;
static uint8_t conveyor_d_press_active;
static uint8_t conveyor_d_long_handled;
static uint32_t conveyor_d_pressed_at;

static void adjust_pid_setpoint(int16_t change_tenths);

static ButtonState buttons[4] = {
  {BTN_A_GPIO_Port, BTN_A_Pin, 0U, 0U, 0U, 0U, 0U, 0U},
  {BTN_B_GPIO_Port, BTN_B_Pin, 0U, 0U, 0U, 0U, 0U, 0U},
  {BTN_C_GPIO_Port, BTN_C_Pin, 0U, 0U, 0U, 0U, 0U, 0U},
  {BTN_D_GPIO_Port, BTN_D_Pin, 0U, 0U, 0U, 0U, 0U, 0U}
};

static void update_buttons(void)
{
  for (uint8_t i = 0U; i < 4U; ++i)
  {
    uint8_t pressed = (HAL_GPIO_ReadPin(buttons[i].port, buttons[i].pin)
                       == GPIO_PIN_RESET) ? 1U : 0U;

    if (pressed != buttons[i].raw_pressed)
    {
      buttons[i].raw_pressed = pressed;
      buttons[i].stable_samples = 0U;
    }
    else if (buttons[i].stable_samples < BUTTON_DEBOUNCE_SAMPLES)
    {
      ++buttons[i].stable_samples;
    }
    else if (buttons[i].stable_pressed != pressed)
    {
      buttons[i].stable_pressed = pressed;
      if (pressed != 0U)
      {
        ++buttons[i].press_count;
        buttons[i].pressed_at = HAL_GetTick();
        buttons[i].last_repeat_at = buttons[i].pressed_at;
      }
    }
  }
}

static void adjust_calibration_reference(int16_t change_tenths)
{
  int32_t adjusted = (int32_t)calibration_reference_tenths + change_tenths;

  calibration_reference_tracks_measurement = 0U;
  if (adjusted > 3000)
  {
    adjusted = 3000;
  }
  else if (adjusted < -500)
  {
    adjusted = -500;
  }
  calibration_reference_tenths = (int16_t)adjusted;
}

static void update_button_auto_repeat(void)
{
  uint32_t now;

  if ((current_screen != SCREEN_THERMISTOR_CALIBRATION)
      && (current_screen != SCREEN_PID)
      && (current_screen != SCREEN_REFLOW))
  {
    return;
  }

  now = HAL_GetTick();
  for (uint8_t i = 1U; i <= 2U; ++i)
  {
    uint32_t repeat_rate = (current_screen == SCREEN_THERMISTOR_CALIBRATION)
                           ? REFERENCE_REPEAT_RATE_MS
                           : ((current_screen == SCREEN_REFLOW)
                              ? REFLOW_REPEAT_RATE_MS
                              : PID_REPEAT_RATE_MS);
    if ((buttons[i].stable_pressed != 0U)
        && ((now - buttons[i].pressed_at) >= REFERENCE_REPEAT_DELAY_MS)
        && ((now - buttons[i].last_repeat_at) >= repeat_rate))
    {
      if (current_screen == SCREEN_PID)
      {
        adjust_pid_setpoint((i == 1U) ? PID_SETPOINT_STEP_TENTHS
                                     : -PID_SETPOINT_STEP_TENTHS);
      }
      else if (current_screen == SCREEN_REFLOW)
      {
        Reflow_AdjustSelectedTemperature(
            (i == 1U) ? REFLOW_TEMPERATURE_STEP_TENTHS
                      : -REFLOW_TEMPERATURE_STEP_TENTHS);
      }
      else
      {
        adjust_calibration_reference((i == 1U) ? 1 : -1);
      }
      buttons[i].last_repeat_at = now;
    }
  }
}

static void set_heater_output(void)
{
  uint32_t compare = 0U;

  if (ProcessInterlock_GetOwner() == PROCESS_OWNER_CONVEYOR)
  {
    heater_enabled = 0U;
  }
  if (heater_enabled != 0U)
  {
    compare = ((uint32_t)__HAL_TIM_GET_AUTORELOAD(&htim3) + 1U)
              * heater_duty_percent / 100U;
  }
  __HAL_TIM_SET_COMPARE(&htim3, HEATER_PWM_CHANNEL, compare);
}

static void enforce_direct_heater_safety(void)
{
  if ((heater_safety_limit_tenths > 0)
      && (heater_enabled != 0U)
      && ((latest_temperature_valid == 0U)
          || (Thermistor_IsCalibrated() == 0U)
          || (latest_temperature_tenths >= heater_safety_limit_tenths)))
  {
    heater_enabled = 0U;
    set_heater_output();
  }
}

static float clamp_float(float value, float minimum, float maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return value;
}

static void pid_apply_output(float output_percent)
{
  pid_output_percent = clamp_float(output_percent, 0.0f, 100.0f);
  heater_duty_percent = (uint8_t)(pid_output_percent + 0.5f);
  heater_enabled = ((pid_running != 0U) && (heater_duty_percent > 0U))
                   ? 1U : 0U;
  set_heater_output();
}

static void pid_stop(uint8_t fault)
{
  pid_running = 0U;
  pid_fault = fault;
  pid_integral = 0.0f;
  pid_p_term = 0.0f;
  pid_i_term = 0.0f;
  pid_d_term = 0.0f;
  pid_apply_output(0.0f);
}

static uint8_t pid_start(void)
{
  float measurement;

  if ((ProcessInterlock_GetOwner() == PROCESS_OWNER_CONVEYOR)
      || (latest_temperature_valid == 0U)
      || (Thermistor_IsCalibrated() == 0U)
      || (latest_temperature_tenths >= PID_SAFETY_LIMIT_TENTHS))
  {
    pid_stop(1U);
    return 0U;
  }

  measurement = (float)latest_temperature_tenths / 10.0f;
  pid_fault = 0U;
  pid_running = 1U;
  pid_integral = 0.0f;
  pid_ramped_setpoint = measurement;
  pid_filtered_temperature = measurement;
  pid_previous_filtered_temperature = measurement;
  pid_last_update = HAL_GetTick();
  pid_apply_output(0.0f);
  return 1U;
}

static void adjust_pid_setpoint(int16_t change_tenths)
{
  int32_t adjusted = (int32_t)pid_setpoint_tenths + change_tenths;

  if (adjusted > PID_SETPOINT_MAX_TENTHS)
  {
    adjusted = PID_SETPOINT_MAX_TENTHS;
  }
  else if (adjusted < PID_SETPOINT_MIN_TENTHS)
  {
    adjusted = PID_SETPOINT_MIN_TENTHS;
  }
  pid_setpoint_tenths = (int16_t)adjusted;
}

static void update_pid_controller(void)
{
  uint32_t now;
  uint32_t elapsed_ms;
  float dt;
  float measurement;
  float requested_setpoint;
  float ramp_step;
  float filter_alpha;
  float temperature_rate;
  float error;
  float final_error;
  float output_limit = 100.0f;
  float candidate_integral;
  float unclamped_output;
  float commanded_output;

  if (pid_running == 0U)
  {
    return;
  }
  if ((latest_temperature_valid == 0U)
      || (Thermistor_IsCalibrated() == 0U)
      || (latest_temperature_tenths >= PID_SAFETY_LIMIT_TENTHS))
  {
    pid_stop(1U);
    return;
  }

  now = HAL_GetTick();
  elapsed_ms = now - pid_last_update;
  if (elapsed_ms < PID_CONTROL_INTERVAL_MS)
  {
    return;
  }
  pid_last_update = now;
  dt = (float)elapsed_ms / 1000.0f;
  if (dt > 1.0f)
  {
    dt = 1.0f;
  }

  measurement = (float)latest_temperature_tenths / 10.0f;
  requested_setpoint = (float)pid_setpoint_tenths / 10.0f;
  ramp_step = PID_SETPOINT_RAMP_C_PER_S * dt;
  if (pid_ramped_setpoint < requested_setpoint)
  {
    pid_ramped_setpoint += ramp_step;
    if (pid_ramped_setpoint > requested_setpoint)
    {
      pid_ramped_setpoint = requested_setpoint;
    }
  }
  else if (pid_ramped_setpoint > requested_setpoint)
  {
    pid_ramped_setpoint -= ramp_step;
    if (pid_ramped_setpoint < requested_setpoint)
    {
      pid_ramped_setpoint = requested_setpoint;
    }
  }

  filter_alpha = dt / (PID_DERIVATIVE_FILTER_S + dt);
  pid_filtered_temperature += filter_alpha
                              * (measurement - pid_filtered_temperature);
  temperature_rate = (pid_filtered_temperature
                      - pid_previous_filtered_temperature) / dt;
  pid_previous_filtered_temperature = pid_filtered_temperature;

  error = pid_ramped_setpoint - measurement;
  final_error = requested_setpoint - measurement;
  pid_p_term = PID_KP * error;
  pid_d_term = -PID_KD * temperature_rate;

  /* Taper maximum power close to the final setpoint. This leaves room for
   * stored thermal energy and is intentionally conservative. */
  if (final_error < 1.0f)
  {
    output_limit = 15.0f;
  }
  else if (final_error < 3.0f)
  {
    output_limit = 30.0f;
  }
  else if (final_error < 8.0f)
  {
    output_limit = 60.0f;
  }

  candidate_integral = clamp_float(pid_integral + (PID_KI * error * dt),
                                   0.0f, 60.0f);
  unclamped_output = pid_p_term + candidate_integral + pid_d_term;

  /* Conditional integration prevents windup while output is saturated. */
  if (((unclamped_output > 0.0f) && (unclamped_output < output_limit))
      || ((unclamped_output >= output_limit) && (error < 0.0f))
      || ((unclamped_output <= 0.0f) && (error > 0.0f)))
  {
    pid_integral = candidate_integral;
  }
  pid_i_term = pid_integral;
  commanded_output = clamp_float(pid_p_term + pid_i_term + pid_d_term,
                                 0.0f, output_limit);

  /* Cut heat immediately above target. Allow output to rise gradually, but
   * never limit a downward correction. */
  if (measurement >= (requested_setpoint + 0.2f))
  {
    commanded_output = 0.0f;
    pid_integral *= 0.98f;
  }
  if (commanded_output > (pid_output_percent + 10.0f))
  {
    commanded_output = pid_output_percent + 10.0f;
  }

  pid_apply_output(commanded_output);
}

static void enter_calibration_screen(void)
{
  heater_enabled = 0U;
  set_heater_output();
  current_screen = SCREEN_THERMISTOR_CALIBRATION;
  calibration_point_index = 0U;
  calibration_captured_mask = 0U;
  calibration_reference_tracks_measurement = 1U;
  calibration_reference_tenths = latest_temperature_tenths;
  /* BTN_D is still held when it opens this screen. Arm it after release so
   * that entering calibration cannot accidentally enable the heater. */
  calibration_d_armed = 0U;
  calibration_d_press_active = 0U;
  calibration_d_long_handled = 0U;
}

static void leave_calibration_screen(void)
{
  pid_stop(0U);

  if ((calibration_captured_mask == 0x03U)
      && (Thermistor_CalibrationReady() != 0U))
  {
    (void)Thermistor_SaveCalibration();
  }

  /* Reload saved data; this also discards an incomplete calibration. */
  Thermistor_Init();
  current_screen = SCREEN_PID;
}

static void update_calibration_heater_button(void)
{
  uint8_t pressed;

  if (current_screen != SCREEN_THERMISTOR_CALIBRATION)
  {
    return;
  }

  pressed = buttons[3].stable_pressed;
  if (calibration_d_armed == 0U)
  {
    if (pressed == 0U)
    {
      calibration_d_armed = 1U;
    }
    return;
  }

  if (pressed != 0U)
  {
    if (calibration_d_press_active == 0U)
    {
      calibration_d_press_active = 1U;
      calibration_d_long_handled = 0U;
      calibration_d_pressed_at = HAL_GetTick();
    }
    else if ((calibration_d_long_handled == 0U)
             && ((HAL_GetTick() - calibration_d_pressed_at)
                 >= CALIBRATION_EXIT_HOLD_MS))
    {
      calibration_d_long_handled = 1U;
      leave_calibration_screen();
    }
  }
  else if (calibration_d_press_active != 0U)
  {
    if (calibration_d_long_handled == 0U)
    {
      /* Do not energize a plant without a valid sensor or beyond the test
       * temperature limit. Turning an already-running heater OFF is allowed. */
      if (heater_enabled != 0U)
      {
        heater_enabled = 0U;
      }
      else if ((ProcessInterlock_GetOwner() != PROCESS_OWNER_CONVEYOR)
               && (latest_temperature_valid != 0U)
               && (latest_temperature_tenths
                   < CALIBRATION_MAX_TEMP_TENTHS))
      {
        heater_enabled = 1U;
      }
      set_heater_output();
    }
    calibration_d_press_active = 0U;
    calibration_d_long_handled = 0U;
  }
}

static void enter_reflow_screen(void)
{
  pid_stop(0U);
  current_screen = SCREEN_REFLOW;
  reflow_d_armed = 0U;
  reflow_d_press_active = 0U;
  reflow_d_long_handled = 0U;
}

static void enter_characterization_screen(void)
{
  Reflow_RequestStop();
  pid_stop(0U);
  current_screen = SCREEN_CHARACTERIZATION;
  characterization_d_armed = 0U;
  characterization_d_press_active = 0U;
  characterization_d_long_handled = 0U;
}

static void enter_conveyor_screen(void)
{
  ConveyorApp_ManualMotorStop();
  current_screen = SCREEN_CONVEYOR;
  conveyor_d_armed = 0U;
  conveyor_d_press_active = 0U;
  conveyor_d_long_handled = 0U;
}

static void enter_motor_test_screen(void)
{
  HeaterCharacterization_RequestStop();
  HardwareTest_HeaterStop();
  current_screen = SCREEN_MOTOR_TEST;
}

static void update_reflow_navigation_button(void)
{
  uint8_t pressed;

  if (current_screen != SCREEN_REFLOW)
  {
    return;
  }

  pressed = buttons[3].stable_pressed;
  if (reflow_d_armed == 0U)
  {
    if (pressed == 0U)
    {
      reflow_d_armed = 1U;
    }
    return;
  }

  if (pressed != 0U)
  {
    if (reflow_d_press_active == 0U)
    {
      reflow_d_press_active = 1U;
      reflow_d_long_handled = 0U;
      reflow_d_pressed_at = HAL_GetTick();
    }
    else if ((reflow_d_long_handled == 0U)
             && ((HAL_GetTick() - reflow_d_pressed_at)
                 >= REFLOW_EXIT_HOLD_MS))
    {
      reflow_d_long_handled = 1U;
      enter_characterization_screen();
    }
  }
  else if (reflow_d_press_active != 0U)
  {
    if (reflow_d_long_handled == 0U)
    {
      Reflow_SelectNextProfile();
    }
    reflow_d_press_active = 0U;
    reflow_d_long_handled = 0U;
  }
}

static void update_characterization_navigation_button(void)
{
  uint8_t pressed;

  if (current_screen != SCREEN_CHARACTERIZATION)
  {
    return;
  }

  pressed = buttons[3].stable_pressed;
  if (characterization_d_armed == 0U)
  {
    if (pressed == 0U)
    {
      characterization_d_armed = 1U;
    }
    return;
  }

  if (pressed != 0U)
  {
    if (characterization_d_press_active == 0U)
    {
      characterization_d_press_active = 1U;
      characterization_d_long_handled = 0U;
      characterization_d_pressed_at = HAL_GetTick();
    }
    else if ((characterization_d_long_handled == 0U)
             && ((HAL_GetTick() - characterization_d_pressed_at)
                 >= CHARACTERIZATION_EXIT_HOLD_MS))
    {
      characterization_d_long_handled = 1U;
      enter_motor_test_screen();
    }
  }
  else if (characterization_d_press_active != 0U)
  {
    characterization_d_press_active = 0U;
    characterization_d_long_handled = 0U;
  }
}

static void update_conveyor_navigation_button(void)
{
  uint8_t pressed;

  if (current_screen != SCREEN_CONVEYOR)
  {
    return;
  }
  pressed = buttons[3].stable_pressed;
  if (conveyor_d_armed == 0U)
  {
    if (pressed == 0U)
    {
      conveyor_d_armed = 1U;
    }
    return;
  }
  if (pressed != 0U)
  {
    if (conveyor_d_press_active == 0U)
    {
      conveyor_d_press_active = 1U;
      conveyor_d_long_handled = 0U;
      conveyor_d_pressed_at = HAL_GetTick();
    }
    else if ((conveyor_d_long_handled == 0U)
             && ((HAL_GetTick() - conveyor_d_pressed_at)
                 >= CONVEYOR_EXIT_HOLD_MS))
    {
      conveyor_d_long_handled = 1U;
      current_screen = SCREEN_HEATER;
    }
  }
  else if (conveyor_d_press_active != 0U)
  {
    if (conveyor_d_long_handled == 0U)
    {
      ConveyorApp_CenterServo();
    }
    conveyor_d_press_active = 0U;
    conveyor_d_long_handled = 0U;
  }
}

static void update_controls(void)
{
  for (uint8_t i = 0U; i < 4U; ++i)
  {
    if (handled_press_count[i] == buttons[i].press_count)
    {
      continue;
    }
    handled_press_count[i] = buttons[i].press_count;

    if (current_screen == SCREEN_HEATER)
    {
      switch (i)
      {
        case 0U:
          if ((heater_enabled != 0U)
              || (ProcessInterlock_GetOwner() != PROCESS_OWNER_CONVEYOR))
          {
            heater_enabled ^= 1U;
          }
          break;
        case 1U:
          if (heater_manual_duty_percent <= (100U - HEATER_DUTY_STEP))
          {
            heater_manual_duty_percent += HEATER_DUTY_STEP;
          }
          heater_duty_percent = heater_manual_duty_percent;
          break;
        case 2U:
          if (heater_manual_duty_percent > HEATER_DUTY_STEP)
          {
            heater_manual_duty_percent -= HEATER_DUTY_STEP;
          }
          else
          {
            heater_manual_duty_percent = HEATER_DUTY_STEP;
          }
          heater_duty_percent = heater_manual_duty_percent;
          break;
        case 3U:
          enter_calibration_screen();
          break;
        default:
          break;
      }
      set_heater_output();
    }
    else if (current_screen == SCREEN_THERMISTOR_CALIBRATION)
    {
      switch (i)
      {
        case 0U:
          if ((latest_temperature_valid != 0U)
              && (Thermistor_SetCalibrationPoint(
                    calibration_point_index, latest_adc,
                    calibration_reference_tenths) != 0U))
          {
            calibration_captured_mask |= (uint8_t)(1U << calibration_point_index);
            if (calibration_point_index == 0U)
            {
              calibration_point_index = 1U;
              calibration_reference_tracks_measurement = 1U;
            }
          }
          break;
        case 1U:
          adjust_calibration_reference(1);
          break;
        case 2U:
          adjust_calibration_reference(-1);
          break;
        case 3U:
          /* Short/long BTN_D actions are resolved on release/time below. */
          break;
        default:
          break;
      }
    }
    else if (current_screen == SCREEN_PID)
    {
      switch (i)
      {
        case 0U:
          if (pid_running != 0U)
          {
            pid_stop(0U);
          }
          else
          {
            (void)pid_start();
          }
          break;
        case 1U:
          adjust_pid_setpoint(PID_SETPOINT_STEP_TENTHS);
          break;
        case 2U:
          adjust_pid_setpoint(-PID_SETPOINT_STEP_TENTHS);
          break;
        case 3U:
          enter_reflow_screen();
          break;
        default:
          break;
      }
    }
    else if (current_screen == SCREEN_REFLOW)
    {
      switch (i)
      {
        case 0U:
          if (Reflow_IsRunning() != 0U)
          {
            Reflow_RequestStop();
          }
          else
          {
            Reflow_RequestStart();
          }
          break;
        case 1U:
          Reflow_AdjustSelectedTemperature(
              REFLOW_TEMPERATURE_STEP_TENTHS);
          break;
        case 2U:
          Reflow_AdjustSelectedTemperature(
              -REFLOW_TEMPERATURE_STEP_TENTHS);
          break;
        case 3U:
          /* Short/long actions are resolved on release/time. */
          break;
        default:
          break;
      }
    }
    else if (current_screen == SCREEN_CHARACTERIZATION)
    {
      switch (i)
      {
        case 0U:
          if (HeaterCharacterization_IsRunning() != 0U)
          {
            HeaterCharacterization_RequestStop();
          }
          else
          {
            HeaterCharacterization_RequestStart();
          }
          break;
        case 1U:
          HeaterCharacterization_AdjustDuty(1);
          break;
        case 2U:
          HeaterCharacterization_AdjustDuty(-1);
          break;
        case 3U:
          /* Hold action is resolved by the navigation handler. */
          break;
        default:
          break;
      }
    }
    else if (current_screen == SCREEN_MOTOR_TEST)
    {
      ConveyorAppStatus conveyor;
      ConveyorApp_GetStatus(&conveyor);
      switch (i)
      {
        case 0U:
          if (conveyor.manual_test_active != 0U)
          {
            ConveyorApp_ManualMotorStop();
          }
          else
          {
            (void)ConveyorApp_ManualMotorStart(motor_test_duty_percent);
          }
          break;
        case 1U:
          if (motor_test_duty_percent <= (100U - MOTOR_TEST_DUTY_STEP))
          {
            motor_test_duty_percent += MOTOR_TEST_DUTY_STEP;
          }
          ConveyorApp_ManualMotorSetDuty(motor_test_duty_percent);
          break;
        case 2U:
          if (motor_test_duty_percent
              >= (MOTOR_TEST_MIN_DUTY + MOTOR_TEST_DUTY_STEP))
          {
            motor_test_duty_percent -= MOTOR_TEST_DUTY_STEP;
          }
          ConveyorApp_ManualMotorSetDuty(motor_test_duty_percent);
          break;
        case 3U:
          enter_conveyor_screen();
          break;
        default:
          break;
      }
    }
    else
    {
      ConveyorAppStatus conveyor;
      ConveyorApp_GetStatus(&conveyor);
      switch (i)
      {
        case 0U:
          if (conveyor.state == CONVEYOR_SEQ_IDLE)
          {
            (void)ConveyorApp_RequestStart();
          }
          else if ((conveyor.state == CONVEYOR_SEQ_ESTOP)
                   || (conveyor.state == CONVEYOR_SEQ_MOTOR_FAULT)
                   || (conveyor.state == CONVEYOR_SEQ_PCB_TIMEOUT)
                   || (conveyor.state == CONVEYOR_SEQ_HEATER_FAULT))
          {
            ConveyorApp_AcknowledgeFault();
          }
          else
          {
            ConveyorApp_RequestAbort();
          }
          break;
        case 1U:
          if (conveyor.state == CONVEYOR_SEQ_INSPECTION)
          {
            Inspection_SubmitManualResult(1U);
          }
          else
          {
            ConveyorApp_AdjustSpeed(1);
          }
          break;
        case 2U:
          if (conveyor.state == CONVEYOR_SEQ_INSPECTION)
          {
            Inspection_SubmitManualResult(0U);
          }
          else
          {
            ConveyorApp_AdjustSpeed(-1);
          }
          break;
        case 3U:
        default:
          break;
      }
    }
  }
}

static uint16_t read_adc_average(void)
{
  uint32_t total = 0U;
  uint16_t valid_samples = 0U;
  uint16_t minimum = ADC_FULL_SCALE;
  uint16_t maximum = 0U;

  for (uint8_t i = 0U; i < ADC_AVERAGE_SAMPLES; ++i)
  {
    if (HAL_ADC_Start(&hadc1) == HAL_OK)
    {
      if (HAL_ADC_PollForConversion(&hadc1, 10U) == HAL_OK)
      {
        uint16_t sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
        total += sample;
        if (sample < minimum)
        {
          minimum = sample;
        }
        if (sample > maximum)
        {
          maximum = sample;
        }
        ++valid_samples;
      }
      (void)HAL_ADC_Stop(&hadc1);
    }
  }

  if (valid_samples == 0U)
  {
    return 0U;
  }
  if (valid_samples > 2U)
  {
    total -= minimum;
    total -= maximum;
    valid_samples -= 2U;
  }
  return (uint16_t)((total + (valid_samples / 2U)) / valid_samples);
}

static void draw_button_line(uint8_t row,
                             char first_name,
                             const ButtonState *first,
                             char second_name,
                             const ButtonState *second)
{
  char line[22];
  (void)TextFormat(line, sizeof(line), "%c:%c%03u %c:%c%03u",
                 first_name, first->stable_pressed ? 'P' : '-',
                 first->press_count,
                 second_name, second->stable_pressed ? 'P' : '-',
                 second->press_count);
  SSD1306_DrawString(&oled, 0U, row, line);
}

static void format_temperature(char *text, size_t size, int16_t tenths)
{
  int16_t magnitude = (tenths < 0) ? (int16_t)-tenths : tenths;
  (void)TextFormat(text, size, "%s%d.%d", (tenths < 0) ? "-" : "",
                 magnitude / 10, magnitude % 10);
}

static void draw_heater_screen(uint32_t millivolts)
{
  char line[22];

  SSD1306_DrawString(&oled, 0U, 0U, "SSR HEATER TEST");
  (void)TextFormat(line, sizeof(line), "SSR:%s DUTY:%3u%%",
                 heater_enabled ? "RUN" : "OFF", heater_duty_percent);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  draw_button_line(2U, 'A', &buttons[0], 'B', &buttons[1]);
  draw_button_line(3U, 'C', &buttons[2], 'D', &buttons[3]);

  (void)TextFormat(line, sizeof(line), "ADC:%4u %4lumV", latest_adc,
                 (unsigned long)millivolts);
  SSD1306_DrawString(&oled, 0U, 5U, line);

  if (latest_temperature_valid != 0U)
  {
    char temperature[10];
    format_temperature(temperature, sizeof(temperature),
                       latest_temperature_tenths);
    (void)TextFormat(line, sizeof(line), "NTC:%s C %s", temperature,
                   Thermistor_IsCalibrated() ? "CAL" : "DEF");
  }
  else
  {
    (void)TextFormat(line, sizeof(line), "NTC: OPEN/SHORT");
  }
  SSD1306_DrawString(&oled, 0U, 6U, line);
  SSD1306_DrawString(&oled, 0U, 7U, "A:ON B:+ C:- D:CAL");
}

static void draw_calibration_screen(void)
{
  char line[22];
  char measured[10];
  char reference[10];
  char point_1[10] = "--.-";
  char point_2[10] = "--.-";
  const ThermistorCalibrationPoint *p1 = Thermistor_GetCalibrationPoint(0U);
  const ThermistorCalibrationPoint *p2 = Thermistor_GetCalibrationPoint(1U);

  format_temperature(measured, sizeof(measured), latest_temperature_tenths);
  format_temperature(reference, sizeof(reference),
                     calibration_reference_tenths);
  if ((p1 != NULL) && (p1->valid != 0U))
  {
    format_temperature(point_1, sizeof(point_1), p1->reference_tenths);
  }
  if ((p2 != NULL) && (p2->valid != 0U))
  {
    format_temperature(point_2, sizeof(point_2), p2->reference_tenths);
  }

  (void)TextFormat(line, sizeof(line), "THERMISTOR CAL P%u",
                 calibration_point_index + 1U);
  SSD1306_DrawString(&oled, 0U, 0U, line);
  (void)TextFormat(line, sizeof(line), "ADC:%4u R:%lu", latest_adc,
                 (unsigned long)latest_resistance_ohm);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)TextFormat(line, sizeof(line), "MEAS:%s C", measured);
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)TextFormat(line, sizeof(line), "REF :%s C %s", reference,
                 calibration_reference_tracks_measurement ? "AUTO" : "SET");
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)TextFormat(line, sizeof(line), "P1:%.6s P2:%.6s", point_1, point_2);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)TextFormat(line, sizeof(line), "HEAT:%s PWM:%3u%%",
                 heater_enabled ? "RUN" : "OFF", heater_duty_percent);
  SSD1306_DrawString(&oled, 0U, 5U, line);
  SSD1306_DrawString(&oled, 0U, 6U, "A:CAP B:+.1 C:-.1");
  SSD1306_DrawString(&oled, 0U, 7U, "D:HEAT HOLD:NEXT");
}

static void draw_pid_screen(void)
{
  char line[22];
  char process_value[10] = "--.-";
  char setpoint[10];
  char ramped_setpoint[10];
  int16_t ramped_tenths = (int16_t)((pid_ramped_setpoint >= 0.0f)
                            ? (pid_ramped_setpoint * 10.0f + 0.5f)
                            : (pid_ramped_setpoint * 10.0f - 0.5f));

  if (latest_temperature_valid != 0U)
  {
    format_temperature(process_value, sizeof(process_value),
                       latest_temperature_tenths);
  }
  format_temperature(setpoint, sizeof(setpoint), pid_setpoint_tenths);
  format_temperature(ramped_setpoint, sizeof(ramped_setpoint),
                     ramped_tenths);

  SSD1306_DrawString(&oled, 0U, 0U, "TEMPERATURE PID");
  (void)TextFormat(line, sizeof(line), "PV:%.6s SP:%.6s",
                 process_value, setpoint);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)TextFormat(line, sizeof(line), "R:%.6s OUT:%3u%%", ramped_setpoint,
                 (unsigned int)(pid_output_percent + 0.5f));
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)TextFormat(line, sizeof(line), "STATE:%s",
                 pid_fault ? "FAULT" : (pid_running ? "RUN" : "READY"));
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)TextFormat(line, sizeof(line), "P:%d I:%d D:%d",
                 (int)pid_p_term, (int)pid_i_term, (int)pid_d_term);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)TextFormat(line, sizeof(line), "CAL:%s SAFE<150C",
                 Thermistor_IsCalibrated() ? "OK" : "NO");
  SSD1306_DrawString(&oled, 0U, 5U, line);
  SSD1306_DrawString(&oled, 0U, 6U, "A:RUN B:+.5 C:-.5");
  SSD1306_DrawString(&oled, 0U, 7U, "D:OFF/REFLOW");
}

static const char *reflow_state_name(ReflowState state)
{
  switch (state)
  {
    case REFLOW_STATE_PREHEAT:
      return "PREHEAT";
    case REFLOW_STATE_SOAKING:
      return "SOAKING";
    case REFLOW_STATE_REFLOW:
      return "REFLOW";
    case REFLOW_STATE_TIMED_TEST:
      return "TEST-HEAT";
    case REFLOW_STATE_COOLING:
      return "COOLING";
    case REFLOW_STATE_IDLE:
    default:
      return "IDLE";
  }
}

static uint8_t reflow_graph_y(int16_t temperature_tenths)
{
  int32_t clamped = temperature_tenths;

  if (clamped < 200)
  {
    clamped = 200;
  }
  else if (clamped > 1200)
  {
    clamped = 1200;
  }
  return (uint8_t)(47 - (((clamped - 200) * 31) / 1000));
}

static void draw_reflow_screen(void)
{
  ReflowStatus status;
  char line[22];
  char temperature[10] = "--.-";
  char selected_name;
  int16_t target;
  uint8_t preheat_degrees;
  uint8_t soaking_degrees;
  uint8_t reflow_degrees;
  uint8_t previous_valid = 0U;
  uint8_t previous_x = 0U;
  uint8_t previous_y = 0U;

  Reflow_GetStatus(&status);
  if (latest_temperature_valid != 0U)
  {
    format_temperature(temperature, sizeof(temperature),
                       latest_temperature_tenths);
  }

  (void)TextFormat(line, sizeof(line), "%s %sC %lus",
                 reflow_state_name(status.state), temperature,
                 (unsigned long)status.elapsed_seconds);
  SSD1306_DrawString(&oled, 0U, 0U, line);

  selected_name = (status.selected == REFLOW_PROFILE_PREHEAT) ? 'P'
                  : ((status.selected == REFLOW_PROFILE_SOAKING) ? 'S' : 'R');
  preheat_degrees = (uint8_t)(status.preheat_tenths / 10);
  soaking_degrees = (uint8_t)(status.soaking_tenths / 10);
  reflow_degrees = (uint8_t)(status.reflow_tenths / 10);
  (void)TextFormat(line, sizeof(line), "P%03u S%03u R%03u >%c",
                 preheat_degrees, soaking_degrees, reflow_degrees,
                 selected_name);
  SSD1306_DrawString(&oled, 0U, 1U, line);

  if (status.state == REFLOW_STATE_PREHEAT)
  {
    target = status.preheat_tenths;
  }
  else if (status.state == REFLOW_STATE_SOAKING)
  {
    target = status.soaking_tenths;
  }
  else if (status.state == REFLOW_STATE_REFLOW)
  {
    target = status.reflow_tenths;
  }
  else if (status.selected == REFLOW_PROFILE_PREHEAT)
  {
    target = status.preheat_tenths;
  }
  else if (status.selected == REFLOW_PROFILE_SOAKING)
  {
    target = status.soaking_tenths;
  }
  else
  {
    target = status.reflow_tenths;
  }

  if (status.state != REFLOW_STATE_COOLING)
  {
    uint8_t target_y = reflow_graph_y(target);
    for (uint8_t x = 0U; x < SSD1306_WIDTH; x += 4U)
    {
      SSD1306_DrawPixel(&oled, x, target_y, 1U);
    }
  }

  for (uint8_t i = 0U; i < status.graph_count; ++i)
  {
    uint8_t sample = status.graph_temperature_degrees[i];
    if (sample == 0xffU)
    {
      previous_valid = 0U;
      continue;
    }

    uint8_t y = reflow_graph_y((int16_t)sample * 10);
    if (previous_valid != 0U)
    {
      SSD1306_DrawLine(&oled, previous_x, previous_y, i, y);
    }
    else
    {
      SSD1306_DrawPixel(&oled, i, y, 1U);
    }
    previous_valid = 1U;
    previous_x = i;
    previous_y = y;
  }

  (void)TextFormat(line, sizeof(line), "%sOUT:%3u%% TGT:%3dC",
                 status.fault ? "!" : " ", heater_duty_percent,
                 target / 10);
  SSD1306_DrawString(&oled, 0U, 6U, line);
  SSD1306_DrawString(&oled, 0U, 7U,
                     (status.state == REFLOW_STATE_IDLE)
                     ? "A:RUN B:+ C:- D:SEL"
                     : "A:STOP D-HOLD:NEXT");
}

static const char *characterization_fault_name(
    HeaterCharacterizationFault fault)
{
  switch (fault)
  {
    case HEATER_CHARACTERIZATION_FAULT_SENSOR:
      return "SENSOR";
    case HEATER_CHARACTERIZATION_FAULT_CALIBRATION:
      return "CAL";
    case HEATER_CHARACTERIZATION_FAULT_START_HOT:
      return "HOT";
    case HEATER_CHARACTERIZATION_FAULT_HEATING_TIMEOUT:
      return "HEAT-TIME";
    case HEATER_CHARACTERIZATION_FAULT_COOLING_TIMEOUT:
      return "COOL-TIME";
    case HEATER_CHARACTERIZATION_FAULT_ABORTED:
      return "ABORT";
    case HEATER_CHARACTERIZATION_FAULT_NONE:
    default:
      return "NONE";
  }
}

static void draw_characterization_screen(void)
{
  HeaterCharacterizationStatus status;
  char line[22];
  char temperature[8] = "--.-";
  char peak[8];
  char overshoot[8];
  int32_t rate_magnitude;
  int32_t average_magnitude;

  HeaterCharacterization_GetStatus(&status);
  if (latest_temperature_valid != 0U)
  {
    format_temperature(temperature, sizeof(temperature),
                       latest_temperature_tenths);
  }
  format_temperature(peak, sizeof(peak), status.peak_temperature_tenths);
  format_temperature(overshoot, sizeof(overshoot), status.overshoot_tenths);
  rate_magnitude = (status.rate_milli_c_per_s < 0)
                   ? -status.rate_milli_c_per_s
                   : status.rate_milli_c_per_s;
  average_magnitude = (status.average_rate_milli_c_per_s < 0)
                      ? -status.average_rate_milli_c_per_s
                      : status.average_rate_milli_c_per_s;

  (void)TextFormat(line, sizeof(line), "%s %sC",
                   HeaterCharacterization_StateName(status.state),
                   temperature);
  SSD1306_DrawString(&oled, 0U, 0U, line);
  (void)TextFormat(line, sizeof(line), "DUTY:%3u%% CUT:110C",
                   status.duty_percent);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)TextFormat(line, sizeof(line), "RATE:%c%lu.%03luC/s",
                   (status.rate_milli_c_per_s < 0) ? '-' : '+',
                   (unsigned long)(rate_magnitude / 1000L),
                   (unsigned long)(rate_magnitude % 1000L));
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)TextFormat(line, sizeof(line), "AVG :%c%lu.%03luC/s",
                   (status.average_rate_milli_c_per_s < 0) ? '-' : '+',
                   (unsigned long)(average_magnitude / 1000L),
                   (unsigned long)(average_magnitude % 1000L));
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)TextFormat(line, sizeof(line), "PEAK:%sC OV:%sC", peak, overshoot);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)TextFormat(line, sizeof(line), "TIME:%lus F:%s",
                   (unsigned long)(status.elapsed_ms / 1000UL),
                   characterization_fault_name(status.fault));
  SSD1306_DrawString(&oled, 0U, 5U, line);
  SSD1306_DrawString(&oled, 0U, 6U, "D-HOLD:NEXT");
  SSD1306_DrawString(&oled, 0U, 7U,
                     HeaterCharacterization_IsRunning()
                     ? "A:STOP AUTO LOGGING"
                     : "A:RUN B:+25 C:-25");
}

static const char *inspection_state_name(InspectionState state)
{
  switch (state)
  {
    case INSPECTION_REQUESTING: return "SEND";
    case INSPECTION_WAITING: return "WAIT";
    case INSPECTION_PASS: return "PASS";
    case INSPECTION_FAIL: return "FAIL";
    case INSPECTION_TIMEOUT: return "TIMEOUT";
    case INSPECTION_PROTOCOL_ERROR: return "PROTO-ERR";
    case INSPECTION_IDLE:
    default: return "IDLE";
  }
}

static const char *process_owner_name(ProcessOwner owner)
{
  if (owner == PROCESS_OWNER_CONVEYOR)
  {
    return "BELT";
  }
  if (owner == PROCESS_OWNER_HEATER)
  {
    return "HEAT";
  }
  return "FREE";
}

static void draw_motor_test_screen(void)
{
  ConveyorAppStatus conveyor;
  char line[22];

  ConveyorApp_GetStatus(&conveyor);
  SSD1306_DrawString(&oled, 0U, 0U, "DC MOTOR TEST");
  (void)TextFormat(line, sizeof(line), "STATE: %s",
                   conveyor.manual_test_active ? "RUNNING" : "STOPPED");
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)TextFormat(line, sizeof(line), "PWM  : %3u%%", motor_test_duty_percent);
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)TextFormat(line, sizeof(line), "PULSE: %lu",
                   (unsigned long)conveyor.current_pulses);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  SSD1306_DrawString(&oled, 0U, 6U, "A:START / STOP");
  SSD1306_DrawString(&oled, 0U, 7U, "B:+10 C:-10 D:NEXT");
}

static void draw_conveyor_screen(void)
{
  ConveyorAppStatus conveyor;
  InspectionStatus inspection;
  char line[22];

  ConveyorApp_GetStatus(&conveyor);
  Inspection_GetStatus(&inspection);
  (void)TextFormat(line, sizeof(line), "CV:%s %lus",
                   ConveyorSequencer_StateName(conveyor.state),
                   (unsigned long)(conveyor.state_elapsed_ms / 1000UL));
  SSD1306_DrawString(&oled, 0U, 0U, line);
  (void)TextFormat(line, sizeof(line), "M:%3u%% SET:%3u%%",
                   conveyor.motor_percent, conveyor.speed_percent);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)TextFormat(line, sizeof(line), "POS:%lu/%lu IR:%s",
                   (unsigned long)conveyor.current_pulses,
                   (unsigned long)conveyor.target_pulses,
                   conveyor.ir_detected ? "YES" : "NO");
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)TextFormat(line, sizeof(line), "LOCK:%s SER:%uus",
                   process_owner_name(conveyor.process_owner),
                   conveyor.servo_pulse_us);
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)TextFormat(line, sizeof(line), "PCB:%lu INSP:%s",
                   (unsigned long)inspection.board_id,
                   inspection_state_name(inspection.state));
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)TextFormat(line, sizeof(line), "PASS:%u FAIL:%u",
                   inspection.pass_count, inspection.fail_count);
  SSD1306_DrawString(&oled, 0U, 5U, line);
  SSD1306_DrawString(&oled, 0U, 6U,
                     (conveyor.state == CONVEYOR_SEQ_INSPECTION)
                     ? "B:PASS C:FAIL"
                     : ((conveyor.state == CONVEYOR_SEQ_IDLE)
                        ? "B:SPEED+ C:SPEED-"
                        : "CONTROLS LOCKED"));
  SSD1306_DrawString(&oled, 0U, 7U,
                     (conveyor.state == CONVEYOR_SEQ_IDLE)
                     ? "A:START D-HOLD:BACK"
                     : (((conveyor.state == CONVEYOR_SEQ_ESTOP)
                         || (conveyor.state == CONVEYOR_SEQ_MOTOR_FAULT)
                         || (conveyor.state == CONVEYOR_SEQ_PCB_TIMEOUT)
                         || (conveyor.state == CONVEYOR_SEQ_HEATER_FAULT))
                        ? "A:ACK D-HOLD:BACK"
                        : "A:ABORT D-HOLD:BACK"));
}

void HardwareTest_Init(void)
{
  hardware_initialized = 0U;
  heater_safety_limit_tenths = 0;
  /* Start PWM at 0% so PA6 and the SSR are guaranteed OFF at boot. */
  __HAL_TIM_SET_COMPARE(&htim3, HEATER_PWM_CHANNEL, 0U);
  if (HAL_TIM_PWM_Start(&htim3, HEATER_PWM_CHANNEL) != HAL_OK)
  {
    Error_Handler();
  }

  (void)HAL_ADCEx_Calibration_Start(&hadc1);
  Thermistor_Init();
  latest_adc = read_adc_average();
  latest_temperature_valid = Thermistor_Calculate(
      latest_adc, &latest_temperature_tenths, &latest_resistance_ohm);
  /* Open the end-to-end conveyor test after boot. All actuators remain OFF
   * until button A is pressed. */
  current_screen = SCREEN_CONVEYOR;
  oled_ready = (SSD1306_Init(&oled, &hi2c1, OLED_ADDRESS_7BIT) == HAL_OK)
               ? 1U : 0U;
  last_display_update = HAL_GetTick() - DISPLAY_UPDATE_MS;
  hardware_initialized = 1U;
}

void HardwareTest_InputRun(void)
{
  if (hardware_initialized == 0U)
  {
    return;
  }

  update_buttons();
  if ((current_screen == SCREEN_THERMISTOR_CALIBRATION)
      && (calibration_reference_tracks_measurement != 0U)
      && (latest_temperature_valid != 0U))
  {
    calibration_reference_tenths = latest_temperature_tenths;
  }
  update_controls();
  update_calibration_heater_button();
  update_reflow_navigation_button();
  update_characterization_navigation_button();
  update_conveyor_navigation_button();
  update_button_auto_repeat();
  update_pid_controller();
  enforce_direct_heater_safety();
}

void HardwareTest_Run(void)
{
  uint16_t adc;
  uint32_t millivolts;

  if (hardware_initialized == 0U)
  {
    return;
  }

  if ((HAL_GetTick() - last_display_update) < DISPLAY_UPDATE_MS)
  {
    return;
  }
  last_display_update = HAL_GetTick();

  adc = read_adc_average();
  latest_adc = adc;
  latest_temperature_valid = Thermistor_Calculate(
      latest_adc, &latest_temperature_tenths, &latest_resistance_ohm);
  enforce_direct_heater_safety();
  if ((current_screen == SCREEN_THERMISTOR_CALIBRATION)
      && ((latest_temperature_valid == 0U)
          || (latest_temperature_tenths >= CALIBRATION_MAX_TEMP_TENTHS)))
  {
    heater_enabled = 0U;
    set_heater_output();
  }
  millivolts = ((uint32_t)latest_adc * ADC_REFERENCE_MV + (ADC_FULL_SCALE / 2U))
               / ADC_FULL_SCALE;

  if (oled_ready != 0U)
  {
    SSD1306_Clear(&oled);
    if (current_screen == SCREEN_HEATER)
    {
      draw_heater_screen(millivolts);
    }
    else if (current_screen == SCREEN_THERMISTOR_CALIBRATION)
    {
      draw_calibration_screen();
    }
    else if (current_screen == SCREEN_PID)
    {
      draw_pid_screen();
    }
    else if (current_screen == SCREEN_REFLOW)
    {
      draw_reflow_screen();
    }
    else if (current_screen == SCREEN_CHARACTERIZATION)
    {
      draw_characterization_screen();
    }
    else if (current_screen == SCREEN_MOTOR_TEST)
    {
      draw_motor_test_screen();
    }
    else
    {
      draw_conveyor_screen();
    }

    if (SSD1306_Update(&oled) != HAL_OK)
    {
      oled_ready = 0U;
    }
  }
  else if (HAL_I2C_IsDeviceReady(&hi2c1, (OLED_ADDRESS_7BIT << 1U),
                                 1U, 20U) == HAL_OK)
  {
    oled_ready = (SSD1306_Init(&oled, &hi2c1, OLED_ADDRESS_7BIT) == HAL_OK)
                 ? 1U : 0U;
  }
}

void HardwareTest_GetStatus(HardwareTestStatus *status)
{
  if (status == NULL)
  {
    return;
  }

  status->screen = (HardwareTestScreen)current_screen;
  status->adc = latest_adc;
  status->temperature_tenths = latest_temperature_tenths;
  status->resistance_ohm = latest_resistance_ohm;
  status->pid_setpoint_tenths = pid_setpoint_tenths;
  status->temperature_valid = latest_temperature_valid;
  status->thermistor_calibrated = Thermistor_IsCalibrated();
  status->heater_enabled = heater_enabled;
  status->heater_duty_percent = heater_duty_percent;
  status->pid_running = pid_running;
  status->pid_fault = pid_fault;
}

uint8_t HardwareTest_PIDStartAt(int16_t setpoint_tenths)
{
  HardwareTest_PIDSetSetpoint(setpoint_tenths);
  return pid_start();
}

void HardwareTest_PIDSetSetpoint(int16_t setpoint_tenths)
{
  if (setpoint_tenths < PID_SETPOINT_MIN_TENTHS)
  {
    setpoint_tenths = PID_SETPOINT_MIN_TENTHS;
  }
  else if (setpoint_tenths > PID_SETPOINT_MAX_TENTHS)
  {
    setpoint_tenths = PID_SETPOINT_MAX_TENTHS;
  }
  pid_setpoint_tenths = setpoint_tenths;
}

void HardwareTest_PIDStop(void)
{
  pid_stop(0U);
}

uint8_t HardwareTest_HeaterStartAtDuty(uint8_t duty,
                                       int16_t safety_limit_tenths)
{
  pid_stop(0U);
  if ((ProcessInterlock_GetOwner() == PROCESS_OWNER_CONVEYOR)
      || (duty == 0U) || (duty > 100U)
      || (safety_limit_tenths <= 0)
      || (latest_temperature_valid == 0U)
      || (Thermistor_IsCalibrated() == 0U)
      || (latest_temperature_tenths >= safety_limit_tenths))
  {
    HardwareTest_HeaterStop();
    return 0U;
  }

  heater_duty_percent = duty;
  heater_safety_limit_tenths = safety_limit_tenths;
  heater_enabled = 1U;
  set_heater_output();
  return 1U;
}

void HardwareTest_HeaterStop(void)
{
  heater_enabled = 0U;
  heater_safety_limit_tenths = 0;
  set_heater_output();
}
