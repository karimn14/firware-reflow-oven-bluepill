/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hardware_test.h"
#include "cdc_console.h"
#include "heater_characterization.h"
#include "reflow.h"
#include "tim.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static StaticTask_t inputTaskControlBlock;
static StackType_t inputTaskStack[96];
TaskHandle_t inputTaskHandle;
static StaticTask_t cdcTaskControlBlock;
static StackType_t cdcTaskStack[192];
TaskHandle_t cdcTaskHandle;
static StaticTask_t reflowTaskControlBlock;
static StackType_t reflowTaskStack[96];
TaskHandle_t reflowTaskHandle;

/* USER CODE END Variables */
/* Definitions for defaultTask */
TaskHandle_t defaultTaskHandle;
static StaticTask_t defaultTaskControlBlock;
static StackType_t defaultTaskStack[192];
static StaticTask_t idleTaskControlBlock;
static StackType_t idleTaskStack[configMINIMAL_STACK_SIZE];

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartInputTask(void *argument);
void StartCdcTask(void *argument);
void StartReflowTask(void *argument);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  Reflow_Init();
  HeaterCharacterization_Init();

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = xTaskCreateStatic(StartDefaultTask, "defaultTask",
      sizeof(defaultTaskStack) / sizeof(defaultTaskStack[0]), NULL, 2U,
      defaultTaskStack, &defaultTaskControlBlock);

  /* USER CODE BEGIN RTOS_THREADS */
  inputTaskHandle = xTaskCreateStatic(StartInputTask, "inputTask",
      sizeof(inputTaskStack) / sizeof(inputTaskStack[0]), NULL, 3U,
      inputTaskStack, &inputTaskControlBlock);
  cdcTaskHandle = xTaskCreateStatic(StartCdcTask, "cdcTask",
      sizeof(cdcTaskStack) / sizeof(cdcTaskStack[0]), NULL, 1U,
      cdcTaskStack, &cdcTaskControlBlock);
  reflowTaskHandle = xTaskCreateStatic(StartReflowTask, "thermalTask",
      sizeof(reflowTaskStack) / sizeof(reflowTaskStack[0]), NULL, 2U,
      reflowTaskStack, &reflowTaskControlBlock);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  HardwareTest_Init();

  /* Infinite loop */
  for(;;)
  {
    HardwareTest_Run();
    vTaskDelay(pdMS_TO_TICKS(10U));
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void StartInputTask(void *argument)
{
  (void)argument;
  for (;;)
  {
    HardwareTest_InputRun();
    vTaskDelay(pdMS_TO_TICKS(10U));
  }
}

void StartCdcTask(void *argument)
{
  CDC_Console_Task(argument);
}

void StartReflowTask(void *argument)
{
  Reflow_Task(argument);
}

void vApplicationGetIdleTaskMemory(StaticTask_t **task_buffer,
                                   StackType_t **stack_buffer,
                                   uint32_t *stack_size)
{
  *task_buffer = &idleTaskControlBlock;
  *stack_buffer = idleTaskStack;
  *stack_size = configMINIMAL_STACK_SIZE;
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
  (void)task;
  (void)task_name;
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0U);
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}

/* USER CODE END Application */

