#ifndef OLED_SSD1306_H
#define OLED_SSD1306_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define OLED_WIDTH 128U
#define OLED_HEIGHT 64U
#define OLED_TEXT_COLS 21U
#define OLED_TEXT_ROWS 8U

typedef enum {
	OLED_FONT_SMALL = 1U,
	OLED_FONT_BIG = 2U
} OLED_FontSize_t;

bool OLED_Init(I2C_HandleTypeDef *hi2c);
void OLED_Clear(void);
void OLED_DrawPixel(uint8_t x, uint8_t y, bool on);
void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bitmap, bool invert);
void OLED_DrawCharXY(uint8_t x, uint8_t y, char ch, OLED_FontSize_t fontSize);
void OLED_DrawTextXY(uint8_t x, uint8_t y, const char *text, OLED_FontSize_t fontSize);
void OLED_SetCursor(uint8_t x, uint8_t y);
void OLED_WriteText(const char *text, OLED_FontSize_t fontSize);
void OLED_PrintLine(uint8_t row, const char *text);
HAL_StatusTypeDef OLED_Update(void);

#endif
