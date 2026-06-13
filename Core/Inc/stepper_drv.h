#ifndef STEPPER_DRV_H
#define STEPPER_DRV_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    STEPPER_STATE_IDLE = 0,
    STEPPER_STATE_MOVE,
    STEPPER_STATE_HOME_SEEK,
    STEPPER_STATE_HOME_OFFSET,
    STEPPER_STATE_ERROR
} StepperState;

typedef enum {
    STEPPER_ERR_NONE = 0,
    STEPPER_ERR_TIMEOUT,
    STEPPER_ERR_HOME_NOT_FOUND
} StepperError;

typedef struct {
    TIM_HandleTypeDef *htim;
    TIM_TypeDef *tim;
    uint32_t channel;
    GPIO_TypeDef *dirPort;
    uint16_t dirPin;
    GPIO_TypeDef *enPort;
    uint16_t enPin;
    GPIO_TypeDef *limPort;
    uint16_t limPin;
    GPIO_PinState dirCwLevel;
    bool enableActiveLow;
} StepperHwConfig;

typedef struct {
    float maxSpeed;
    float accel;
    float homeSpeed;
    int32_t homeSeekDistance;
    int32_t homeOffset;
    uint32_t timeoutMs;
} StepperProfile;

typedef struct {
    StepperHwConfig hw;
    StepperProfile cfg;
    volatile StepperState state;
    volatile StepperError error;
    volatile int32_t position;
    int32_t target;
    int32_t homeStartPos;
    int8_t direction;
    float speedAbs;
    float commandMaxSpeed;
    uint32_t stepIntervalUs;
    uint32_t startTickMs;
    bool initialized;
} StepperAxis;

void Stepper_Init(StepperAxis *axis, const StepperHwConfig *hw, const StepperProfile *profile);
void Stepper_StartPwm(StepperAxis *axis);
void Stepper_Enable(StepperAxis *axis, bool enable);
bool Stepper_MoveTo(StepperAxis *axis, int32_t target);
bool Stepper_Home(StepperAxis *axis);
void Stepper_Stop(StepperAxis *axis);
void Stepper_Process1ms(StepperAxis *axis);
void Stepper_OnPulseISR(StepperAxis *axis);
void Stepper_ClearError(StepperAxis *axis);

bool Stepper_IsBusy(const StepperAxis *axis);
bool Stepper_IsIdle(const StepperAxis *axis);
bool Stepper_HasError(const StepperAxis *axis);

#endif
