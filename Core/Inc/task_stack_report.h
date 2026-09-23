#ifndef TASK_STACK_REPORT_H
#define TASK_STACK_REPORT_H

#include <stdint.h>

#define TASK_STACK_REPORT_COUNT 7U

typedef struct
{
  const char *name;
  uint16_t allocated_bytes;
  uint16_t peak_used_bytes;
} TaskStackReport;

uint8_t TaskStackReport_Get(uint8_t index, TaskStackReport *report);

#endif /* TASK_STACK_REPORT_H */
