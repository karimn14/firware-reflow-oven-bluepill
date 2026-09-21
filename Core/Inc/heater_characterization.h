#ifndef HEATER_CHARACTERIZATION_H
#define HEATER_CHARACTERIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
  HEATER_CHARACTERIZATION_IDLE = 0,
  HEATER_CHARACTERIZATION_HEATING,
  HEATER_CHARACTERIZATION_COOLING,
  HEATER_CHARACTERIZATION_COMPLETE,
  HEATER_CHARACTERIZATION_FAULT
} HeaterCharacterizationState;

typedef enum
{
  HEATER_CHARACTERIZATION_FAULT_NONE = 0,
  HEATER_CHARACTERIZATION_FAULT_SENSOR,
  HEATER_CHARACTERIZATION_FAULT_CALIBRATION,
  HEATER_CHARACTERIZATION_FAULT_START_HOT,
  HEATER_CHARACTERIZATION_FAULT_HEATING_TIMEOUT,
  HEATER_CHARACTERIZATION_FAULT_COOLING_TIMEOUT,
  HEATER_CHARACTERIZATION_FAULT_ABORTED
} HeaterCharacterizationFault;

typedef struct
{
  HeaterCharacterizationState state;
  HeaterCharacterizationFault fault;
  uint32_t elapsed_ms;
  int32_t rate_milli_c_per_s;
  int32_t average_rate_milli_c_per_s;
  int16_t start_temperature_tenths;
  int16_t peak_temperature_tenths;
  int16_t overshoot_tenths;
  uint8_t duty_percent;
} HeaterCharacterizationStatus;

typedef struct
{
  uint32_t session;
  uint32_t sequence;
  uint32_t elapsed_ms;
  int32_t rate_milli_c_per_s;
  int16_t temperature_tenths;
  int16_t peak_temperature_tenths;
  int16_t overshoot_tenths;
  HeaterCharacterizationState state;
  HeaterCharacterizationFault fault;
  uint8_t duty_percent;
  uint8_t temperature_valid;
} HeaterCharacterizationLogSample;

void HeaterCharacterization_Init(void);
void HeaterCharacterization_Process(void);
void HeaterCharacterization_RequestStart(void);
void HeaterCharacterization_RequestStop(void);
void HeaterCharacterization_AdjustDuty(int8_t steps);
uint8_t HeaterCharacterization_IsRunning(void);
void HeaterCharacterization_GetStatus(HeaterCharacterizationStatus *status);
uint8_t HeaterCharacterization_GetLatestLog(
    HeaterCharacterizationLogSample *sample);
const char *HeaterCharacterization_StateName(
    HeaterCharacterizationState state);

#ifdef __cplusplus
}
#endif

#endif /* HEATER_CHARACTERIZATION_H */
