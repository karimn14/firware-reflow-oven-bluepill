#ifndef CONVEYOR_CONFIG_H
#define CONVEYOR_CONFIG_H

#include "main.h"
#include "tim.h"

/* STM32F103 Blue Pill adaptation of ../sunda_reflow_oven/firmware/conveyor. */
#define CONVEYOR_ENCODER_SLOTS                 20U
#define CONVEYOR_ENCODER_TIM                    (&htim1)
/* IC1F=0xF: eight stable samples at fDTS/32. With a 72 MHz timer clock this
 * rejects transitions shorter than approximately 3.6 us. */
#define CONVEYOR_ENCODER_FILTER                 15U
#define CONVEYOR_PWM_TIM                       (&htim4)
#define CONVEYOR_PWM_CHANNEL                   TIM_CHANNEL_3
#define CONVEYOR_MOTOR_MIN_PWM                 30U
#define CONVEYOR_DEFAULT_SPEED_PWM             70U
#define CONVEYOR_MIN_SPEED_PWM                 30U
#define CONVEYOR_SPEED_STEP_PWM                 5U

/* Short end-to-end hardware test: move to the heater and energize it briefly,
 * then continue to the end sensor for inspection. The verdict comes from the
 * Raspberry Pi (inspection.c); without one the PCB is sorted as REJECT. */
#define CONVEYOR_E2E_HEATER_DUTY_PERCENT       25U
#define CONVEYOR_E2E_HEATING_DURATION_MS     5000UL

#define CONVEYOR_IR_PORT                       CONVEYOR_IR_GPIO_Port
#define CONVEYOR_IR_PIN                        CONVEYOR_IR_Pin
#define CONVEYOR_IR_ACTIVE_LOW                  1U

#define CONVEYOR_SERVO_TIM                     (&htim2)
#define CONVEYOR_SERVO_CHANNEL                 TIM_CHANNEL_3
#define CONVEYOR_SERVO_LEFT_US                 1000U
#define CONVEYOR_SERVO_CENTER_US               1500U
#define CONVEYOR_SERVO_RIGHT_US                2000U
#define CONVEYOR_SERVO_STEP_MS                  500UL

/* Initial commissioning target; calibrate this pulse count on the machine. */
#define CONVEYOR_HEATER_POSITION_PULSES          50UL
#define CONVEYOR_MOVE_TIMEOUT_MS              15000UL
#define CONVEYOR_HEATER_TIMEOUT_MS            10000UL
/* Full preheat/soak/reflow/cooling can take much longer than the 5 s test. */
#define CONVEYOR_REFLOW_HEATER_TIMEOUT_MS   1800000UL
#define CONVEYOR_IR_TIMEOUT_MS                30000UL
/* Last-resort guard only: inspection.c already rejects after the 300 ms settle
 * plus 3 s without a verdict. Keep this longer than that. */
#define CONVEYOR_INSPECTION_FALLBACK_MS        4500UL
#define CONVEYOR_TASK_INTERVAL_MS                10UL
#define CONVEYOR_SEQUENCE_INTERVAL_MS            50UL

#endif /* CONVEYOR_CONFIG_H */
