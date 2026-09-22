#include "inspection.h"

#include "FreeRTOS.h"
#include "conveyor_app.h"
#include "conveyor_config.h"
#include "conveyor_sequencer.h"
#include "task.h"
#include "text_format.h"

#include <stddef.h>
#include <string.h>

#define INSPECTION_TASK_INTERVAL_MS  20UL
#define INSPECTION_RETRY_MS         1000UL
#define INSPECTION_RESULT_TIMEOUT_MS 5000UL

static volatile InspectionState inspection_state;
static volatile uint32_t active_board_id;
static volatile uint8_t request_pending;
static volatile uint8_t result_pending;
static volatile uint8_t result_pass;
static uint32_t next_board_id;
static uint32_t inspection_started_at;
static uint32_t last_request_at;
static uint16_t pass_count;
static uint16_t fail_count;

static uint8_t hex_value(char value)
{
  if ((value >= '0') && (value <= '9'))
  {
    return (uint8_t)(value - '0');
  }
  if ((value >= 'A') && (value <= 'F'))
  {
    return (uint8_t)(value - 'A' + 10);
  }
  if ((value >= 'a') && (value <= 'f'))
  {
    return (uint8_t)(value - 'a' + 10);
  }
  return 0xffU;
}

static uint8_t validate_checksum(const char *line, const char **payload_end)
{
  const char *asterisk;
  uint8_t checksum = 0U;
  uint8_t high;
  uint8_t low;

  if ((line == NULL) || (line[0] != '$'))
  {
    return 0U;
  }
  asterisk = strchr(line, '*');
  if (asterisk == NULL)
  {
    *payload_end = line + strlen(line);
    return 1U;
  }
  high = hex_value(asterisk[1]);
  low = hex_value(asterisk[2]);
  if ((high > 15U) || (low > 15U) || (asterisk[3] != '\0'))
  {
    return 0U;
  }
  for (const char *cursor = line + 1; cursor < asterisk; ++cursor)
  {
    checksum ^= (uint8_t)*cursor;
  }
  *payload_end = asterisk;
  return (checksum == (uint8_t)((high << 4U) | low)) ? 1U : 0U;
}

static uint8_t parse_board_id(const char *text, const char *end,
                              uint32_t *board_id, const char **after)
{
  uint32_t value = 0U;
  uint8_t digits = 0U;

  while ((text < end) && (*text >= '0') && (*text <= '9'))
  {
    value = value * 10UL + (uint32_t)(*text - '0');
    ++text;
    ++digits;
  }
  if (digits == 0U)
  {
    return 0U;
  }
  *board_id = value;
  *after = text;
  return 1U;
}

void Inspection_Init(void)
{
  inspection_state = INSPECTION_IDLE;
  active_board_id = 0U;
  next_board_id = 1U;
  request_pending = 0U;
  result_pending = 0U;
  pass_count = 0U;
  fail_count = 0U;
}

void Inspection_Task(void *argument)
{
  TickType_t last_wake = xTaskGetTickCount();
  uint8_t was_inspecting = 0U;

  (void)argument;
  for (;;)
  {
    ConveyorAppStatus conveyor;
    uint8_t inspecting;
    uint32_t now = HAL_GetTick();

    ConveyorApp_GetStatus(&conveyor);
    inspecting = (conveyor.state == CONVEYOR_SEQ_INSPECTION) ? 1U : 0U;
    if ((inspecting != 0U) && (was_inspecting == 0U))
    {
      taskENTER_CRITICAL();
      active_board_id = next_board_id++;
      request_pending = 1U;
      result_pending = 0U;
      inspection_state = INSPECTION_REQUESTING;
      taskEXIT_CRITICAL();
      inspection_started_at = now;
      last_request_at = now;
    }
    if (inspecting != 0U)
    {
      if (result_pending != 0U)
      {
        uint8_t pass;
        taskENTER_CRITICAL();
        pass = result_pass;
        result_pending = 0U;
        inspection_state = pass ? INSPECTION_PASS : INSPECTION_FAIL;
        if (pass != 0U)
        {
          ++pass_count;
        }
        else
        {
          ++fail_count;
        }
        taskEXIT_CRITICAL();
        ConveyorApp_ManualInspectionResult(pass);
      }
      else if (((inspection_state == INSPECTION_REQUESTING)
                || (inspection_state == INSPECTION_WAITING)
                || (inspection_state == INSPECTION_PROTOCOL_ERROR))
               && ((now - inspection_started_at)
                   >= CONVEYOR_E2E_AUTO_SWEEP_DELAY_MS))
      {
        taskENTER_CRITICAL();
        inspection_state = INSPECTION_PASS;
        request_pending = 0U;
        ++pass_count;
        taskEXIT_CRITICAL();
        ConveyorApp_ManualInspectionResult(1U);
      }
      else if ((inspection_state != INSPECTION_TIMEOUT)
               && ((now - inspection_started_at)
                   >= INSPECTION_RESULT_TIMEOUT_MS))
      {
        taskENTER_CRITICAL();
        inspection_state = INSPECTION_TIMEOUT;
        request_pending = 0U;
        ++fail_count;
        taskEXIT_CRITICAL();
        ConveyorApp_ManualInspectionResult(0U);
      }
      else if (((inspection_state == INSPECTION_REQUESTING)
                || (inspection_state == INSPECTION_WAITING)
                || (inspection_state == INSPECTION_PROTOCOL_ERROR))
               && ((now - last_request_at) >= INSPECTION_RETRY_MS))
      {
        request_pending = 1U;
        inspection_state = INSPECTION_WAITING;
        last_request_at = now;
      }
    }
    was_inspecting = inspecting;
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(INSPECTION_TASK_INTERVAL_MS));
  }
}

uint8_t Inspection_TakeRequest(uint32_t *board_id)
{
  if ((board_id == NULL) || (request_pending == 0U))
  {
    return 0U;
  }
  taskENTER_CRITICAL();
  *board_id = active_board_id;
  request_pending = 0U;
  inspection_state = INSPECTION_WAITING;
  taskEXIT_CRITICAL();
  return 1U;
}

uint8_t Inspection_HandleCdcLine(const char *line)
{
  const char prefix[] = "$RESULT,id=";
  const char *end;
  const char *cursor;
  uint32_t board_id;
  uint8_t pass;

  if ((line == NULL) || (strncmp(line, prefix, sizeof(prefix) - 1U) != 0))
  {
    return 0U;
  }
  if (validate_checksum(line, &end) == 0U)
  {
    inspection_state = INSPECTION_PROTOCOL_ERROR;
    return 1U;
  }
  cursor = line + sizeof(prefix) - 1U;
  if ((parse_board_id(cursor, end, &board_id, &cursor) == 0U)
      || (cursor >= end) || (*cursor != ','))
  {
    inspection_state = INSPECTION_PROTOCOL_ERROR;
    return 1U;
  }
  ++cursor;
  if (((end - cursor) >= 4) && (strncmp(cursor, "PASS", 4U) == 0)
      && ((cursor + 4 == end) || (cursor[4] == ',')))
  {
    pass = 1U;
  }
  else if (((end - cursor) >= 4) && (strncmp(cursor, "FAIL", 4U) == 0)
           && ((cursor + 4 == end) || (cursor[4] == ',')))
  {
    pass = 0U;
  }
  else
  {
    inspection_state = INSPECTION_PROTOCOL_ERROR;
    return 1U;
  }

  if ((inspection_state == INSPECTION_WAITING)
      || (inspection_state == INSPECTION_REQUESTING))
  {
    if (board_id == active_board_id)
    {
      taskENTER_CRITICAL();
      result_pass = pass;
      result_pending = 1U;
      taskEXIT_CRITICAL();
    }
  }
  return 1U;
}

size_t Inspection_FormatRequest(char *buffer, size_t size, uint32_t board_id)
{
  char payload[32];
  uint8_t checksum = 0U;

  (void)TextFormat(payload, sizeof(payload), "INSPECT,id=%lu",
                   (unsigned long)board_id);
  for (const char *cursor = payload; *cursor != '\0'; ++cursor)
  {
    checksum ^= (uint8_t)*cursor;
  }
  return TextFormat(buffer, size, "$%s*%02X\r\n", payload, checksum);
}

void Inspection_SubmitManualResult(uint8_t pass)
{
  if ((inspection_state != INSPECTION_WAITING)
      && (inspection_state != INSPECTION_REQUESTING))
  {
    return;
  }
  taskENTER_CRITICAL();
  result_pass = (pass != 0U) ? 1U : 0U;
  result_pending = 1U;
  taskEXIT_CRITICAL();
}

void Inspection_GetStatus(InspectionStatus *status)
{
  if (status == NULL)
  {
    return;
  }
  taskENTER_CRITICAL();
  status->state = inspection_state;
  status->board_id = active_board_id;
  status->pass_count = pass_count;
  status->fail_count = fail_count;
  status->waiting = ((inspection_state == INSPECTION_REQUESTING)
                     || (inspection_state == INSPECTION_WAITING)) ? 1U : 0U;
  taskEXIT_CRITICAL();
}
