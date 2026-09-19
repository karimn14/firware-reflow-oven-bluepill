#ifndef THERMISTOR_H
#define THERMISTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef struct
{
  uint16_t adc;
  int16_t reference_tenths;
  uint8_t valid;
} ThermistorCalibrationPoint;

void Thermistor_Init(void);
uint8_t Thermistor_Calculate(uint16_t adc,
                            int16_t *temperature_tenths,
                            uint32_t *resistance_ohm);
uint8_t Thermistor_SetCalibrationPoint(uint8_t index,
                                      uint16_t adc,
                                      int16_t reference_tenths);
const ThermistorCalibrationPoint *Thermistor_GetCalibrationPoint(uint8_t index);
uint8_t Thermistor_CalibrationReady(void);
HAL_StatusTypeDef Thermistor_SaveCalibration(void);
uint16_t Thermistor_GetBeta(void);
uint32_t Thermistor_GetNominalResistance(void);
uint8_t Thermistor_IsCalibrated(void);

#ifdef __cplusplus
}
#endif

#endif /* THERMISTOR_H */
