#ifndef INSPECTION_H
#define INSPECTION_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  INSPECTION_IDLE = 0,
  INSPECTION_REQUESTING,
  INSPECTION_WAITING,
  INSPECTION_PASS,
  INSPECTION_FAIL,
  INSPECTION_TIMEOUT,
  INSPECTION_PROTOCOL_ERROR
} InspectionState;

typedef struct
{
  InspectionState state;
  uint32_t board_id;
  uint16_t pass_count;
  uint16_t fail_count;
  uint8_t waiting;
} InspectionStatus;

void Inspection_Init(void);
void Inspection_Task(void *argument);
uint8_t Inspection_TakeRequest(uint32_t *board_id);
uint8_t Inspection_HandleCdcLine(const char *line);
size_t Inspection_FormatRequest(char *buffer, size_t size, uint32_t board_id);
void Inspection_SubmitManualResult(uint8_t pass);
void Inspection_GetStatus(InspectionStatus *status);

#endif /* INSPECTION_H */
