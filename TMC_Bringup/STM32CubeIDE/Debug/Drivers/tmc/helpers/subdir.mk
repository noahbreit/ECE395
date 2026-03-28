################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/tmc/helpers/CRC.c \
../Drivers/tmc/helpers/Functions.c 

OBJS += \
./Drivers/tmc/helpers/CRC.o \
./Drivers/tmc/helpers/Functions.o 

C_DEPS += \
./Drivers/tmc/helpers/CRC.d \
./Drivers/tmc/helpers/Functions.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/tmc/helpers/%.o Drivers/tmc/helpers/%.su Drivers/tmc/helpers/%.cyclo: ../Drivers/tmc/helpers/%.c Drivers/tmc/helpers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Drivers/ -I../../Core/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-tmc-2f-helpers

clean-Drivers-2f-tmc-2f-helpers:
	-$(RM) ./Drivers/tmc/helpers/CRC.cyclo ./Drivers/tmc/helpers/CRC.d ./Drivers/tmc/helpers/CRC.o ./Drivers/tmc/helpers/CRC.su ./Drivers/tmc/helpers/Functions.cyclo ./Drivers/tmc/helpers/Functions.d ./Drivers/tmc/helpers/Functions.o ./Drivers/tmc/helpers/Functions.su

.PHONY: clean-Drivers-2f-tmc-2f-helpers

