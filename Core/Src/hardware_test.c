#include "hardware_test.h"

#include "adc.h"
#include "gpio.h"
#include "i2c.h"
#include "main.h"
#include "ssd1306.h"
#include "thermistor.h"
#include "tim.h"

#include <stdio.h>

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
  SCREEN_PID
} Screen;

static SSD1306_HandleTypeDef oled;
static uint8_t oled_ready;
static volatile uint8_t hardware_initialized;
static uint32_t last_display_update;
static uint8_t heater_enabled;
static uint8_t heater_duty_percent = HEATER_DEFAULT_DUTY;
static uint8_t heater_manual_duty_percent = HEATER_DEFAULT_DUTY;
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
      && (current_screen != SCREEN_PID))
  {
    return;
  }

  now = HAL_GetTick();
  for (uint8_t i = 1U; i <= 2U; ++i)
  {
    uint32_t repeat_rate = (current_screen == SCREEN_PID)
                           ? PID_REPEAT_RATE_MS
                           : REFERENCE_REPEAT_RATE_MS;
    if ((buttons[i].stable_pressed != 0U)
        && ((now - buttons[i].pressed_at) >= REFERENCE_REPEAT_DELAY_MS)
        && ((now - buttons[i].last_repeat_at) >= repeat_rate))
    {
      if (current_screen == SCREEN_PID)
      {
        adjust_pid_setpoint((i == 1U) ? PID_SETPOINT_STEP_TENTHS
                                     : -PID_SETPOINT_STEP_TENTHS);
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

  if (heater_enabled != 0U)
  {
    compare = ((uint32_t)__HAL_TIM_GET_AUTORELOAD(&htim3) + 1U)
              * heater_duty_percent / 100U;
  }
  __HAL_TIM_SET_COMPARE(&htim3, HEATER_PWM_CHANNEL, compare);
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

  if ((latest_temperature_valid == 0U)
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
      else if ((latest_temperature_valid != 0U)
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
          heater_enabled ^= 1U;
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
    else
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
          pid_stop(0U);
          heater_duty_percent = heater_manual_duty_percent;
          current_screen = SCREEN_HEATER;
          break;
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
  (void)snprintf(line, sizeof(line), "%c:%c%03u %c:%c%03u",
                 first_name, first->stable_pressed ? 'P' : '-',
                 first->press_count,
                 second_name, second->stable_pressed ? 'P' : '-',
                 second->press_count);
  SSD1306_DrawString(&oled, 0U, row, line);
}

static void format_temperature(char *text, size_t size, int16_t tenths)
{
  int16_t magnitude = (tenths < 0) ? (int16_t)-tenths : tenths;
  (void)snprintf(text, size, "%s%d.%d", (tenths < 0) ? "-" : "",
                 magnitude / 10, magnitude % 10);
}

static void draw_heater_screen(uint32_t millivolts)
{
  char line[22];

  SSD1306_DrawString(&oled, 0U, 0U, "SSR HEATER TEST");
  (void)snprintf(line, sizeof(line), "SSR:%s DUTY:%3u%%",
                 heater_enabled ? "RUN" : "OFF", heater_duty_percent);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  draw_button_line(2U, 'A', &buttons[0], 'B', &buttons[1]);
  draw_button_line(3U, 'C', &buttons[2], 'D', &buttons[3]);

  (void)snprintf(line, sizeof(line), "ADC:%4u %4lumV", latest_adc,
                 (unsigned long)millivolts);
  SSD1306_DrawString(&oled, 0U, 5U, line);

  if (latest_temperature_valid != 0U)
  {
    char temperature[10];
    format_temperature(temperature, sizeof(temperature),
                       latest_temperature_tenths);
    (void)snprintf(line, sizeof(line), "NTC:%s C %s", temperature,
                   Thermistor_IsCalibrated() ? "CAL" : "DEF");
  }
  else
  {
    (void)snprintf(line, sizeof(line), "NTC: OPEN/SHORT");
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

  (void)snprintf(line, sizeof(line), "THERMISTOR CAL P%u",
                 calibration_point_index + 1U);
  SSD1306_DrawString(&oled, 0U, 0U, line);
  (void)snprintf(line, sizeof(line), "ADC:%4u R:%lu", latest_adc,
                 (unsigned long)latest_resistance_ohm);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)snprintf(line, sizeof(line), "MEAS:%s C", measured);
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)snprintf(line, sizeof(line), "REF :%s C %s", reference,
                 calibration_reference_tracks_measurement ? "AUTO" : "SET");
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)snprintf(line, sizeof(line), "P1:%.6s P2:%.6s", point_1, point_2);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)snprintf(line, sizeof(line), "HEAT:%s PWM:%3u%%",
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
  (void)snprintf(line, sizeof(line), "PV:%.6s SP:%.6s",
                 process_value, setpoint);
  SSD1306_DrawString(&oled, 0U, 1U, line);
  (void)snprintf(line, sizeof(line), "R:%.6s OUT:%3u%%", ramped_setpoint,
                 (unsigned int)(pid_output_percent + 0.5f));
  SSD1306_DrawString(&oled, 0U, 2U, line);
  (void)snprintf(line, sizeof(line), "STATE:%s",
                 pid_fault ? "FAULT" : (pid_running ? "RUN" : "READY"));
  SSD1306_DrawString(&oled, 0U, 3U, line);
  (void)snprintf(line, sizeof(line), "P:%d I:%d D:%d",
                 (int)pid_p_term, (int)pid_i_term, (int)pid_d_term);
  SSD1306_DrawString(&oled, 0U, 4U, line);
  (void)snprintf(line, sizeof(line), "CAL:%s SAFE<150C",
                 Thermistor_IsCalibrated() ? "OK" : "NO");
  SSD1306_DrawString(&oled, 0U, 5U, line);
  SSD1306_DrawString(&oled, 0U, 6U, "A:RUN B:+.5 C:-.5");
  SSD1306_DrawString(&oled, 0U, 7U, "D:OFF/BACK");
}

void HardwareTest_Init(void)
{
  hardware_initialized = 0U;
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
  current_screen = SCREEN_HEATER;
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
  update_button_auto_repeat();
  update_pid_controller();
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
    else
    {
      draw_pid_screen();
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
