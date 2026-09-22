/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define TERMISTOR_Pin GPIO_PIN_0
#define TERMISTOR_GPIO_Port GPIOA
#define CONVEYOR_SERVO_Pin GPIO_PIN_1
#define CONVEYOR_SERVO_GPIO_Port GPIOA
#define PWM_HEATER_Pin GPIO_PIN_6
#define PWM_HEATER_GPIO_Port GPIOA
#define PWM_FAN_Pin GPIO_PIN_7
#define PWM_FAN_GPIO_Port GPIOA
#define CONVEYOR_ENCODER_Pin GPIO_PIN_8
#define CONVEYOR_ENCODER_GPIO_Port GPIOA
#define CONVEYOR_ENCODER_EXTI_IRQn EXTI9_5_IRQn
#define CONVEYOR_IR_Pin GPIO_PIN_1
#define CONVEYOR_IR_GPIO_Port GPIOB
#define PWM_DC_MOTOR_Pin GPIO_PIN_8
#define PWM_DC_MOTOR_GPIO_Port GPIOB
#define BTN_A_Pin GPIO_PIN_12
#define BTN_A_GPIO_Port GPIOB
#define BTN_A_EXTI_IRQn EXTI15_10_IRQn
#define BTN_B_Pin GPIO_PIN_13
#define BTN_B_GPIO_Port GPIOB
#define BTN_B_EXTI_IRQn EXTI15_10_IRQn
#define BTN_C_Pin GPIO_PIN_14
#define BTN_C_GPIO_Port GPIOB
#define BTN_C_EXTI_IRQn EXTI15_10_IRQn
#define BTN_D_Pin GPIO_PIN_15
#define BTN_D_GPIO_Port GPIOB
#define BTN_D_EXTI_IRQn EXTI15_10_IRQn
#define OLED_SCL_Pin GPIO_PIN_6
#define OLED_SCL_GPIO_Port GPIOB
#define OLED_SDA_Pin GPIO_PIN_7
#define OLED_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
