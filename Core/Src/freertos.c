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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "i2c.h"
#include "oled_icons.h"
#include "oled_ssd1306.h"
#include "stepper_drv.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum {
    BTN_UP_IDX = 0,
    BTN_DOWN_IDX,
    BTN_ENTER_IDX,
    BTN_BACK_IDX,
    BTN_COUNT
} UiButtonIndex_t;

typedef enum {
    COFFEE_SMALL = 0,
    COFFEE_MEDIUM,
    COFFEE_LARGE,
    COFFEE_SIZE_COUNT
} CoffeeSize_t;

typedef enum {
    UI_BOOT = 0,
    UI_HOME,
    UI_PRECHECK,
    UI_LOW_WATER,
    UI_NO_CUP,
    UI_NO_COFFEE,
    UI_BREWING,
    UI_COMPLETE,
    UI_SETTINGS,
    UI_SET_PACKET_COUNT,
    UI_CLEANING,
    UI_SYSTEM_INFO
} UiState_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define J1_SPEED              1000
#define J1_ACCEL_MAX          20000
#define J1_HOME_SPEED         10000
#define J2_SPEED              30000
#define J2_ACCEL_MAX          20000
#define J2_HOME_SPEED         2000
#define J1_MOVE_TIMEOUT_MS    15000U
#define J2_MOVE_TIMEOUT_MS    15000U
#define HOME_SEEK_DISTANCE    25600

#define UI_LOOP_MS            20U
#define BTN_DEBOUNCE_MS       30U
#define PACKET_MAX            18U
#define PACKET_WARN_THRESHOLD 3U
#define BOOT_TIMEOUT_MS       2000U
#define BREW_RENDER_MS        120U

#define SENSOR_WATER_OK_LEVEL    GPIO_PIN_SET
#define SENSOR_CUP_PRESENT_LEVEL GPIO_PIN_RESET

const long offsetToHome = 470;
const long stepSpaceKhay = 1333;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static StepperAxis motor_j1;
static StepperAxis motor_j2;

static bool oledReady = false;
static TickType_t oledTick = 0U;

static volatile UiButtonIndex_t s_lastBtnEvent = BTN_COUNT;
static volatile uint32_t s_btnLastIrqTick[BTN_COUNT] = {0U};
static UiState_t s_uiState = UI_BOOT;
static CoffeeSize_t s_homeSize = COFFEE_MEDIUM;
static uint8_t s_packetCount = PACKET_MAX;
static uint8_t s_packetEdit = PACKET_MAX;
static uint8_t s_settingsIndex = 0U;
static bool s_forceRender = true;
static TickType_t s_stateTick = 0U;
static TickType_t s_lastRenderTick = 0U;
static TickType_t s_brewStartTick = 0U;
static uint32_t s_brewDurationMs = 12000U;

static const uint16_t s_sizeMl[COFFEE_SIZE_COUNT] = {100U, 150U, 200U};
static const char *s_sizeName[COFFEE_SIZE_COUNT] = {"SMALL", "MEDIUM", "LARGE"};
static const char *s_levelName[COFFEE_SIZE_COUNT] = {"LOW", "MEDIUM", "HIGH"};
static const char *s_brewStepName[6] = {
    "POSITIONING",
    "CUTTING PACKET",
    "DISPENSING COFFEE",
    "ADDING WATER",
    "MIXING",
    "FINISHING"
};

/* EEPROM placeholder in RAM. Replace with real EEPROM read/write later. */
static uint8_t s_packetNv = PACKET_MAX;
/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId j1TaskHandle;
osThreadId j2TaskHandle;
osSemaphoreId binarySem_motorJ1Handle;
osSemaphoreId binarySem_motorJ2Handle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void Oled_PrintLineTop(uint8_t rowTop, const char *text);
static void Oled_BuildCenteredLine(char *dst, const char *text);
static void Oled_PrintCenteredTop(uint8_t rowTop, const char *text);
static void Oled_DrawPixelTop(uint8_t x, uint8_t y, bool on);
static void Oled_DrawRectTop(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);
static void Oled_FillRectTop(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on);

static UiButtonIndex_t Ui_ButtonIndexFromPin(uint16_t gpioPin);
static void Ui_ConfigButtonExtiHardware(void);
static void Ui_PollButtons(void);
static bool Ui_TakeShort(UiButtonIndex_t idx);
static bool Ui_TakeLong(UiButtonIndex_t idx);
static bool Ui_WaterOk(void);
static bool Ui_CupPresent(void);
static uint8_t Ui_LoadPacketCount(void);
static void Ui_SavePacketCount(uint8_t count);
static OledCafeLevel_t Ui_SizeToLevel(CoffeeSize_t size);
static void Ui_RenderStatusBar(void);
static void Ui_RenderHome(void);
static void Ui_RenderBoot(void);
static void Ui_RenderSimpleScreen(const char *l1, const char *l2, const char *l3, bool showStatus);
static void Ui_RenderProgress(uint8_t percent);
static void Ui_RenderBrewing(void);
static void Ui_RenderComplete(void);
static void Ui_RenderSettings(void);
static void Ui_RenderPacketEditor(void);
static void Ui_RenderSystemInfo(void);
static void Ui_RenderCleaning(void);
static void Ui_EnterState(UiState_t next);
static void Ui_RenderCurrent(void);
static void Ui_HandleState(void);

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2) {
        Stepper_OnPulseISR(&motor_j1);
    }
    if (htim->Instance == TIM3) {
        Stepper_OnPulseISR(&motor_j2);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    UiButtonIndex_t idx = Ui_ButtonIndexFromPin(GPIO_Pin);

    if (idx < BTN_COUNT) {
        uint32_t nowMs = HAL_GetTick();

        if ((nowMs - s_btnLastIrqTick[idx]) >= BTN_DEBOUNCE_MS) {
            s_btnLastIrqTick[idx] = nowMs;
            s_lastBtnEvent = idx;
        }
    }
}
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTaskJ1(void const * argument);
void StartTaskJ2(void const * argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* definition and creation of binarySem_motorJ1 */
  osSemaphoreDef(binarySem_motorJ1);
  binarySem_motorJ1Handle = osSemaphoreCreate(osSemaphore(binarySem_motorJ1), 1);

  /* definition and creation of binarySem_motorJ2 */
  osSemaphoreDef(binarySem_motorJ2);
  binarySem_motorJ2Handle = osSemaphoreCreate(osSemaphore(binarySem_motorJ2), 1);

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
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of j1Task */
  osThreadDef(j1Task, StartTaskJ1, osPriorityHigh, 0, 512);
  j1TaskHandle = osThreadCreate(osThread(j1Task), NULL);

  /* definition and creation of j2Task */
  osThreadDef(j2Task, StartTaskJ2, osPriorityHigh, 0, 512);
  j2TaskHandle = osThreadCreate(osThread(j2Task), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
static void Oled_PrintLineTop(uint8_t rowTop, const char *text)
{
    uint8_t drvRow;

    if (rowTop >= OLED_TEXT_ROWS) {
        return;
    }
    drvRow = (uint8_t)((OLED_TEXT_ROWS - 1U) - rowTop);
    OLED_PrintLine(drvRow, text);
}

static void Oled_BuildCenteredLine(char *dst, const char *text)
{
    size_t len;
    size_t start;

    memset(dst, ' ', OLED_TEXT_COLS);
    dst[OLED_TEXT_COLS] = '\0';
    if (text == NULL) {
        return;
    }

    len = strlen(text);
    if (len > OLED_TEXT_COLS) {
        len = OLED_TEXT_COLS;
    }
    start = (OLED_TEXT_COLS - len) / 2U;
    memcpy(&dst[start], text, len);
}

static void Oled_PrintCenteredTop(uint8_t rowTop, const char *text)
{
    char line[OLED_TEXT_COLS + 1U];
    Oled_BuildCenteredLine(line, text);
    Oled_PrintLineTop(rowTop, line);
}

static void Oled_DrawPixelTop(uint8_t x, uint8_t y, bool on)
{
    uint8_t mappedY = OLED_IconMapY(y, 1U);
    OLED_DrawPixel(x, mappedY, on);
}

static void Oled_DrawRectTop(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    uint8_t i;

    if (w == 0U || h == 0U) {
        return;
    }

    for (i = 0U; i < w; i++) {
        Oled_DrawPixelTop((uint8_t)(x + i), y, on);
        Oled_DrawPixelTop((uint8_t)(x + i), (uint8_t)(y + h - 1U), on);
    }
    for (i = 0U; i < h; i++) {
        Oled_DrawPixelTop(x, (uint8_t)(y + i), on);
        Oled_DrawPixelTop((uint8_t)(x + w - 1U), (uint8_t)(y + i), on);
    }
}

static void Oled_FillRectTop(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool on)
{
    uint8_t xi;
    uint8_t yi;

    for (yi = 0U; yi < h; yi++) {
        for (xi = 0U; xi < w; xi++) {
            Oled_DrawPixelTop((uint8_t)(x + xi), (uint8_t)(y + yi), on);
        }
    }
}

static UiButtonIndex_t Ui_ButtonIndexFromPin(uint16_t gpioPin)
{
    if (gpioPin == BTN_UP_Pin) {
        return BTN_UP_IDX;
    }
    if (gpioPin == BTN_DOWN_Pin) {
        return BTN_DOWN_IDX;
    }
    if (gpioPin == BTN_ENTER_Pin) {
        return BTN_ENTER_IDX;
    }
    if (gpioPin == BTN_BACK_Pin) {
        return BTN_BACK_IDX;
    }

    return BTN_COUNT;
}

static void Ui_ConfigButtonExtiHardware(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Re-apply button EXTI settings at runtime to survive CubeMX re-generate. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = BTN_ENTER_Pin | BTN_BACK_Pin | BTN_DOWN_Pin | BTN_UP_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
}

static void Ui_PollButtons(void)
{
    /* Event comes directly from EXTI callback. Nothing to poll here. */
}

static bool Ui_TakeShort(UiButtonIndex_t idx)
{
    if (s_lastBtnEvent == idx) {
        s_lastBtnEvent = BTN_COUNT;
        return true;
    }

    return false;
}

static bool Ui_TakeLong(UiButtonIndex_t idx)
{
    (void)idx;
    return false;
}

static bool Ui_WaterOk(void)
{
    return (HAL_GPIO_ReadPin(SENSOR_MUCNUOC_GPIO_Port, SENSOR_MUCNUOC_Pin) == SENSOR_WATER_OK_LEVEL);
}

static bool Ui_CupPresent(void)
{
    return (HAL_GPIO_ReadPin(SENSOR_COC_GPIO_Port, SENSOR_COC_Pin) == SENSOR_CUP_PRESENT_LEVEL);
}

static uint8_t Ui_LoadPacketCount(void)
{
    if (s_packetNv > PACKET_MAX) {
        return PACKET_MAX;
    }
    return s_packetNv;
}

static void Ui_SavePacketCount(uint8_t count)
{
    if (count > PACKET_MAX) {
        count = PACKET_MAX;
    }
    s_packetNv = count;
}

static OledCafeLevel_t Ui_SizeToLevel(CoffeeSize_t size)
{
    switch (size) {
    case COFFEE_SMALL:
        return OLED_CAFE_LEVEL_LOW;
    case COFFEE_MEDIUM:
        return OLED_CAFE_LEVEL_MID;
    case COFFEE_LARGE:
    default:
        return OLED_CAFE_LEVEL_HIGH;
    }
}


static void Ui_RenderHome(void)
{
    char line[OLED_TEXT_COLS + 1U];

    OLED_Clear();
    (void)snprintf(line, sizeof(line), "%s", s_levelName[s_homeSize]);
    OLED_IconDrawCafeState(0, 13, Ui_SizeToLevel(s_homeSize), true, true);
    OLED_DrawTextXY(54, 15, line, OLED_FONT_BIG);
    OLED_DrawTextXY(0, 50, "* Enter / # Setiing", OLED_FONT_SMALL);
    (void)OLED_Update();
}

static void Ui_RenderBoot(void)
{
    OLED_IconShowCafeLogoCentered(true, false);
    Oled_PrintCenteredTop(7U, "COFFEE         MAKER");
    (void)OLED_Update();
}

static void Ui_RenderSimpleScreen(const char *l1, const char *l2, const char *l3, bool showStatus)
{
    OLED_Clear();
    if (l1 != NULL) {
        Oled_PrintCenteredTop(1U, l1);
    }
    if (l2 != NULL) {
        Oled_PrintCenteredTop(3U, l2);
    }
    if (l3 != NULL) {
        Oled_PrintCenteredTop(5U, l3);
    }
    (void)OLED_Update();
}

static void Ui_RenderProgress(uint8_t percent)
{
    uint8_t fill;

    if (percent > 100U) {
        percent = 100U;
    }

    fill = (uint8_t)((percent * 90U) / 100U);
    Oled_DrawRectTop(19U, 50U, 92U, 10U, true);
    if (fill > 0U) {
        Oled_FillRectTop(20U, 51U, fill, 8U, true);
    }
}

static void Ui_RenderBrewing(void)
{
    uint32_t elapsedMs;
    uint8_t percent;
    uint8_t step;

    elapsedMs = (uint32_t)(xTaskGetTickCount() - s_brewStartTick) * (uint32_t)portTICK_PERIOD_MS;
    if (elapsedMs >= s_brewDurationMs) {
        percent = 100U;
        step = 5U;
    } else {
        percent = (uint8_t)((elapsedMs * 100U) / s_brewDurationMs);
        step = (uint8_t)((elapsedMs * 6U) / s_brewDurationMs);
        if (step > 5U) {
            step = 5U;
        }
    }

    OLED_Clear();
    OLED_DrawTextXY(24, 0, "BREWING", OLED_FONT_BIG);
    Oled_PrintCenteredTop(3U, s_brewStepName[step]);
    Ui_RenderProgress(percent);
    (void)OLED_Update();
}

static void Ui_RenderComplete(void)
{
    OLED_Clear();
    Oled_PrintCenteredTop(1U, "ENJOY YOUR COFFEE");
    OLED_IconDrawCafeState(40, 13, Ui_SizeToLevel(s_homeSize), true, true);
    (void)OLED_Update();
}

static void Ui_RenderSettings(void)
{
    const char *item0 = (s_settingsIndex == 0U) ? "> COFFEE PACKETS     " : "  COFFEE PACKETS     ";
    const char *item1 = (s_settingsIndex == 1U) ? "> CLEANING           " : "  CLEANING           ";
    const char *item2 = (s_settingsIndex == 2U) ? "> SYSTEM INFO        " : "  SYSTEM INFO        ";

    OLED_Clear();
    Oled_PrintCenteredTop(7U, "SETTINGS");
    OLED_DrawTextXY(0, 12, item0, OLED_FONT_SMALL);
    OLED_DrawTextXY(0, 24, item1, OLED_FONT_SMALL);
    OLED_DrawTextXY(0, 36, item2, OLED_FONT_SMALL);
    OLED_DrawTextXY(15, 50, "* Enter / # back", OLED_FONT_SMALL);
    (void)OLED_Update();
}

static void Ui_RenderPacketEditor(void)
{
    char line[OLED_TEXT_COLS + 1U];

    OLED_Clear();
    Oled_PrintCenteredTop(7U, "COFFEE PACKETS");
    (void)snprintf(line, sizeof(line), " %02u ", (unsigned int)s_packetEdit);
    OLED_DrawTextXY(26, 24, line, OLED_FONT_BIG);
    OLED_DrawTextXY(15, 50, "* Enter / # back", OLED_FONT_SMALL);
    (void)OLED_Update();
}

static void Ui_RenderSystemInfo(void)
{
    char line[OLED_TEXT_COLS + 1U];

    OLED_Clear();
    Oled_PrintCenteredTop(7U, "SYSTEM INFO");
    (void)snprintf(line, sizeof(line), "PACKETS: %02u", (unsigned int)s_packetCount);
    OLED_DrawTextXY(0, 14, line, OLED_FONT_SMALL);
    (void)snprintf(line, sizeof(line), "WATER: %s  CUP: %s", Ui_WaterOk() ? "OK" : "LOW", Ui_CupPresent() ? "OK" : "NO");
    OLED_DrawTextXY(0, 28, line, OLED_FONT_SMALL);
    OLED_DrawTextXY(70, 50, "# back", OLED_FONT_SMALL);
    (void)OLED_Update();
}

static void Ui_RenderCleaning(void)
{
    OLED_Clear();
    Oled_PrintCenteredTop(7U, "CLEANING");
    OLED_DrawTextXY(15, 50, "* Enter / # back", OLED_FONT_SMALL);
    (void)OLED_Update();
}

static void Ui_EnterState(UiState_t next)
{
    s_uiState = next;
    s_stateTick = xTaskGetTickCount();
    s_forceRender = true;

    if (next == UI_BREWING) {
        s_brewStartTick = s_stateTick;
        s_brewDurationMs = (uint32_t)s_sizeMl[s_homeSize] * 80U;
        if (s_brewDurationMs < 8000U) {
            s_brewDurationMs = 8000U;
        }
    }
}

static void Ui_RenderCurrent(void)
{
    switch (s_uiState) {
    case UI_BOOT:
        Ui_RenderBoot();
        break;
    case UI_HOME:
        Ui_RenderHome();
        break;
    case UI_PRECHECK:
        Ui_RenderSimpleScreen("PRECHECK", "CHECKING SENSORS", "PLEASE WAIT", true);
        break;
    case UI_LOW_WATER:
        OLED_Clear();
        OLED_DrawBitmap(48, 36, OLED_ICON_WARNING_W, OLED_ICON_WARNING_H, epd_bitmap_warning, true);
        OLED_DrawTextXY(15, 34,"LOW WATER",OLED_FONT_BIG);
        OLED_DrawTextXY(14, 54,"REFILL WATER TANK",OLED_FONT_SMALL);
        OLED_DrawTextXY(118, 0, "#", OLED_FONT_SMALL);
        (void)OLED_Update();
        break;
    case UI_NO_CUP:
        OLED_Clear();
        OLED_DrawBitmap(48, 36, OLED_ICON_WARNING_W, OLED_ICON_WARNING_H, epd_bitmap_warning, true);
        OLED_DrawTextXY(15, 34,"PLACE CUP",OLED_FONT_BIG);
        OLED_DrawTextXY(10, 54,"INSERT CUP TO START",OLED_FONT_SMALL);
        OLED_DrawTextXY(118, 0, "#", OLED_FONT_SMALL);
        (void)OLED_Update();
        break;
    case UI_NO_COFFEE:
        OLED_Clear();
        OLED_DrawBitmap(48, 36, OLED_ICON_WARNING_W, OLED_ICON_WARNING_H, epd_bitmap_warning, true);
        OLED_DrawTextXY(15, 34,"NO COFFEE",OLED_FONT_BIG);
        OLED_DrawTextXY(10, 54,"LOAD COFFEE PACKETS",OLED_FONT_SMALL);
        OLED_DrawTextXY(118, 0, "#", OLED_FONT_SMALL);
        (void)OLED_Update();
        break;
    case UI_BREWING:
        Ui_RenderBrewing();
        break;
    case UI_COMPLETE:
        Ui_RenderComplete();
        break;
    case UI_SETTINGS:
        Ui_RenderSettings();
        break;
    case UI_SET_PACKET_COUNT:
        Ui_RenderPacketEditor();
        break;
    case UI_CLEANING:
        Ui_RenderCleaning();
        break;
    case UI_SYSTEM_INFO:
        Ui_RenderSystemInfo();
        break;
    default:
        break;
    }
}

static void Ui_HandleState(void)
{
    TickType_t now = xTaskGetTickCount();
    uint32_t elapsedMs = (uint32_t)(now - s_stateTick) * (uint32_t)portTICK_PERIOD_MS;
    uint32_t brewMs = (uint32_t)(now - s_brewStartTick) * (uint32_t)portTICK_PERIOD_MS;

    switch (s_uiState) {
    case UI_BOOT:
        if (elapsedMs >= BOOT_TIMEOUT_MS) {
            Ui_EnterState(UI_HOME);
        }
        break;
    case UI_HOME:
        if (Ui_TakeShort(BTN_DOWN_IDX)) {
            s_homeSize = (s_homeSize == COFFEE_SMALL) ? COFFEE_LARGE : (CoffeeSize_t)(s_homeSize - 1);
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_UP_IDX)) {
            s_homeSize = (CoffeeSize_t)((s_homeSize + 1U) % COFFEE_SIZE_COUNT);
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_SETTINGS);
            break;
        }
        if (Ui_TakeShort(BTN_ENTER_IDX)) {
            Ui_EnterState(UI_PRECHECK);
        }
        break;
    case UI_PRECHECK:
        if (!Ui_WaterOk()) {
            Ui_EnterState(UI_LOW_WATER);
        } else if (!Ui_CupPresent()) {
            Ui_EnterState(UI_NO_CUP);
        } else if (s_packetCount == 0U) {
            Ui_EnterState(UI_NO_COFFEE);
        } else {
            Ui_EnterState(UI_BREWING);
        }
        break;
    case UI_LOW_WATER:
        if (Ui_WaterOk() || Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_HOME);
        }
        break;
    case UI_NO_CUP:
        if (Ui_CupPresent() || Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_HOME);
        }
        break;
    case UI_NO_COFFEE:
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_HOME);
        }
        break;
    case UI_BREWING:
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            HAL_GPIO_WritePin(MOTOR_KHUAY_GPIO_Port, MOTOR_KHUAY_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin, GPIO_PIN_RESET);
            Ui_EnterState(UI_HOME);
            break;
        }
        if (brewMs >= s_brewDurationMs) {
            if (s_packetCount > 0U) {
                s_packetCount--;
                Ui_SavePacketCount(s_packetCount);
            }
            Ui_EnterState(UI_COMPLETE);
            Ui_TakeShort(BTN_ENTER_IDX); // tránh trường hợp nhấn đúp khiến màn hình complete thoát luôn
        }
        break;
    case UI_COMPLETE:
        if (Ui_TakeShort(BTN_ENTER_IDX) || Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_HOME);
        }
        break;
    case UI_SETTINGS:
        if (Ui_TakeShort(BTN_UP_IDX) && s_settingsIndex > 0U) {
            s_settingsIndex--;
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_DOWN_IDX) && s_settingsIndex < 2U) {
            s_settingsIndex++;
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_HOME);
            break;
        }
        if (Ui_TakeShort(BTN_ENTER_IDX)) {
            if (s_settingsIndex == 0U) {
                s_packetEdit = s_packetCount;
                Ui_EnterState(UI_SET_PACKET_COUNT);
            } else if (s_settingsIndex == 1U) {
                Ui_EnterState(UI_CLEANING);
            } else {
                Ui_EnterState(UI_SYSTEM_INFO);
            }
        }
        break;
    case UI_SET_PACKET_COUNT:
        if (Ui_TakeShort(BTN_UP_IDX) && s_packetEdit < PACKET_MAX) {
            s_packetEdit++;
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_DOWN_IDX) && s_packetEdit > 0U) {
            s_packetEdit--;
            s_forceRender = true;
        }
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_SETTINGS);
            break;
        }
        if (Ui_TakeShort(BTN_ENTER_IDX)) {
            s_packetCount = s_packetEdit;
            Ui_SavePacketCount(s_packetCount);
            Ui_EnterState(UI_SETTINGS);
        }
        break;
    case UI_CLEANING:
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_SETTINGS);
        } else if (Ui_TakeShort(BTN_ENTER_IDX)) {
            Ui_RenderSimpleScreen("CLEANING", "RUNNING...", "PLEASE WAIT", false);
            osDelay(1500);
            Ui_EnterState(UI_SETTINGS);
        }
        break;
    case UI_SYSTEM_INFO:
        if (Ui_TakeShort(BTN_BACK_IDX)) {
            Ui_EnterState(UI_SETTINGS);
        }
        break;
    default:
        break;
    }
}

/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* init code for USB_DEVICE */
	osDelay(1000);
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  TIM1->CCR1 = 0;
  TIM1->CCR2 = 0;
  TIM1->CCR3 = 0;

  HAL_GPIO_WritePin(MOTOR_KHUAY_GPIO_Port, MOTOR_KHUAY_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PUMP_GPIO_Port, PUMP_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(BOIL_GPIO_Port, BOIL_Pin, GPIO_PIN_RESET);

    Ui_ConfigButtonExtiHardware();

  oledReady = OLED_Init(&hi2c1);
  if (!oledReady) {
      for (;;) {
          osDelay(1000);
      }
  }

    s_packetCount = Ui_LoadPacketCount();
    s_lastBtnEvent = BTN_COUNT;
  Ui_EnterState(UI_BOOT);

  for (;;) {
      TickType_t now;
      bool periodicRender;

      Ui_PollButtons();
      Ui_HandleState();

      now = xTaskGetTickCount();
      periodicRender = ((now - s_lastRenderTick) >= pdMS_TO_TICKS(BREW_RENDER_MS));
      if (s_forceRender || (periodicRender && (s_uiState == UI_HOME || s_uiState == UI_BREWING || s_uiState == UI_SYSTEM_INFO))) {
          Ui_RenderCurrent();
          s_forceRender = false;
          s_lastRenderTick = now;
          oledTick = now;
      }

      osDelay(UI_LOOP_MS);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTaskJ1 */
/**
* @brief Function implementing the j1Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskJ1 */
void StartTaskJ1(void const * argument)
{
  /* USER CODE BEGIN StartTaskJ1 */
    StepperHwConfig hw;
    StepperProfile profile;

    osDelay(200);
    hw.htim = &htim2;
    hw.tim = TIM2;
    hw.channel = TIM_CHANNEL_1;
    hw.dirPort = J1_DIR_GPIO_Port;
    hw.dirPin = J1_DIR_Pin;
    hw.enPort = J1_EN_GPIO_Port;
    hw.enPin = J1_EN_Pin;
    hw.limPort = LIM_M1_GPIO_Port;
    hw.limPin = LIM_M1_Pin;
    hw.dirCwLevel = GPIO_PIN_RESET;
    hw.enableActiveLow = true;

    profile.maxSpeed = J1_SPEED;
    profile.accel = J1_ACCEL_MAX;
    profile.homeSpeed = J1_HOME_SPEED;
    profile.homeSeekDistance = HOME_SEEK_DISTANCE;
    profile.homeOffset = offsetToHome;
    profile.timeoutMs = J1_MOVE_TIMEOUT_MS;

    Stepper_Init(&motor_j1, &hw, &profile);
    Stepper_StartPwm(&motor_j1);

    for(;;)
    {
        Stepper_Process1ms(&motor_j1);
        osDelay(1);
    }
  /* USER CODE END StartTaskJ1 */
}

/* USER CODE BEGIN Header_StartTaskJ2 */
/**
* @brief Function implementing the j2Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTaskJ2 */
void StartTaskJ2(void const * argument)
{
  /* USER CODE BEGIN StartTaskJ2 */
    StepperHwConfig hw;
    StepperProfile profile;

    osDelay(200);
    hw.htim = &htim3;
    hw.tim = TIM3;
    hw.channel = TIM_CHANNEL_1;
    hw.dirPort = J2_DIR_GPIO_Port;
    hw.dirPin = J2_DIR_Pin;
    hw.enPort = J2_EN_GPIO_Port;
    hw.enPin = J2_EN_Pin;
    hw.limPort = LIM_M2_GPIO_Port;
    hw.limPin = LIM_M2_Pin;
    hw.dirCwLevel = GPIO_PIN_RESET;
    hw.enableActiveLow = true;

    profile.maxSpeed = J2_SPEED;
    profile.accel = J2_ACCEL_MAX;
    profile.homeSpeed = J2_HOME_SPEED;
    profile.homeSeekDistance = HOME_SEEK_DISTANCE;
    profile.homeOffset = 5;
    profile.timeoutMs = J2_MOVE_TIMEOUT_MS;

    Stepper_Init(&motor_j2, &hw, &profile);
    Stepper_StartPwm(&motor_j2);

    for(;;)
    {
        Stepper_Process1ms(&motor_j2);
        osDelay(1);
    }
  /* USER CODE END StartTaskJ2 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

