#include "cdc_console.h"

#include "FreeRTOS.h"
#include "conveyor_app.h"
#include "hardware_test.h"
#include "heater_characterization.h"
#include "inspection.h"
#include "task.h"
#include "text_format.h"
#include "usbd_cdc_if.h"

#include <string.h>

#define CDC_RX_RING_SIZE       256U
#define CDC_COMMAND_SIZE       96U
#define CDC_TX_TIMEOUT_MS      500U
#define CDC_TX_RETRY_DELAY_MS  2U

static uint8_t rx_ring[CDC_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile uint32_t rx_dropped;
static volatile uint32_t connection_generation;
static volatile uint8_t terminal_connected;

static uint8_t console_read_byte(uint8_t *value)
{
  uint16_t tail = rx_tail;

  if (tail == rx_head)
  {
    return 0U;
  }

  *value = rx_ring[tail];
  rx_tail = (uint16_t)((tail + 1U) % CDC_RX_RING_SIZE);
  return 1U;
}

static uint8_t console_write(const char *text, uint16_t length)
{
  uint32_t started_at = xTaskGetTickCount();

  if ((text == NULL) || (length == 0U))
  {
    return 1U;
  }

  while (CDC_Transmit_FS((uint8_t *)(uintptr_t)text, length) != USBD_OK)
  {
    if ((xTaskGetTickCount() - started_at) >= CDC_TX_TIMEOUT_MS)
    {
      return 0U;
    }
    vTaskDelay(pdMS_TO_TICKS(CDC_TX_RETRY_DELAY_MS));
  }

  /* The USB stack retains the supplied buffer until transmission completes. */
  while (CDC_TransmitReady_FS() == 0U)
  {
    if ((xTaskGetTickCount() - started_at) >= CDC_TX_TIMEOUT_MS)
    {
      return 0U;
    }
    vTaskDelay(pdMS_TO_TICKS(CDC_TX_RETRY_DELAY_MS));
  }
  return 1U;
}

static void console_write_text(const char *text)
{
  (void)console_write(text, (uint16_t)strlen(text));
}

static void console_print_prompt(void)
{
  console_write_text("bluepill> ");
}

static void console_print_welcome(void)
{
  console_write_text(
      "\r\nSTM32 Blue Pill USB CDC console\r\n"
      "Type 'help' to list commands.\r\n");
  console_print_prompt();
}

static const char *screen_name(HardwareTestScreen screen)
{
  switch (screen)
  {
    case HARDWARE_TEST_SCREEN_HEATER:
      return "heater-test";
    case HARDWARE_TEST_SCREEN_CALIBRATION:
      return "calibration";
    case HARDWARE_TEST_SCREEN_PID:
      return "pid";
    case HARDWARE_TEST_SCREEN_REFLOW:
      return "reflow";
    case HARDWARE_TEST_SCREEN_CHARACTERIZATION:
      return "heater-characterization";
    case HARDWARE_TEST_SCREEN_MOTOR_TEST:
      return "motor-test";
    case HARDWARE_TEST_SCREEN_CONVEYOR:
      return "conveyor";
    default:
      return "unknown";
  }
}

static void console_print_status(void)
{
  HardwareTestStatus status;
  ConveyorAppStatus conveyor;
  InspectionStatus inspection;
  char response[192];
  int16_t setpoint_magnitude;

  HardwareTest_GetStatus(&status);
  setpoint_magnitude = (status.pid_setpoint_tenths < 0)
                       ? (int16_t)-status.pid_setpoint_tenths
                       : status.pid_setpoint_tenths;
  if (status.temperature_valid != 0U)
  {
    int16_t temperature_magnitude = (status.temperature_tenths < 0)
                                    ? (int16_t)-status.temperature_tenths
                                    : status.temperature_tenths;
    (void)TextFormat(response, sizeof(response),
        "screen=%s heater=%s duty=%u%% adc=%u temperature=%s%d.%dC "
        "resistance=%luohm calibrated=%s pid=%s setpoint=%s%d.%dC\r\n",
        screen_name(status.screen), status.heater_enabled ? "on" : "off",
        status.heater_duty_percent, status.adc,
        (status.temperature_tenths < 0) ? "-" : "",
        temperature_magnitude / 10, temperature_magnitude % 10,
        (unsigned long)status.resistance_ohm,
        status.thermistor_calibrated ? "yes" : "no",
        status.pid_fault ? "fault" : (status.pid_running ? "running" : "stopped"),
        (status.pid_setpoint_tenths < 0) ? "-" : "",
        setpoint_magnitude / 10, setpoint_magnitude % 10);
  }
  else
  {
    (void)TextFormat(response, sizeof(response),
        "screen=%s heater=%s duty=%u%% adc=%u temperature=invalid "
        "calibrated=%s pid=%s setpoint=%s%d.%dC\r\n",
        screen_name(status.screen), status.heater_enabled ? "on" : "off",
        status.heater_duty_percent, status.adc,
        status.thermistor_calibrated ? "yes" : "no",
        status.pid_fault ? "fault" : (status.pid_running ? "running" : "stopped"),
        (status.pid_setpoint_tenths < 0) ? "-" : "",
        setpoint_magnitude / 10, setpoint_magnitude % 10);
  }
  console_write_text(response);
  ConveyorApp_GetStatus(&conveyor);
  Inspection_GetStatus(&inspection);
  (void)TextFormat(response, sizeof(response),
      "conveyor=%s motor=%u%% speed=%u%% position=%lu/%lu ir=%s "
      "pcb=%lu inspection=%u pass=%u fail=%u\r\n",
      ConveyorSequencer_StateName(conveyor.state), conveyor.motor_percent,
      conveyor.speed_percent, (unsigned long)conveyor.current_pulses,
      (unsigned long)conveyor.target_pulses,
      conveyor.ir_detected ? "detected" : "clear",
      (unsigned long)inspection.board_id, (unsigned int)inspection.state,
      inspection.pass_count, inspection.fail_count);
  console_write_text(response);
}

static void console_print_characterization_log(
    uint32_t *seen_session, uint32_t *seen_sequence)
{
  HeaterCharacterizationLogSample sample;
  char response[160];

  if ((terminal_connected == 0U)
      || (HeaterCharacterization_GetLatestLog(&sample) == 0U)
      || (sample.sequence == *seen_sequence
          && sample.session == *seen_session))
  {
    return;
  }

  if (sample.session != *seen_session)
  {
    console_write_text(
        "\r\ncharacterization_csv_begin\r\n"
        "elapsed_ms,state,duty_pct,temp_tenths_c,"
        "rate_milli_c_per_s,peak_tenths_c,overshoot_tenths_c,fault\r\n");
    *seen_session = sample.session;
  }

  (void)TextFormat(
      response, sizeof(response), "%lu,%s,%u,%d,%ld,%d,%d,%u\r\n",
      (unsigned long)sample.elapsed_ms,
      HeaterCharacterization_StateName(sample.state), sample.duty_percent,
      sample.temperature_valid ? sample.temperature_tenths : -32768,
      (long)sample.rate_milli_c_per_s,
      sample.peak_temperature_tenths, sample.overshoot_tenths,
      (unsigned int)sample.fault);
  console_write_text(response);
  *seen_sequence = sample.sequence;
}

static void console_execute(char *command)
{
  char *end;

  while (*command == ' ')
  {
    ++command;
  }
  end = command + strlen(command);
  while ((end > command) && (end[-1] == ' '))
  {
    *--end = '\0';
  }

  if (Inspection_HandleCdcLine(command) != 0U)
  {
    return;
  }

  if (strcmp(command, "help") == 0)
  {
    console_write_text(
        "Commands:\r\n"
        "  help         show this help\r\n"
        "  ping         test the connection\r\n"
        "  info         show firmware information\r\n"
        "  status       show sensor and controller status\r\n"
        "  echo <text>  return text to the host\r\n"
        "Protocol: $RESULT,id=<n>,PASS|FAIL*CS\r\n");
  }
  else if (strcmp(command, "ping") == 0)
  {
    console_write_text("pong\r\n");
  }
  else if (strcmp(command, "info") == 0)
  {
    console_write_text(
        "board=STM32F103C8T6 firmware=test-bluepill-1 "
        "transport=USB-CDC bootloader=stm32-hid-bootloader\r\n");
  }
  else if (strcmp(command, "status") == 0)
  {
    console_print_status();
  }
  else if (strncmp(command, "echo ", 5U) == 0)
  {
    console_write_text(command + 5U);
    console_write_text("\r\n");
  }
  else if (*command != '\0')
  {
    console_write_text("Unknown command. Type 'help'.\r\n");
  }
}

static void console_send_inspection_request(void)
{
  uint32_t board_id;
  char frame[48];
  size_t length;

  if ((terminal_connected == 0U)
      || (Inspection_TakeRequest(&board_id) == 0U))
  {
    return;
  }
  length = Inspection_FormatRequest(frame, sizeof(frame), board_id);
  if (length < sizeof(frame))
  {
    (void)console_write(frame, (uint16_t)length);
  }
}

void CDC_Console_OnReceive(const uint8_t *data, uint32_t length)
{
  for (uint32_t i = 0U; i < length; ++i)
  {
    uint16_t head = rx_head;
    uint16_t next = (uint16_t)((head + 1U) % CDC_RX_RING_SIZE);

    if (next == rx_tail)
    {
      ++rx_dropped;
    }
    else
    {
      rx_ring[head] = data[i];
      rx_head = next;
    }
  }
}

void CDC_Console_OnControlLineState(uint8_t dtr_active)
{
  uint8_t new_state = (dtr_active != 0U) ? 1U : 0U;

  if ((new_state != 0U) && (terminal_connected == 0U))
  {
    ++connection_generation;
  }
  terminal_connected = new_state;
}

void CDC_Console_Task(void *argument)
{
  char command[CDC_COMMAND_SIZE];
  uint16_t command_length = 0U;
  uint32_t seen_connection = 0U;
  uint32_t seen_dropped = 0U;
  uint32_t seen_characterization_session = 0U;
  uint32_t seen_characterization_sequence = 0U;
  uint8_t previous_was_cr = 0U;
  uint8_t protocol_line = 0U;

  (void)argument;
  for (;;)
  {
    uint8_t value;

    if (seen_connection != connection_generation)
    {
      seen_connection = connection_generation;
      command_length = 0U;
      previous_was_cr = 0U;
      protocol_line = 0U;
      seen_characterization_session = 0U;
      seen_characterization_sequence = 0U;
      console_print_welcome();
    }

    if (seen_dropped != rx_dropped)
    {
      seen_dropped = rx_dropped;
      console_write_text("\r\nRX overflow; input was dropped.\r\n");
      command_length = 0U;
      console_print_prompt();
    }

    while (console_read_byte(&value) != 0U)
    {
      if ((value == '\r') || (value == '\n'))
      {
        if ((value == '\n') && (previous_was_cr != 0U))
        {
          previous_was_cr = 0U;
          continue;
        }
        previous_was_cr = (value == '\r') ? 1U : 0U;
        if (protocol_line == 0U)
        {
          console_write_text("\r\n");
        }
        command[command_length] = '\0';
        console_execute(command);
        command_length = 0U;
        if (protocol_line == 0U)
        {
          console_print_prompt();
        }
        protocol_line = 0U;
      }
      else if ((value == 0x08U) || (value == 0x7fU))
      {
        previous_was_cr = 0U;
        if (command_length > 0U)
        {
          --command_length;
          console_write_text("\b \b");
        }
      }
      else if ((value >= 0x20U) && (value <= 0x7eU))
      {
        previous_was_cr = 0U;
        if ((command_length == 0U) && (value == '$'))
        {
          protocol_line = 1U;
        }
        if (command_length < (CDC_COMMAND_SIZE - 1U))
        {
          command[command_length++] = (char)value;
          if (protocol_line == 0U)
          {
            (void)console_write((const char *)&value, 1U);
          }
        }
        else
        {
          console_write_text("\r\nCommand too long.\r\n");
          command_length = 0U;
          console_print_prompt();
        }
      }
    }
    console_send_inspection_request();
    console_print_characterization_log(&seen_characterization_session,
                                       &seen_characterization_sequence);
    vTaskDelay(pdMS_TO_TICKS(5U));
  }
}
