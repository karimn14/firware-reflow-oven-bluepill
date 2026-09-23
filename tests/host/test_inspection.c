/* Runs firmware-bluepill's inspection.c on a PC: fake 1 ms clock, a small conveyor model, and a
 * scripted Raspberry Pi. Each scenario is one uninterrupted run of Inspection_Task().
 * Every frame the firmware writes is printed as "TX <ms> <frame>" so the Pi's parser can check it. */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "conveyor_app.h"
#include "hardware_test.h"
#include "inspection.h"
#include "reflow.h"
#include "watchdog.h"

typedef enum { PI_ANSWERS = 0, PI_SILENT, PI_LOST_ACK, PI_OFFLINE, PI_FAIL } PiMode;

static uint32_t now_ms, end_ms;
static jmp_buf done;
static ConveyorAppStatus cv;
static uint32_t state_since;
static PiMode mode;
static int verdicts, verdict_pass = -1, boards;
static unsigned det_seen_id;
static uint32_t det_seen_at, answer_at;

uint32_t HAL_GetTick(void) { return now_ms; }
TickType_t xTaskGetTickCount(void) { return now_ms; }
void Watchdog_Heartbeat(WatchdogId id) { (void)id; }
void ConveyorApp_GetStatus(ConveyorAppStatus *s) { *s = cv; }
void HardwareTest_GetStatus(HardwareTestStatus *s)
{
  memset(s, 0, sizeof(*s));
  s->temperature_valid = 1U;
  s->temperature_tenths = 1824;
  s->fan_duty_percent = 70U;
}
void Reflow_GetStatus(ReflowStatus *s) { memset(s, 0, sizeof(*s)); }

static void go(ConveyorSequenceState st) { cv.state = st; state_since = now_ms; }

/* Same rule as ConveyorSequencer_NotifyInspection(): only while INSPECTION. */
void ConveyorApp_ManualInspectionResult(uint8_t pass)
{
  if (cv.state != CONVEYOR_SEQ_INSPECTION)
  {
    return;
  }
  verdicts++;
  verdict_pass = pass;
  cv.last_result = pass ? CONVEYOR_RESULT_PASS : CONVEYOR_RESULT_FAIL;
  go(pass ? CONVEYOR_SEQ_SERVO_RIGHT : CONVEYOR_SEQ_SERVO_LEFT);
}

static void feed(const char *line)
{
  printf("RX %u %s\n", (unsigned)now_ms, line);
  (void)Inspection_HandleCdcLine(line);
}

static int det_count, tx_count;
static int stat_has_fan;
static char last_sort[64];

static uint8_t write_fn(const char *text, uint16_t length)
{
  unsigned id;
  printf("TX %u %.*s", (unsigned)now_ms, (int)length, text);
  tx_count++;
  if (strncmp(text, "$DET", 4) == 0)
  {
    det_count++;
  }
  if ((strncmp(text, "$STAT", 5) == 0)
      && (strstr(text, ",fan=70,") != NULL))
  {
    stat_has_fan = 1;
  }
  if (strncmp(text, "$SORT", 5) == 0 && length < sizeof(last_sort))
  {
    memcpy(last_sort, text, length);
    last_sort[length] = '\0';
  }
  if (det_seen_id == 0U && sscanf(text, "$DET,id=%u", &id) == 1)
  {
    det_seen_id = id;
    det_seen_at = now_ms;
  }
  return 1U;
}

/* The scripted Pi, called every 5 ms. Frames were produced by pi/station/protocol.py encode(). */
static void pi_step(void)
{
  if (det_seen_id == 0U)
  {
    return;
  }
  if (mode == PI_ANSWERS || mode == PI_FAIL)
  {
    if (now_ms == det_seen_at + 5U)
    {
      feed("$ACK,id=1*64");
      feed("$RES,id=7,v=FAIL,code=C1_MISSING,n=1,ms=5*0B");     /* another board: acked, ignored */
      feed("$RES,id=1,v=PASS,code=-,n=0,ms=180*5");               /* corrupted: dropped */
      answer_at = now_ms + 200U;                                  /* inspection takes ~200 ms */
    }
    if (answer_at != 0U && now_ms == answer_at)
    {
      feed(mode == PI_ANSWERS ? "$RES,id=1,v=PASS,code=-,n=0,ms=180*57"
                              : "$RES,id=1,v=FAIL,code=R2_WRONG,n=1,ms=210*1E");
      answer_at = 0U;
    }
  }
  else if (mode == PI_LOST_ACK && now_ms == 1500U)
  {
    /* the Pi's $ACK never arrived; its answer comes after the Blue Pill resent $DET */
    feed("$RES,id=1,v=PASS,code=-,n=0,ms=180*57");
  }
}

void vTaskDelayUntil(TickType_t *last, TickType_t period)
{
  /* One 20 ms inspection-task tick: the CDC task runs every 5 ms, then the conveyor model. */
  for (int i = 0; i < 4; i++)
  {
    now_ms += 5U;
    Inspection_CdcPoll(write_fn, mode != PI_OFFLINE);
    pi_step();
    if (now_ms == 400U)
    {
      go(CONVEYOR_SEQ_INSPECTION);                               /* IR sensor: board under camera */
      boards++;
    }
  }
  *last += period;
  if ((cv.state == CONVEYOR_SEQ_SERVO_RIGHT || cv.state == CONVEYOR_SEQ_SERVO_LEFT)
      && now_ms - state_since >= 500U)
  {
    go(CONVEYOR_SEQ_SERVO_CENTER);
  }
  else if (cv.state == CONVEYOR_SEQ_SERVO_CENTER && now_ms - state_since >= 500U)
  {
    go(CONVEYOR_SEQ_IDLE);
  }
  if (now_ms >= end_ms)
  {
    longjmp(done, 1);
  }
}

int main(int argc, char **argv)
{
  const char *name = argc > 1 ? argv[1] : "pass";
  InspectionStatus st;

  mode = !strcmp(name, "silent") ? PI_SILENT : !strcmp(name, "lostack") ? PI_LOST_ACK
       : !strcmp(name, "offline") ? PI_OFFLINE : !strcmp(name, "fail") ? PI_FAIL : PI_ANSWERS;
  end_ms = (mode == PI_SILENT || mode == PI_OFFLINE) ? 5000U : 3000U;
  Inspection_Init();
  if (setjmp(done) == 0)
  {
    Inspection_Task(NULL);
  }
  if (mode == PI_SILENT)
  {
    now_ms += 5U;
    feed("$RES,id=1,v=PASS,code=-,n=0,ms=180*57");                /* too late: acked, ignored */
    Inspection_CdcPoll(write_fn, 1U);
  }
  Inspection_GetStatus(&st);
  printf("END boards=%d verdicts=%d pass=%d state=%d errors=%u conveyor=%d dets=%d frames=%d\n",
         boards, verdicts, verdict_pass, (int)st.state, (unsigned)st.protocol_errors, (int)cv.state,
         det_count, tx_count);

  /* expected outcome per scenario (docs/stm-pi-protocol.md) */
  {
    int ok = (verdicts == 1) && (cv.state == CONVEYOR_SEQ_IDLE)
             && ((mode == PI_OFFLINE) || (stat_has_fan != 0));
    switch (mode)
    {
      case PI_ANSWERS:
        ok = ok && verdict_pass == 1 && st.state == INSPECTION_PASS && det_count == 1
             && strstr(last_sort, "bin=PASS") != NULL && st.protocol_errors == 1U;
        break;
      case PI_FAIL:
        ok = ok && verdict_pass == 0 && st.state == INSPECTION_FAIL && det_count == 1
             && strstr(last_sort, "bin=REJECT*") != NULL;
        break;
      case PI_SILENT:
        ok = ok && verdict_pass == 0 && st.state == INSPECTION_TIMEOUT && det_count == 3
             && strstr(last_sort, "bin=REJECT,why=TIMEOUT") != NULL;
        break;
      case PI_LOST_ACK:
        ok = ok && verdict_pass == 1 && st.state == INSPECTION_PASS && det_count == 3
             && strstr(last_sort, "bin=PASS") != NULL;
        break;
      case PI_OFFLINE:
        ok = ok && verdict_pass == 0 && st.state == INSPECTION_TIMEOUT && tx_count == 0;
        break;
    }
    printf("%s %s\n", ok ? "OK" : "FAILED", name);
    return ok ? 0 : 1;
  }
}
