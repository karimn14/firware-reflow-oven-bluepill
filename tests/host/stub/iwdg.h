#pragma once

typedef struct { int unused; } IWDG_HandleTypeDef;
typedef int HAL_StatusTypeDef;

extern IWDG_HandleTypeDef hiwdg;
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *handle);
