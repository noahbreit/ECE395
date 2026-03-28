################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/TMC-API/tmc/ramp/LinearRamp.c \
../Drivers/TMC-API/tmc/ramp/LinearRamp1.c \
../Drivers/TMC-API/tmc/ramp/Ramp.c 

OBJS += \
./Drivers/TMC-API/tmc/ramp/LinearRamp.o \
./Drivers/TMC-API/tmc/ramp/LinearRamp1.o \
./Drivers/TMC-API/tmc/ramp/Ramp.o 

C_DEPS += \
./Drivers/TMC-API/tmc/ramp/LinearRamp.d \
./Drivers/TMC-API/tmc/ramp/LinearRamp1.d \
./Drivers/TMC-API/tmc/ramp/Ramp.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/TMC-API/tmc/ramp/%.o Drivers/TMC-API/tmc/ramp/%.su Drivers/TMC-API/tmc/ramp/%.cyclo: ../Drivers/TMC-API/tmc/ramp/%.c Drivers/TMC-API/tmc/ramp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../Drivers/ -I../../Core/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-ramp

clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-ramp:
	-$(RM) ./Drivers/TMC-API/tmc/ramp/LinearRamp.cyclo ./Drivers/TMC-API/tmc/ramp/LinearRamp.d ./Drivers/TMC-API/tmc/ramp/LinearRamp.o ./Drivers/TMC-API/tmc/ramp/LinearRamp.su ./Drivers/TMC-API/tmc/ramp/LinearRamp1.cyclo ./Drivers/TMC-API/tmc/ramp/LinearRamp1.d ./Drivers/TMC-API/tmc/ramp/LinearRamp1.o ./Drivers/TMC-API/tmc/ramp/LinearRamp1.su ./Drivers/TMC-API/tmc/ramp/Ramp.cyclo ./Drivers/TMC-API/tmc/ramp/Ramp.d ./Drivers/TMC-API/tmc/ramp/Ramp.o ./Drivers/TMC-API/tmc/ramp/Ramp.su

.PHONY: clean-Drivers-2f-TMC-2d-API-2f-tmc-2f-ramp

