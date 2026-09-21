#ifndef REFLOW_H
#define REFLOW_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define REFLOW_GRAPH_SAMPLES 128U

typedef enum
{
  REFLOW_STATE_IDLE = 0,
  REFLOW_STATE_PREHEAT,
  REFLOW_STATE_SOAKING,
  REFLOW_STATE_REFLOW,
  REFLOW_STATE_COOLING
} ReflowState;

typedef enum
{
  REFLOW_PROFILE_PREHEAT = 0,
  REFLOW_PROFILE_SOAKING,
  REFLOW_PROFILE_REFLOW
} ReflowProfileSelection;

typedef struct
{
  ReflowState state;
  ReflowProfileSelection selected;
  int16_t preheat_tenths;
  int16_t soaking_tenths;
  int16_t reflow_tenths;
  uint32_t elapsed_seconds;
  uint8_t fault;
  uint8_t graph_count;
  uint8_t graph_temperature_degrees[REFLOW_GRAPH_SAMPLES];
} ReflowStatus;

void Reflow_Init(void);
void Reflow_Task(void *argument);
void Reflow_RequestStart(void);
void Reflow_RequestStop(void);
void Reflow_SelectNextProfile(void);
void Reflow_AdjustSelectedTemperature(int16_t change_tenths);
uint8_t Reflow_IsRunning(void);
void Reflow_GetStatus(ReflowStatus *status);

#ifdef __cplusplus
}
#endif

#endif /* REFLOW_H */
