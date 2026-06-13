#include "oled_ssd1306.h"

#include <string.h>

#define OLED_I2C_ADDR (0x3CU << 1)
#define OLED_PAGES (OLED_HEIGHT / 8U)

typedef struct {
    char ch;
    uint8_t col[5];
} OledGlyph;

static I2C_HandleTypeDef *s_hi2c;
static uint8_t s_buf[OLED_WIDTH * OLED_PAGES];
static uint8_t s_cursorX;
static uint8_t s_cursorY;

static const OledGlyph s_glyphs[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'#', {0x14,0x7F,0x14,0x7F,0x14}},
    {'*', {0x14,0x08,0x3E,0x08,0x14}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'.', {0x00,0x00,0x60,0x60,0x00}},
    {'/', {0x01,0x02,0x04,0x08,0x10}},
    {'>', {0x00,0x22,0x14,0x08,0x00}},
    {':', {0x00,0x36,0x36,0x00,0x00}},
    {'0', {0x3E,0x45,0x49,0x51,0x3E}},
    {'1', {0x00,0x21,0x7F,0x01,0x00}},
    {'2', {0x21,0x43,0x45,0x49,0x31}},
    {'3', {0x42,0x41,0x51,0x69,0x46}},
    {'4', {0x0C,0x14,0x24,0x7F,0x04}},
    {'5', {0x72,0x51,0x51,0x51,0x4E}},
    {'6', {0x1E,0x29,0x49,0x49,0x06}},
    {'7', {0x40,0x47,0x48,0x50,0x60}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x30,0x49,0x49,0x4A,0x3C}},
    {'A', {0x3F,0x48,0x48,0x48,0x3F}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x48,0x48,0x48,0x40}},
    {'G', {0x3E,0x41,0x49,0x49,0x2E}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'J', {0x02,0x01,0x01,0x01,0x7E}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x01,0x01,0x01,0x01}},
    {'M', {0x7F,0x20,0x10,0x20,0x7F}},
    {'N', {0x7F,0x10,0x08,0x04,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x48,0x48,0x48,0x30}},
    {'Q', {0x3E,0x41,0x45,0x42,0x3D}},
    {'R', {0x7F,0x48,0x4C,0x4A,0x31}},
    {'S', {0x31,0x49,0x49,0x49,0x46}},
    {'T', {0x40,0x40,0x7F,0x40,0x40}},
    {'U', {0x7E,0x01,0x01,0x01,0x7E}},
    {'V', {0x7C,0x02,0x01,0x02,0x7C}},
    {'W', {0x7F,0x02,0x04,0x02,0x7F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'Y', {0x70,0x08,0x07,0x08,0x70}},
    {'Z', {0x43,0x45,0x49,0x51,0x61}},
    {'a', {0x02,0x15,0x15,0x15,0x0F}},
    {'b', {0x7F,0x09,0x11,0x11,0x0E}},
    {'c', {0x0E,0x11,0x11,0x11,0x0A}},
    {'d', {0x0E,0x11,0x11,0x09,0x7F}},
    {'e', {0x0E,0x15,0x15,0x15,0x0C}},
    {'f', {0x08,0x3F,0x48,0x40,0x20}},
    {'g', {0x18,0x25,0x25,0x25,0x1E}},
    {'h', {0x7F,0x08,0x10,0x10,0x0F}},
    {'i', {0x00,0x11,0x5F,0x01,0x00}},
    {'j', {0x02,0x01,0x11,0x5E,0x00}},
    {'k', {0x7F,0x04,0x0A,0x11,0x00}},
    {'l', {0x00,0x41,0x7F,0x01,0x00}},
    {'m', {0x1F,0x10,0x0E,0x10,0x0F}},
    {'n', {0x1F,0x08,0x10,0x10,0x0F}},
    {'o', {0x0E,0x11,0x11,0x11,0x0E}},
    {'p', {0x1F,0x14,0x14,0x14,0x08}},
    {'q', {0x08,0x14,0x14,0x0C,0x1F}},
    {'r', {0x1F,0x08,0x10,0x10,0x08}},
    {'s', {0x09,0x15,0x15,0x15,0x02}},
    {'t', {0x10,0x7E,0x11,0x01,0x02}},
    {'u', {0x1E,0x01,0x01,0x02,0x1F}},
    {'v', {0x1C,0x02,0x01,0x02,0x1C}},
    {'w', {0x1E,0x01,0x06,0x01,0x1E}},
    {'x', {0x11,0x0A,0x04,0x0A,0x11}},
    {'y', {0x18,0x05,0x05,0x05,0x1E}},
    {'z', {0x11,0x13,0x15,0x19,0x11}},
    {'?', {0x20,0x40,0x4D,0x50,0x20}}
};

static const uint8_t *OLED_FindGlyph(char ch)
{
    uint32_t i;

    for (i = 0; i < (uint32_t)(sizeof(s_glyphs) / sizeof(s_glyphs[0])); i++) {
        if (s_glyphs[i].ch == ch) {
            return s_glyphs[i].col;
        }
    }

    return s_glyphs[(sizeof(s_glyphs) / sizeof(s_glyphs[0])) - 1U].col;
}

static uint8_t OLED_MapTopY(uint8_t yTop, uint8_t objHeight)
{
    if (objHeight >= OLED_HEIGHT) {
        return 0U;
    }

    if ((uint16_t)yTop + objHeight > OLED_HEIGHT) {
        yTop = (uint8_t)(OLED_HEIGHT - objHeight);
    }

    return (uint8_t)(OLED_HEIGHT - objHeight - yTop);
}

static HAL_StatusTypeDef OLED_WriteCmd(uint8_t cmd)
{
    uint8_t pkt[2];

    pkt[0] = 0x00;
    pkt[1] = cmd;
    return HAL_I2C_Master_Transmit(s_hi2c, OLED_I2C_ADDR, pkt, 2U, 100U);
}

bool OLED_Init(I2C_HandleTypeDef *hi2c)
{
    static const uint8_t initSeq[] = {
        0xAE, 0x20, 0x00, 0x40, 0xA1, 0xC0,
        0x81, 0x7F, 0xA8, 0x3F, 0xD3, 0x00,
        0xDA, 0x12, 0xD5, 0x80, 0xD9, 0xF1,
        0xDB, 0x40, 0x8D, 0x14, 0xA4, 0xA6, 0xAF
    };
    uint32_t i;

    s_hi2c = hi2c;
    if (s_hi2c == NULL) {
        return false;
    }

    for (i = 0; i < (uint32_t)sizeof(initSeq); i++) {
        if (OLED_WriteCmd(initSeq[i]) != HAL_OK) {
            return false;
        }
    }

    OLED_Clear();
    s_cursorX = 0U;
    s_cursorY = 0U;
    return (OLED_Update() == HAL_OK);
}

void OLED_Clear(void)
{
    memset(s_buf, 0, sizeof(s_buf));
}

void OLED_DrawPixel(uint8_t x, uint8_t y, bool on)
{
    uint16_t offset;
    uint8_t bitMask;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    offset = ((uint16_t)(y / 8U) * OLED_WIDTH) + x;
    bitMask = (uint8_t)(1U << (y % 8U));
    if (on) {
        s_buf[offset] |= bitMask;
    } else {
        s_buf[offset] &= (uint8_t)(~bitMask);
    }
}

void OLED_DrawBitmap(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const uint8_t *bitmap, bool invert)
{
    uint8_t row;
    uint8_t col;
    uint8_t bytesPerRow;
    uint8_t srcRow;

    if (bitmap == NULL || width == 0U || height == 0U) {
        return;
    }

    bytesPerRow = (uint8_t)((width + 7U) / 8U);
    for (row = 0U; row < height; row++) {
        srcRow = (uint8_t)((height - 1U) - row);
        for (col = 0U; col < width; col++) {
            uint16_t srcIdx = ((uint16_t)srcRow * bytesPerRow) + (col / 8U);
            uint8_t bitMask = (uint8_t)(0x80U >> (col % 8U));
            bool srcOn = ((bitmap[srcIdx] & bitMask) != 0U);
            bool pixelOn = invert ? (!srcOn) : srcOn;

            OLED_DrawPixel((uint8_t)(x + col), (uint8_t)(y + row), pixelOn);
        }
    }
}

void OLED_DrawCharXY(uint8_t x, uint8_t y, char ch, OLED_FontSize_t fontSize)
{
    const uint8_t *glyph;
    uint8_t scale;
    uint8_t charHeight;
    uint8_t baseY;
    uint8_t col;
    uint8_t row;

    scale = (fontSize == OLED_FONT_BIG) ? 2U : 1U;
    charHeight = (uint8_t)(7U * scale);
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    baseY = OLED_MapTopY(y, charHeight);
    glyph = OLED_FindGlyph(ch);

    for (col = 0U; col < 5U; col++) {
        for (row = 0U; row < 7U; row++) {
            if ((glyph[col] & (1U << row)) != 0U) {
                uint8_t dx;
                uint8_t dy;

                for (dx = 0U; dx < scale; dx++) {
                    for (dy = 0U; dy < scale; dy++) {
                        OLED_DrawPixel((uint8_t)(x + (col * scale) + dx),
                                       (uint8_t)(baseY + (row * scale) + dy),
                                       true);
                    }
                }
            }
        }
    }
}

void OLED_DrawTextXY(uint8_t x, uint8_t y, const char *text, OLED_FontSize_t fontSize)
{
    uint8_t cursorX;
    uint8_t scale;
    uint8_t charStep;

    if (text == NULL) {
        return;
    }

    scale = (fontSize == OLED_FONT_BIG) ? 2U : 1U;
    charStep = (uint8_t)(6U * scale);
    cursorX = x;

    while (*text != '\0' && cursorX < OLED_WIDTH) {
        OLED_DrawCharXY(cursorX, y, *text, fontSize);
        if ((uint16_t)cursorX + charStep > OLED_WIDTH) {
            break;
        }
        cursorX = (uint8_t)(cursorX + charStep);
        text++;
    }
}

void OLED_SetCursor(uint8_t x, uint8_t y)
{
    if (x >= OLED_WIDTH) {
        x = (uint8_t)(OLED_WIDTH - 1U);
    }
    if (y >= OLED_HEIGHT) {
        y = (uint8_t)(OLED_HEIGHT - 1U);
    }

    s_cursorX = x;
    s_cursorY = y;
}

void OLED_WriteText(const char *text, OLED_FontSize_t fontSize)
{
    uint8_t scale;
    uint8_t charStep;
    uint16_t len;
    uint16_t maxChars;

    if (text == NULL) {
        return;
    }

    OLED_DrawTextXY(s_cursorX, s_cursorY, text, fontSize);

    scale = (fontSize == OLED_FONT_BIG) ? 2U : 1U;
    charStep = (uint8_t)(6U * scale);
    len = (uint16_t)strlen(text);
    maxChars = (uint16_t)(OLED_WIDTH - s_cursorX) / charStep;
    if (len > maxChars) {
        len = maxChars;
    }
    s_cursorX = (uint8_t)(s_cursorX + (uint8_t)(len * charStep));
}

void OLED_PrintLine(uint8_t row, const char *text)
{
    uint16_t base;
    uint16_t col;
    uint16_t idx;
    uint8_t mappedRow;

    if (row >= OLED_TEXT_ROWS) {
        return;
    }

    mappedRow = (uint8_t)((OLED_TEXT_ROWS - 1U) - row);
    base = (uint16_t)mappedRow * OLED_WIDTH;
    memset(&s_buf[base], 0, OLED_WIDTH);

    for (col = 0; col < OLED_TEXT_COLS; col++) {
        const uint8_t *glyph;
        char ch = ' ';

        if (text != NULL && text[col] != '\0') {
            ch = text[col];
        }

        glyph = OLED_FindGlyph(ch);
        idx = (uint16_t)(base + (col * 6U));
        if ((idx + 5U) >= (base + OLED_WIDTH)) {
            break;
        }

        /* Clear bit7 to keep 1px gap between rows and reduce touching lines. */
        s_buf[idx + 0U] = (uint8_t)(glyph[0] & 0x7FU);
        s_buf[idx + 1U] = (uint8_t)(glyph[1] & 0x7FU);
        s_buf[idx + 2U] = (uint8_t)(glyph[2] & 0x7FU);
        s_buf[idx + 3U] = (uint8_t)(glyph[3] & 0x7FU);
        s_buf[idx + 4U] = (uint8_t)(glyph[4] & 0x7FU);
        s_buf[idx + 5U] = 0x00;
    }
}

HAL_StatusTypeDef OLED_Update(void)
{
    uint8_t tx[1U + OLED_WIDTH];
    uint8_t page;
    uint16_t offset;

    if (s_hi2c == NULL) {
        return HAL_ERROR;
    }

    tx[0] = 0x40;
    for (page = 0; page < OLED_PAGES; page++) {
        if (OLED_WriteCmd((uint8_t)(0xB0U + page)) != HAL_OK) {
            return HAL_ERROR;
        }
        if (OLED_WriteCmd(0x00U) != HAL_OK) {
            return HAL_ERROR;
        }
        if (OLED_WriteCmd(0x10U) != HAL_OK) {
            return HAL_ERROR;
        }

        offset = (uint16_t)page * OLED_WIDTH;
        memcpy(&tx[1], &s_buf[offset], OLED_WIDTH);
        if (HAL_I2C_Master_Transmit(s_hi2c, OLED_I2C_ADDR, tx, sizeof(tx), 100U) != HAL_OK) {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}
