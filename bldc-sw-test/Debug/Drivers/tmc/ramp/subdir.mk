################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/tmc/ramp/LinearRamp.c \
../Drivers/tmc/ramp/LinearRamp1.c \
../Drivers/tmc/ramp/Ramp.c 

OBJS += \
./Drivers/tmc/ramp/LinearRamp.o \
./Drivers/tmc/ramp/LinearRamp1.o \
./Drivers/tmc/ramp/Ramp.o 

C_DEPS += \
./Drivers/tmc/ramp/LinearRamp.d \
./Drivers/tmc/ramp/LinearRamp1.d \
./Drivers/tmc/ramp/Ramp.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/tmc/ramp/%.o Drivers/tmc/ramp/%.su Drivers/tmc/ramp/%.cyclo: ../Drivers/tmc/ramp/%.c Drivers/tmc/ramp/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L412xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/ -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-tmc-2f-ramp

clean-Drivers-2f-tmc-2f-ramp:
	-$(RM) ./Drivers/tmc/ramp/LinearRamp.cyclo ./Drivers/tmc/ramp/LinearRamp.d ./Drivers/tmc/ramp/LinearRamp.o ./Drivers/tmc/ramp/LinearRamp.su ./Drivers/tmc/ramp/LinearRamp1.cyclo ./Drivers/tmc/ramp/LinearRamp1.d ./Drivers/tmc/ramp/LinearRamp1.o ./Drivers/tmc/ramp/LinearRamp1.su ./Drivers/tmc/ramp/Ramp.cyclo ./Drivers/tmc/ramp/Ramp.d ./Drivers/tmc/ramp/Ramp.o ./Drivers/tmc/ramp/Ramp.su

.PHONY: clean-Drivers-2f-tmc-2f-ramp

