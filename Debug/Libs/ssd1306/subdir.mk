################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Libs/ssd1306/ssd1306.c \
../Libs/ssd1306/ssd1306_fonts.c \
../Libs/ssd1306/ssd1306_tests.c 

OBJS += \
./Libs/ssd1306/ssd1306.o \
./Libs/ssd1306/ssd1306_fonts.o \
./Libs/ssd1306/ssd1306_tests.o 

C_DEPS += \
./Libs/ssd1306/ssd1306.d \
./Libs/ssd1306/ssd1306_fonts.d \
./Libs/ssd1306/ssd1306_tests.d 


# Each subdirectory must supply rules for building sources it contributes
Libs/ssd1306/%.o Libs/ssd1306/%.su Libs/ssd1306/%.cyclo: ../Libs/ssd1306/%.c Libs/ssd1306/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I../Libs/ssd1306 -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Libs-2f-ssd1306

clean-Libs-2f-ssd1306:
	-$(RM) ./Libs/ssd1306/ssd1306.cyclo ./Libs/ssd1306/ssd1306.d ./Libs/ssd1306/ssd1306.o ./Libs/ssd1306/ssd1306.su ./Libs/ssd1306/ssd1306_fonts.cyclo ./Libs/ssd1306/ssd1306_fonts.d ./Libs/ssd1306/ssd1306_fonts.o ./Libs/ssd1306/ssd1306_fonts.su ./Libs/ssd1306/ssd1306_tests.cyclo ./Libs/ssd1306/ssd1306_tests.d ./Libs/ssd1306/ssd1306_tests.o ./Libs/ssd1306/ssd1306_tests.su

.PHONY: clean-Libs-2f-ssd1306

