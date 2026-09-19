#ifndef SSD1306_H
#define SSD1306_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define SSD1306_WIDTH  128U
#define SSD1306_HEIGHT 64U

typedef struct
{
  I2C_HandleTypeDef *i2c;
  uint16_t address;
  uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8U];
} SSD1306_HandleTypeDef;

HAL_StatusTypeDef SSD1306_Init(SSD1306_HandleTypeDef *display,
                              I2C_HandleTypeDef *i2c,
                              uint8_t address_7bit);
void SSD1306_Clear(SSD1306_HandleTypeDef *display);
void SSD1306_DrawString(SSD1306_HandleTypeDef *display,
                       uint8_t x,
                       uint8_t row,
                       const char *text);
HAL_StatusTypeDef SSD1306_Update(SSD1306_HandleTypeDef *display);

#ifdef __cplusplus
}
#endif

#endif /* SSD1306_H */
