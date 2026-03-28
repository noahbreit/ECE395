################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/TMC-API/tmc/helpers/CRC.c \
../Drivers/TMC-API/tmc/helpers/Functions.c 

OBJS += \
./Drivers/TMC-API/tmc/helpers/CRC.o \
./Drivers/TMC-API/tmc/helpers/Functions.o 

C_DEPS += \
./Drivers/TMC-API/tmc/helpers/CRC.d \
./Drivers/TMC-API/tmc/helpers/Functions.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/TMC-API/tmc/helpers/%.o Drivers/TMC-API/tmc/helpers/%.su Drivers/TMC-API/tmc/helpers/%.cyclo: ../Drivers/TMC-API/tmc/helpers/%.c Drivers/TMC-API/tmc/helpers/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Drivers/ -I../../Core/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-helpers

clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-helpers:
	-$(RM) ./Drivers/TMC-API/tmc/helpers/CRC.cyclo ./Drivers/TMC-API/tmc/helpers/CRC.d ./Drivers/TMC-API/tmc/helpers/CRC.o ./Drivers/TMC-API/tmc/helpers/CRC.su ./Drivers/TMC-API/tmc/helpers/Functions.cyclo ./Drivers/TMC-API/tmc/helpers/Functions.d ./Drivers/TMC-API/tmc/helpers/Functions.o ./Drivers/TMC-API/tmc/helpers/Functions.su

.PHONY: clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-helpers

