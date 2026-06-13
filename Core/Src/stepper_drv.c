#include "stepper_drv.h"

#include <math.h>
#include <stdlib.h>

#define STEPPER_MIN_SPEED_SPS 10.0f
#define STEPPER_MIN_ARR_US 60U
#define STEPPER_MAX_ARR_US 65000U
#define STEPPER_PULSE_WIDTH_TICKS 2U

static void Stepper_SetPulseEnabled(StepperAxis *axis, bool enabled)
{
    switch (axis->hw.channel) {
    case TIM_CHANNEL_1:
        axis->hw.tim->CCR1 = enabled ? STEPPER_PULSE_WIDTH_TICKS : 0U;
        break;
    case TIM_CHANNEL_2:
        axis->hw.tim->CCR2 = enabled ? STEPPER_PULSE_WIDTH_TICKS : 0U;
        break;
    case TIM_CHANNEL_3:
        axis->hw.tim->CCR3 = enabled ? STEPPER_PULSE_WIDTH_TICKS : 0U;
        break;
    case TIM_CHANNEL_4:
        axis->hw.tim->CCR4 = enabled ? STEPPER_PULSE_WIDTH_TICKS : 0U;
        break;
    default:
        break;
    }
}

static void Stepper_UpdateDirPin(StepperAxis *axis)
{
    GPIO_PinState state = (axis->direction > 0) ? axis->hw.dirCwLevel :
        (axis->hw.dirCwLevel == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(axis->hw.dirPort, axis->hw.dirPin, state);
}

static uint32_t Stepper_ClampArr(float speedAbs)
{
    uint32_t arr;

    if (speedAbs < STEPPER_MIN_SPEED_SPS) {
        speedAbs = STEPPER_MIN_SPEED_SPS;
    }

    arr = (uint32_t)(1000000.0f / speedAbs);
    if (arr < STEPPER_MIN_ARR_US) {
        arr = STEPPER_MIN_ARR_US;
    }
    if (arr > STEPPER_MAX_ARR_US) {
        arr = STEPPER_MAX_ARR_US;
    }
    return arr;
}

void Stepper_Init(StepperAxis *axis, const StepperHwConfig *hw, const StepperProfile *profile)
{
    axis->hw = *hw;
    axis->cfg = *profile;
    axis->state = STEPPER_STATE_IDLE;
    axis->error = STEPPER_ERR_NONE;
    axis->position = 0;
    axis->target = 0;
    axis->homeStartPos = 0;
    axis->direction = 1;
    axis->speedAbs = 0.0f;
    axis->commandMaxSpeed = profile->maxSpeed;
    axis->stepIntervalUs = 1000U;
    axis->startTickMs = 0U;
    axis->initialized = true;

    axis->hw.tim->ARR = axis->stepIntervalUs;
    Stepper_SetPulseEnabled(axis, false);
    Stepper_Enable(axis, false);
}

void Stepper_StartPwm(StepperAxis *axis)
{
    if (!axis->initialized) {
        return;
    }
    HAL_TIM_PWM_Start_IT(axis->hw.htim, axis->hw.channel);
}

void Stepper_Enable(StepperAxis *axis, bool enable)
{
    GPIO_PinState pinState;

    if (axis->hw.enableActiveLow) {
        pinState = enable ? GPIO_PIN_RESET : GPIO_PIN_SET;
    } else {
        pinState = enable ? GPIO_PIN_SET : GPIO_PIN_RESET;
    }

    HAL_GPIO_WritePin(axis->hw.enPort, axis->hw.enPin, pinState);
}

bool Stepper_MoveTo(StepperAxis *axis, int32_t target)
{
    if (!axis->initialized || Stepper_IsBusy(axis)) {
        return false;
    }

    axis->error = STEPPER_ERR_NONE;
    axis->target = target;
    axis->commandMaxSpeed = axis->cfg.maxSpeed;
    axis->state = STEPPER_STATE_MOVE;
    axis->startTickMs = HAL_GetTick();
    Stepper_Enable(axis, true);
    return true;
}

bool Stepper_Home(StepperAxis *axis)
{
    if (!axis->initialized || Stepper_IsBusy(axis)) {
        return false;
    }

    axis->error = STEPPER_ERR_NONE;
    axis->homeStartPos = axis->position;
    axis->direction = -1;
    axis->commandMaxSpeed = axis->cfg.homeSpeed;
    axis->state = STEPPER_STATE_HOME_SEEK;
    axis->startTickMs = HAL_GetTick();
    Stepper_Enable(axis, true);
    return true;
}

void Stepper_Stop(StepperAxis *axis)
{
    axis->speedAbs = 0.0f;
    axis->state = STEPPER_STATE_IDLE;
    Stepper_SetPulseEnabled(axis, false);
}

void Stepper_ClearError(StepperAxis *axis)
{
    axis->error = STEPPER_ERR_NONE;
    if (axis->state == STEPPER_STATE_ERROR) {
        axis->state = STEPPER_STATE_IDLE;
    }
}

bool Stepper_IsBusy(const StepperAxis *axis)
{
    return (axis->state == STEPPER_STATE_MOVE) ||
           (axis->state == STEPPER_STATE_HOME_SEEK) ||
           (axis->state == STEPPER_STATE_HOME_OFFSET);
}

bool Stepper_IsIdle(const StepperAxis *axis)
{
    return axis->state == STEPPER_STATE_IDLE;
}

bool Stepper_HasError(const StepperAxis *axis)
{
    return axis->state == STEPPER_STATE_ERROR;
}

void Stepper_Process1ms(StepperAxis *axis)
{
    float desiredSpeedAbs = 0.0f;
    float maxDelta;
    int32_t dist;
    float stopSpeed;

    if (!Stepper_IsBusy(axis)) {
        return;
    }

    if ((HAL_GetTick() - axis->startTickMs) > axis->cfg.timeoutMs) {
        axis->state = STEPPER_STATE_ERROR;
        axis->error = STEPPER_ERR_TIMEOUT;
        Stepper_Stop(axis);
        return;
    }

    if (axis->state == STEPPER_STATE_HOME_SEEK) {
        if (HAL_GPIO_ReadPin(axis->hw.limPort, axis->hw.limPin) == GPIO_PIN_RESET) {
            axis->position = 0;
            axis->target = axis->cfg.homeOffset;
            axis->direction = 1;                        /* homeOffset luôn dương, set trước để ISR không đếm sai */
            axis->speedAbs = 0.0f;                      /* Reset tốc độ — không đảo chiều ở tốc độ cao */
            axis->commandMaxSpeed = axis->cfg.maxSpeed; /* Di chuyển offset ở tốc độ thường, accel đúng */
            axis->state = STEPPER_STATE_HOME_OFFSET;
            axis->startTickMs = HAL_GetTick();
        } else {
            int32_t traveled = axis->homeStartPos - axis->position;
            if (traveled >= axis->cfg.homeSeekDistance) {
                axis->state = STEPPER_STATE_ERROR;
                axis->error = STEPPER_ERR_HOME_NOT_FOUND;
                Stepper_Stop(axis);
                return;
            }
        }
    }

    if (axis->state == STEPPER_STATE_HOME_SEEK) {
        desiredSpeedAbs = axis->cfg.homeSpeed;
        axis->direction = -1;
    } else {
        dist = axis->target - axis->position;
        if (dist == 0) {
            axis->speedAbs = 0.0f;
            axis->state = STEPPER_STATE_IDLE;
            Stepper_SetPulseEnabled(axis, false);
            return;
        }

        axis->direction = (dist >= 0) ? 1 : -1;
        stopSpeed = sqrtf(2.0f * axis->cfg.accel * (float)labs(dist));
        desiredSpeedAbs = stopSpeed;
        if (desiredSpeedAbs > axis->commandMaxSpeed) {
            desiredSpeedAbs = axis->commandMaxSpeed;
        }
        if (desiredSpeedAbs < STEPPER_MIN_SPEED_SPS) {
            desiredSpeedAbs = STEPPER_MIN_SPEED_SPS;
        }
    }

    maxDelta = axis->cfg.accel * 0.001f;
    if (axis->speedAbs < desiredSpeedAbs) {
        axis->speedAbs += maxDelta;
        if (axis->speedAbs > desiredSpeedAbs) {
            axis->speedAbs = desiredSpeedAbs;
        }
    } else if (axis->speedAbs > desiredSpeedAbs) {
        axis->speedAbs -= maxDelta;
        if (axis->speedAbs < desiredSpeedAbs) {
            axis->speedAbs = desiredSpeedAbs;
        }
    }

    axis->stepIntervalUs = Stepper_ClampArr(axis->speedAbs);
    axis->hw.tim->ARR = axis->stepIntervalUs;
    Stepper_UpdateDirPin(axis);
    Stepper_SetPulseEnabled(axis, true);
}

void Stepper_OnPulseISR(StepperAxis *axis)
{
    if (!Stepper_IsBusy(axis)) {
        Stepper_SetPulseEnabled(axis, false);
        return;
    }

    axis->position += axis->direction;
}
