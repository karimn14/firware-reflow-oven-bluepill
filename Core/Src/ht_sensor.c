/* ht_sensor.c — NTC thermistor ADC reading and calibration reference helpers.
 *
 * Owns: latest_adc, latest_temperature_tenths, latest_resistance_ohm,
 *       latest_temperature_valid, calibration_point_index,
 *       calibration_captured_mask, calibration_reference_tracks_measurement,
 *       calibration_reference_tenths.
 *
 * Called from: hardware_test.c (HardwareTest_Run / HardwareTest_Init).
 */

#include "ht_private.h"

#include "adc.h"
#include "thermistor.h"

#include <stdint.h>

/* ---- ADC configuration -------------------------------------------------- */
#define ADC_FULL_SCALE      4095U
#define ADC_AVERAGE_SAMPLES   32U

/* Reference repeat rate used when the calibration reference auto-tracks the
 * live measurement (user holds B/C in UI_CAL_EDIT). */
#define REFERENCE_REPEAT_DELAY_MS 400U
#define REFERENCE_REPEAT_RATE_MS   50U

/* Maximum temperature allowed when the calibration heater is active. */
#define CALIBRATION_MAX_TEMP_TENTHS 1500

/* ---- State --------------------------------------------------------------- */
uint16_t latest_adc;
int16_t  latest_temperature_tenths;
uint32_t latest_resistance_ohm;
uint8_t  latest_temperature_valid;

uint8_t  calibration_point_index;
uint8_t  calibration_captured_mask;
uint8_t  calibration_reference_tracks_measurement;
int16_t  calibration_reference_tenths;

/* ---- read_adc_average() -------------------------------------------------- */
/* Reads ADC_AVERAGE_SAMPLES samples and returns their average with the single
 * minimum and maximum samples discarded (simple outlier rejection). */
uint16_t read_adc_average(void)
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
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
        ++valid_samples;
      }
      (void)HAL_ADC_Stop(&hadc1);
    }
  }

  if (valid_samples == 0U) return 0U;

  /* Drop the single highest and lowest before averaging. */
  if (valid_samples > 2U)
  {
    total -= minimum;
    total -= maximum;
    valid_samples -= 2U;
  }
  return (uint16_t)((total + (valid_samples / 2U)) / valid_samples);
}

/* ---- adjust_calibration_reference() -------------------------------------- */
/* Moves the calibration reference temperature by change_tenths (can be
 * positive or negative) in 0.1 °C steps; called by the UI auto-repeat. */
void adjust_calibration_reference(int16_t change_tenths)
{
  calibration_reference_tracks_measurement = 0U;
  calibration_reference_tenths =
      (int16_t)(calibration_reference_tenths + change_tenths);
}
