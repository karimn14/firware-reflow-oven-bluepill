#ifndef INSPECTION_H
#define INSPECTION_H

/* Raspberry Pi inspection link over USB CDC.
 * Protocol: sunda_reflow_oven/docs/stm-pi-protocol.md ($DET / $ACK / $RES / $SORT / $STAT / $HB). */

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  INSPECTION_IDLE = 0,
  INSPECTION_REQUESTING,       /* belt stopped, settling before the first $DET */
  INSPECTION_WAITING,          /* $DET sent, waiting for $RES */
  INSPECTION_PASS,
  INSPECTION_FAIL,
  INSPECTION_TIMEOUT,          /* no matching $RES in time: sorted as REJECT */
  INSPECTION_PROTOCOL_ERROR    /* kept for the UI; bad frames are counted, not a state */
} InspectionState;

typedef struct
{
  InspectionState state;
  uint32_t board_id;
  uint16_t pass_count;
  uint16_t fail_count;
  uint16_t protocol_errors;
  uint8_t waiting;
  uint8_t vision_online;       /* Pi heartbeat fresh and it has a camera (OLED only) */
} InspectionStatus;

/* Writes one frame to the USB CDC port; returns 0 if it could not be sent. */
typedef uint8_t (*InspectionWriteFn)(const char *text, uint16_t length);

void Inspection_Init(void);

/* Inspection task: settle, $DET resends, verdict timeout, $SORT and $EVT detection. */
void Inspection_Task(void *argument);

/* CDC task only. Returns 1 for every line that starts with '$' (a protocol frame, valid or not),
 * so the console neither echoes nor answers it. */
uint8_t Inspection_HandleCdcLine(const char *line);

/* CDC task only, every loop: sends the pending frames. Nothing is sent while no host holds DTR. */
void Inspection_CdcPoll(InspectionWriteFn write, uint8_t host_connected);

/* CDC task only: DTR rose (the Pi or a terminal opened the port). */
void Inspection_OnHostConnected(void);

/* Buttons B/C on the conveyor screen. */
void Inspection_SubmitManualResult(uint8_t pass);
void Inspection_GetStatus(InspectionStatus *status);

#endif /* INSPECTION_H */
