#include "thermistor.h"

#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"

#include <math.h>
#include <stddef.h>

#define ADC_FULL_SCALE             4095U
#define NTC_FIXED_RESISTOR_OHM     4700.0f
#define NTC_DEFAULT_R25_OHM        100000.0f
#define NTC_DEFAULT_BETA           3950.0f
#define NTC_REFERENCE_KELVIN       298.15f

/* The tested STM32F103C8 board has 128 KiB physical flash and 1 KiB pages.
 * The final page is reserved in the linker script so calibration survives
 * firmware updates. The legacy address supports one-time migration from the
 * previous 64 KiB memory layout. */
#define CALIBRATION_FLASH_ADDRESS         0x0801fc00UL
#define LEGACY_CALIBRATION_FLASH_ADDRESS  0x0800fc00UL
#define CALIBRATION_MAGIC          0x4e544343UL /* "NTCC" */
#define CALIBRATION_VERSION        2UL

typedef struct
{
  uint32_t magic;
  uint32_t version;
  uint32_t point_1_adc;
  uint32_t point_1_temperature;
  uint32_t point_2_adc;
  uint32_t point_2_temperature;
  uint32_t checksum;
} CalibrationRecord;

static ThermistorCalibrationPoint points[2];
static float model_beta = NTC_DEFAULT_BETA;
static float model_r25 = NTC_DEFAULT_R25_OHM;
static uint8_t calibrated;
static uint8_t ntc_to_ground = 1U;

static float resistance_from_adc(uint16_t adc)
{
  if (ntc_to_ground != 0U)
  {
    return NTC_FIXED_RESISTOR_OHM * (float)adc
           / (float)(ADC_FULL_SCALE - adc);
  }
  return NTC_FIXED_RESISTOR_OHM * (float)(ADC_FULL_SCALE - adc)
         / (float)adc;
}

static uint32_t record_checksum(const CalibrationRecord *record)
{
  const uint32_t *words = (const uint32_t *)record;
  uint32_t hash = 2166136261UL;

  for (size_t i = 0U; i < 6U; ++i)
  {
    hash ^= words[i];
    hash *= 16777619UL;
  }
  return hash;
}

static uint8_t record_is_valid(const CalibrationRecord *record)
{
  return ((record->magic == CALIBRATION_MAGIC)
          && (record->version == CALIBRATION_VERSION)
          && (record->checksum == record_checksum(record))) ? 1U : 0U;
}

static uint8_t rebuild_model(void)
{
  float r1;
  float r2;
  float t1;
  float t2;
  float denominator;
  float beta;
  float r25;

  if ((points[0].valid == 0U) || (points[1].valid == 0U)
      || (points[0].adc == points[1].adc)
      || (points[0].reference_tenths == points[1].reference_tenths))
  {
    return 0U;
  }

  t1 = ((float)points[0].reference_tenths / 10.0f) + 273.15f;
  t2 = ((float)points[1].reference_tenths / 10.0f) + 273.15f;
  if ((t1 < 223.15f) || (t2 < 223.15f))
  {
    return 0U;
  }

  /* With the NTC toward GND, ADC falls as temperature rises. With the NTC
   * toward VDD, ADC rises. Detect that topology from the two reference points. */
  ntc_to_ground = (((int32_t)points[1].adc - points[0].adc)
                   * ((int32_t)points[1].reference_tenths
                      - points[0].reference_tenths) < 0) ? 1U : 0U;
  r1 = resistance_from_adc(points[0].adc);
  r2 = resistance_from_adc(points[1].adc);

  denominator = (1.0f / t1) - (1.0f / t2);
  if (fabsf(denominator) < 0.000001f)
  {
    return 0U;
  }

  beta = logf(r1 / r2) / denominator;
  r25 = r1 / expf(beta * ((1.0f / t1) - (1.0f / NTC_REFERENCE_KELVIN)));

  /* Reject implausible data instead of saving a dangerous temperature curve. */
  if (!isfinite(beta) || !isfinite(r25)
      || (beta < 1500.0f) || (beta > 7000.0f)
      || (r25 < 1000.0f) || (r25 > 500000.0f))
  {
    return 0U;
  }

  model_beta = beta;
  model_r25 = r25;
  calibrated = 1U;
  return 1U;
}

void Thermistor_Init(void)
{
  const CalibrationRecord *record =
      (const CalibrationRecord *)CALIBRATION_FLASH_ADDRESS;
  uint8_t migrate_legacy_record = 0U;

  model_beta = NTC_DEFAULT_BETA;
  model_r25 = NTC_DEFAULT_R25_OHM;
  calibrated = 0U;
  ntc_to_ground = 1U;
  points[0].valid = 0U;
  points[1].valid = 0U;

  if (record_is_valid(record) == 0U)
  {
    record = (const CalibrationRecord *)LEGACY_CALIBRATION_FLASH_ADDRESS;
    if (record_is_valid(record) == 0U)
    {
      return;
    }
    migrate_legacy_record = 1U;
  }

  points[0].adc = (uint16_t)record->point_1_adc;
  points[0].reference_tenths = (int16_t)record->point_1_temperature;
  points[0].valid = 1U;
  points[1].adc = (uint16_t)record->point_2_adc;
  points[1].reference_tenths = (int16_t)record->point_2_temperature;
  points[1].valid = 1U;

  if (rebuild_model() == 0U)
  {
    points[0].valid = 0U;
    points[1].valid = 0U;
  }
  else if (migrate_legacy_record != 0U)
  {
    (void)Thermistor_SaveCalibration();
  }
}

uint8_t Thermistor_Calculate(uint16_t adc,
                            int16_t *temperature_tenths,
                            uint32_t *resistance_ohm)
{
  float resistance;
  float inverse_kelvin;
  float celsius;

  if ((temperature_tenths == NULL) || (adc < 2U)
      || (adc > (ADC_FULL_SCALE - 2U)))
  {
    return 0U;
  }

  resistance = resistance_from_adc(adc);
  inverse_kelvin = (1.0f / NTC_REFERENCE_KELVIN)
                   + (logf(resistance / model_r25) / model_beta);
  celsius = (1.0f / inverse_kelvin) - 273.15f;
  if (!isfinite(celsius) || (celsius < -100.0f) || (celsius > 350.0f))
  {
    return 0U;
  }

  *temperature_tenths = (int16_t)((celsius >= 0.0f)
                           ? (celsius * 10.0f + 0.5f)
                           : (celsius * 10.0f - 0.5f));
  if (resistance_ohm != NULL)
  {
    *resistance_ohm = (uint32_t)(resistance + 0.5f);
  }
  return 1U;
}

uint8_t Thermistor_SetCalibrationPoint(uint8_t index,
                                      uint16_t adc,
                                      int16_t reference_tenths)
{
  if ((index >= 2U) || (adc < 2U) || (adc > (ADC_FULL_SCALE - 2U))
      || (reference_tenths < -500) || (reference_tenths > 3000))
  {
    return 0U;
  }

  points[index].adc = adc;
  points[index].reference_tenths = reference_tenths;
  points[index].valid = 1U;

  if ((points[0].valid != 0U) && (points[1].valid != 0U))
  {
    return rebuild_model();
  }
  return 1U;
}

const ThermistorCalibrationPoint *Thermistor_GetCalibrationPoint(uint8_t index)
{
  return (index < 2U) ? &points[index] : NULL;
}

uint8_t Thermistor_CalibrationReady(void)
{
  return ((points[0].valid != 0U) && (points[1].valid != 0U)
          && (calibrated != 0U)) ? 1U : 0U;
}

HAL_StatusTypeDef Thermistor_SaveCalibration(void)
{
  CalibrationRecord record;
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_error = 0U;
  const uint32_t *words = (const uint32_t *)&record;
  HAL_StatusTypeDef status;

  if (Thermistor_CalibrationReady() == 0U)
  {
    return HAL_ERROR;
  }

  record.magic = CALIBRATION_MAGIC;
  record.version = CALIBRATION_VERSION;
  record.point_1_adc = points[0].adc;
  record.point_1_temperature = (uint32_t)(int32_t)points[0].reference_tenths;
  record.point_2_adc = points[1].adc;
  record.point_2_temperature = (uint32_t)(int32_t)points[1].reference_tenths;
  record.checksum = record_checksum(&record);

  status = HAL_FLASH_Unlock();
  if (status != HAL_OK)
  {
    return status;
  }

  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = CALIBRATION_FLASH_ADDRESS;
  erase.NbPages = 1U;
  status = HAL_FLASHEx_Erase(&erase, &page_error);

  for (uint32_t i = 0U; (i < 7U) && (status == HAL_OK); ++i)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                               CALIBRATION_FLASH_ADDRESS + (i * 4U),
                               words[i]);
  }

  (void)HAL_FLASH_Lock();
  return status;
}

uint16_t Thermistor_GetBeta(void)
{
  return (uint16_t)(model_beta + 0.5f);
}

uint32_t Thermistor_GetNominalResistance(void)
{
  return (uint32_t)(model_r25 + 0.5f);
}

uint8_t Thermistor_IsCalibrated(void)
{
  return calibrated;
}
