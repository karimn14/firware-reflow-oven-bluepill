#include "inspection.h"

#include "FreeRTOS.h"
#include "conveyor_app.h"
#include "conveyor_sequencer.h"
#include "hardware_test.h"
#include "pi_protocol.h"
#include "reflow.h"
#include "task.h"

#include <stddef.h>
#include <string.h>

/* Blue Pill <-> Raspberry Pi protocol: sunda_reflow_oven/docs/stm-pi-protocol.md
 *
 *   INSPECT entered -> settle -> $DET,id=N (until the Pi's $ACK, at most 3 sends)
 *   $RES,id=N,v=PASS|FAIL,...  -> $ACK,id=N, servo right (PASS) or left (FAIL)
 *   no matching $RES in time   -> left (REJECT), $SORT carries why=TIMEOUT
 *   servo back at centre       -> $SORT,id=N,bin=PASS|REJECT
 *   every 500 ms               -> $STAT (cycle state, heater, belt, counters)
 *
 * Timing lives in the inspection task. The CDC task is the only one that writes to USB: it calls
 * Inspection_HandleCdcLine() for received '$' lines and Inspection_CdcPoll() to send. */

#define INSPECTION_TASK_INTERVAL_MS    20UL
#define INSPECTION_SETTLE_MS          300UL  /* belt stopped -> sharp photo */
#define INSPECTION_DET_RETRY_MS       300UL
#define INSPECTION_DET_SENDS            3U
#define INSPECTION_RESULT_TIMEOUT_MS 3000UL  /* after the first $DET */
#define INSPECTION_STAT_PERIOD_MS     500UL
#define INSPECTION_HB_TIMEOUT_MS     3000UL
/* Bench tests without the Pi only: PASS after this many ms. Must stay 0 on the line, where a
 * missing verdict has to end in the REJECT bin. */
#define INSPECTION_BENCH_AUTO_PASS_MS   0
#define INSPECTION_FW_VERSION          "bluepill-0.3"

typedef enum
{
  EVENT_ESTOP = 0,
  EVENT_MOTOR_FAULT,
  EVENT_PCB_TIMEOUT,
  EVENT_HEATER_FAULT,
  EVENT_RESET,
  EVENT_COUNT
} InspectionEvent;

static const char *const event_codes[EVENT_COUNT] = {
  "ESTOP", "MOTOR_FAULT", "PCB_TIMEOUT", "HEATER_FAULT", "RESET"
};

/* Shared between the inspection task, the CDC task and the UI. */
static volatile InspectionState inspection_state;
static volatile uint32_t active_board_id;
static volatile uint8_t result_pending;
static volatile uint8_t result_pass;
static volatile uint8_t det_pending;        /* inspection task -> CDC task: send $DET */
static volatile uint8_t det_acked;          /* CDC task -> inspection task: Pi has the $DET */
static volatile uint8_t sort_pending;
static volatile uint8_t sort_pass;
static volatile uint8_t sort_timeout;
static volatile uint32_t sort_board_id;
static volatile uint32_t events_pending;    /* bit per InspectionEvent */
static volatile uint8_t hello_pending;
static volatile uint32_t last_hb_at;
static volatile uint8_t hb_seen;
static volatile uint8_t camera_ok;
static volatile uint16_t pass_count;
static volatile uint16_t fail_count;
static volatile uint16_t protocol_errors;

/* Inspection task only. */
static uint32_t next_board_id;
static uint32_t inspection_started_at;
static uint32_t first_det_at;
static uint32_t next_det_at;
static uint8_t det_sends;

/* CDC task only. Static so the CDC task stack stays small. */
static uint8_t ack_pending;
static uint32_t ack_board_id;
static uint32_t next_stat_at;
static char rx_frame[PI_FRAME_MAX];
static PiFrameBuilder tx;
static ReflowStatus reflow_status;

static uint8_t is_fault(ConveyorSequenceState state)
{
  return ((state == CONVEYOR_SEQ_ESTOP)
          || (state == CONVEYOR_SEQ_MOTOR_FAULT)
          || (state == CONVEYOR_SEQ_PCB_TIMEOUT)
          || (state == CONVEYOR_SEQ_HEATER_FAULT)) ? 1U : 0U;
}

static void raise_event(InspectionEvent event)
{
  taskENTER_CRITICAL();
  events_pending |= (1UL << (uint32_t)event);
  taskEXIT_CRITICAL();
}

/* The Pi's verdict, a manual button, or a timeout: hand it to the conveyor exactly once. */
static void decide(InspectionState outcome)
{
  uint8_t pass = (outcome == INSPECTION_PASS) ? 1U : 0U;

  taskENTER_CRITICAL();
  inspection_state = outcome;
  result_pending = 0U;
  det_pending = 0U;
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

void Inspection_Init(void)
{
  inspection_state = INSPECTION_IDLE;
  active_board_id = 0U;
  next_board_id = 1U;
  result_pending = 0U;
  det_pending = 0U;
  det_acked = 0U;
  sort_pending = 0U;
  events_pending = 0U;
  hello_pending = 1U;
  hb_seen = 0U;
  camera_ok = 0U;
  pass_count = 0U;
  fail_count = 0U;
  protocol_errors = 0U;
  ack_pending = 0U;
  next_stat_at = 0U;
}

void Inspection_Task(void *argument)
{
  TickType_t last_wake = xTaskGetTickCount();
  ConveyorSequenceState previous = CONVEYOR_SEQ_IDLE;

  (void)argument;
  for (;;)
  {
    ConveyorAppStatus conveyor;
    uint32_t now = HAL_GetTick();

    ConveyorApp_GetStatus(&conveyor);

    if ((conveyor.state == CONVEYOR_SEQ_INSPECTION)
        && (previous != CONVEYOR_SEQ_INSPECTION))
    {
      /* A new board under the camera: number it and drop anything left from the last one. */
      taskENTER_CRITICAL();
      active_board_id = next_board_id++;
      result_pending = 0U;
      det_pending = 0U;
      det_acked = 0U;
      inspection_state = INSPECTION_REQUESTING;
      taskEXIT_CRITICAL();
      inspection_started_at = now;
      det_sends = 0U;
    }

    if (conveyor.state == CONVEYOR_SEQ_INSPECTION)
    {
      if ((inspection_state == INSPECTION_REQUESTING)
          || (inspection_state == INSPECTION_WAITING))
      {
        if (result_pending != 0U)
        {
          decide((result_pass != 0U) ? INSPECTION_PASS : INSPECTION_FAIL);
        }
#if INSPECTION_BENCH_AUTO_PASS_MS > 0
        else if ((now - inspection_started_at) >= INSPECTION_BENCH_AUTO_PASS_MS)
        {
          decide(INSPECTION_PASS);
        }
#endif
        else if (inspection_state == INSPECTION_REQUESTING)
        {
          if ((now - inspection_started_at) >= INSPECTION_SETTLE_MS)
          {
            first_det_at = now;
            next_det_at = now;
            inspection_state = INSPECTION_WAITING;
          }
        }
        else if ((now - first_det_at) >= INSPECTION_RESULT_TIMEOUT_MS)
        {
          decide(INSPECTION_TIMEOUT);
        }
      }
      if ((inspection_state == INSPECTION_WAITING) && (det_acked == 0U)
          && (det_sends < INSPECTION_DET_SENDS)
          && ((int32_t)(now - next_det_at) >= 0))
      {
        det_pending = 1U;
        ++det_sends;
        next_det_at = now + INSPECTION_DET_RETRY_MS;
      }
    }

    if (conveyor.state != previous)
    {
      if ((conveyor.state == CONVEYOR_SEQ_SERVO_CENTER)
          && ((previous == CONVEYOR_SEQ_SERVO_RIGHT)
              || (previous == CONVEYOR_SEQ_SERVO_LEFT)))
      {
        /* Servo back at centre: report where the board physically went. */
        taskENTER_CRITICAL();
        sort_board_id = active_board_id;
        sort_pass = (conveyor.last_result == CONVEYOR_RESULT_PASS) ? 1U : 0U;
        sort_timeout = ((conveyor.last_result == CONVEYOR_RESULT_TIMEOUT)
                        || (inspection_state == INSPECTION_TIMEOUT)) ? 1U : 0U;
        sort_pending = 1U;
        taskEXIT_CRITICAL();
      }
      switch (conveyor.state)
      {
        case CONVEYOR_SEQ_ESTOP:        raise_event(EVENT_ESTOP); break;
        case CONVEYOR_SEQ_MOTOR_FAULT:  raise_event(EVENT_MOTOR_FAULT); break;
        case CONVEYOR_SEQ_PCB_TIMEOUT:  raise_event(EVENT_PCB_TIMEOUT); break;
        case CONVEYOR_SEQ_HEATER_FAULT: raise_event(EVENT_HEATER_FAULT); break;
        case CONVEYOR_SEQ_IDLE:
          if (is_fault(previous) != 0U)
          {
            raise_event(EVENT_RESET);
          }
          break;
        default:
          break;
      }
    }
    previous = conveyor.state;
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(INSPECTION_TASK_INTERVAL_MS));
  }
}

uint8_t Inspection_HandleCdcLine(const char *line)
{
  static PiFrame frame;                   /* CDC task only; ~100 B kept off its stack */
  uint32_t board_id;
  size_t length;

  if ((line == NULL) || (line[0] != '$'))
  {
    return 0U;
  }
  length = strlen(line);
  if (length >= sizeof(rx_frame))
  {
    ++protocol_errors;
    return 1U;
  }
  memcpy(rx_frame, line, length + 1U);
  if (PiProto_Parse(rx_frame, &frame) != PI_PROTO_OK)
  {
    ++protocol_errors;                    /* dropped, never answered */
    return 1U;
  }

  if (strcmp(frame.type, "HB") == 0)
  {
    const char *cv = PiProto_Get(&frame, "cv");
    last_hb_at = HAL_GetTick();
    hb_seen = 1U;
    camera_ok = ((cv != NULL) && (strcmp(cv, "NOCAM") != 0)) ? 1U : 0U;
  }
  else if (strcmp(frame.type, "HELLO") == 0)
  {
    hello_pending = 1U;
  }
  else if ((strcmp(frame.type, "ACK") == 0)
           && (PiProto_GetUint(&frame, "id", &board_id) != 0U))
  {
    if (board_id == active_board_id)
    {
      det_acked = 1U;
    }
  }
  else if ((strcmp(frame.type, "RES") == 0)
           && (PiProto_GetUint(&frame, "id", &board_id) != 0U))
  {
    const char *verdict = PiProto_Get(&frame, "v");
    uint8_t pass = 2U;

    ack_board_id = board_id;              /* always acknowledged, even a late repeat */
    ack_pending = 1U;
    if (verdict != NULL)
    {
      if (strcmp(verdict, "PASS") == 0)
      {
        pass = 1U;
      }
      else if (strcmp(verdict, "FAIL") == 0)
      {
        pass = 0U;
      }
    }
    taskENTER_CRITICAL();
    if ((pass != 2U) && (board_id == active_board_id)
        && ((inspection_state == INSPECTION_REQUESTING)
            || (inspection_state == INSPECTION_WAITING)))
    {
      result_pass = pass;
      result_pending = 1U;
    }
    taskEXIT_CRITICAL();
  }
  return 1U;
}

static void send(InspectionWriteFn write)
{
  size_t length = PiProto_End(&tx);

  if (length != 0U)
  {
    (void)write(tx.buf, (uint16_t)length);
  }
}

static const char *cycle_name(ConveyorSequenceState state)
{
  switch (state)
  {
    case CONVEYOR_SEQ_START_REQUESTED:
    case CONVEYOR_SEQ_MOVING_TO_HEATER:     return "TO_HEATER";
    case CONVEYOR_SEQ_HEATING_WAIT:         return "HEATING";
    case CONVEYOR_SEQ_MOVING_TO_INSPECTION: return "TO_CAMERA";
    case CONVEYOR_SEQ_INSPECTION:           return "INSPECT";
    case CONVEYOR_SEQ_SERVO_RIGHT:
    case CONVEYOR_SEQ_SERVO_LEFT:
    case CONVEYOR_SEQ_SERVO_CENTER:         return "SORT";
    case CONVEYOR_SEQ_ESTOP:                return "ESTOP";
    case CONVEYOR_SEQ_MOTOR_FAULT:
    case CONVEYOR_SEQ_PCB_TIMEOUT:
    case CONVEYOR_SEQ_HEATER_FAULT:         return "FAULT";
    case CONVEYOR_SEQ_IDLE:
    default:                                return "IDLE";
  }
}

static const char *zone_name(ReflowState state)
{
  switch (state)
  {
    case REFLOW_STATE_PREHEAT:    return "PREHEAT";
    case REFLOW_STATE_SOAKING:    return "SOAK";
    case REFLOW_STATE_REFLOW:     return "REFLOW";
    case REFLOW_STATE_COOLING:    return "COOL";
    case REFLOW_STATE_TIMED_TEST: return "TEST";
    case REFLOW_STATE_IDLE:
    default:                      return "IDLE";
  }
}

static void send_stat(InspectionWriteFn write, uint32_t now)
{
  ConveyorAppStatus conveyor;
  HardwareTestStatus hardware;

  ConveyorApp_GetStatus(&conveyor);
  HardwareTest_GetStatus(&hardware);
  Reflow_GetStatus(&reflow_status);

  PiProto_Begin(&tx, "STAT");
  PiProto_AddStr(&tx, "st", cycle_name(conveyor.state));
  PiProto_AddStr(&tx, "zone", zone_name(reflow_status.state));
  if (hardware.temperature_valid != 0U)
  {
    PiProto_AddTenths(&tx, "pv", hardware.temperature_tenths);
  }
  if (hardware.pid_running != 0U)
  {
    PiProto_AddTenths(&tx, "sp", hardware.pid_setpoint_tenths);
  }
  PiProto_AddUint(&tx, "heat",
                  (hardware.heater_enabled != 0U) ? hardware.heater_duty_percent : 0U);
  PiProto_AddStr(&tx, "conv", (conveyor.motor_percent != 0U) ? "RUN" : "STOP");
  PiProto_AddUint(&tx, "prox", conveyor.ir_detected);
  PiProto_AddUint(&tx, "item", active_board_id);
  PiProto_AddUint(&tx, "up", now);
  PiProto_AddUint(&tx, "pass", pass_count);
  PiProto_AddUint(&tx, "fail", fail_count);
  send(write);
}

void Inspection_CdcPoll(InspectionWriteFn write, uint8_t host_connected)
{
  uint32_t now = HAL_GetTick();

  if ((write == NULL) || (host_connected == 0U))
  {
    /* Nobody reads the port. The inspection task still settles and times out, so the board
     * still ends in a bin; a stale $DET or $ACK must not be sent later. */
    det_pending = 0U;
    ack_pending = 0U;
    return;
  }

  if (hello_pending != 0U)
  {
    hello_pending = 0U;
    PiProto_Begin(&tx, "HELLO");
    PiProto_AddStr(&tx, "fw", INSPECTION_FW_VERSION);
    PiProto_AddUint(&tx, "up", now);
    send(write);
  }
  if (ack_pending != 0U)
  {
    ack_pending = 0U;
    PiProto_Begin(&tx, "ACK");
    PiProto_AddUint(&tx, "id", ack_board_id);
    send(write);
  }
  if (det_pending != 0U)
  {
    det_pending = 0U;
    PiProto_Begin(&tx, "DET");
    PiProto_AddUint(&tx, "id", active_board_id);
    send(write);
  }
  if (sort_pending != 0U)
  {
    sort_pending = 0U;
    PiProto_Begin(&tx, "SORT");
    PiProto_AddUint(&tx, "id", sort_board_id);
    PiProto_AddStr(&tx, "bin", (sort_pass != 0U) ? "PASS" : "REJECT");
    if (sort_timeout != 0U)
    {
      PiProto_AddStr(&tx, "why", "TIMEOUT");
    }
    send(write);
  }
  for (uint32_t event = 0U; event < (uint32_t)EVENT_COUNT; ++event)
  {
    if ((events_pending & (1UL << event)) != 0U)
    {
      taskENTER_CRITICAL();
      events_pending &= ~(1UL << event);
      taskEXIT_CRITICAL();
      PiProto_Begin(&tx, "EVT");
      PiProto_AddStr(&tx, "code", event_codes[event]);
      send(write);
    }
  }
  if ((int32_t)(now - next_stat_at) >= 0)
  {
    next_stat_at = now + INSPECTION_STAT_PERIOD_MS;
    send_stat(write, now);
  }
}

void Inspection_OnHostConnected(void)
{
  hello_pending = 1U;
}

void Inspection_SubmitManualResult(uint8_t pass)
{
  taskENTER_CRITICAL();
  if ((inspection_state == INSPECTION_WAITING)
      || (inspection_state == INSPECTION_REQUESTING))
  {
    result_pass = (pass != 0U) ? 1U : 0U;
    result_pending = 1U;
  }
  taskEXIT_CRITICAL();
}

void Inspection_GetStatus(InspectionStatus *status)
{
  uint32_t now = HAL_GetTick();

  if (status == NULL)
  {
    return;
  }
  taskENTER_CRITICAL();
  status->state = inspection_state;
  status->board_id = active_board_id;
  status->pass_count = pass_count;
  status->fail_count = fail_count;
  status->protocol_errors = protocol_errors;
  status->waiting = ((inspection_state == INSPECTION_REQUESTING)
                     || (inspection_state == INSPECTION_WAITING)) ? 1U : 0U;
  status->vision_online = ((hb_seen != 0U) && (camera_ok != 0U)
                           && ((now - last_hb_at) < INSPECTION_HB_TIMEOUT_MS)) ? 1U : 0U;
  taskEXIT_CRITICAL();
}
