################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/tmc/ic/TMC9660/TMC9660.c 

OBJS += \
./Drivers/tmc/ic/TMC9660/TMC9660.o 

C_DEPS += \
./Drivers/tmc/ic/TMC9660/TMC9660.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/tmc/ic/TMC9660/%.o Drivers/tmc/ic/TMC9660/%.su Drivers/tmc/ic/TMC9660/%.cyclo: ../Drivers/tmc/ic/TMC9660/%.c Drivers/tmc/ic/TMC9660/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L412xx -c -I../Core/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc -I../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../Drivers/CMSIS/Include -I../USB_DEVICE/App -I../USB_DEVICE/Target -I../Middlewares/ST/STM32_USB_Device_Library/Core/Inc -I../Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc -I../Drivers/ -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-tmc-2f-ic-2f-TMC9660

clean-Drivers-2f-tmc-2f-ic-2f-TMC9660:
	-$(RM) ./Drivers/tmc/ic/TMC9660/TMC9660.cyclo ./Drivers/tmc/ic/TMC9660/TMC9660.d ./Drivers/tmc/ic/TMC9660/TMC9660.o ./Drivers/tmc/ic/TMC9660/TMC9660.su

.PHONY: clean-Drivers-2f-tmc-2f-ic-2f-TMC9660

