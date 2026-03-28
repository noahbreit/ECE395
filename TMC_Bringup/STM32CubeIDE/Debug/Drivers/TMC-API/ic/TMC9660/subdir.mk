################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/TMC-API/ic/TMC9660/TMC9660.c 

OBJS += \
./Drivers/TMC-API/ic/TMC9660/TMC9660.o 

C_DEPS += \
./Drivers/TMC-API/ic/TMC9660/TMC9660.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/TMC-API/ic/TMC9660/%.o Drivers/TMC-API/ic/TMC9660/%.su Drivers/TMC-API/ic/TMC9660/%.cyclo: ../Drivers/TMC-API/ic/TMC9660/%.c Drivers/TMC-API/ic/TMC9660/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32L476xx -c -I../../Drivers/TMC-API/helpers -I../../Drivers/TMC-API/ic/TMC9660 -I../../Core/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc -I../../Drivers/STM32L4xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32L4xx/Include -I../../Drivers/CMSIS/Include -I../../Drivers/TMC-API/ramp -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-TMC-2d-API-2f-ic-2f-TMC9660

clean-Drivers-2f-TMC-2d-API-2f-ic-2f-TMC9660:
	-$(RM) ./Drivers/TMC-API/ic/TMC9660/TMC9660.cyclo ./Drivers/TMC-API/ic/TMC9660/TMC9660.d ./Drivers/TMC-API/ic/TMC9660/TMC9660.o ./Drivers/TMC-API/ic/TMC9660/TMC9660.su

.PHONY: clean-Drivers-2f-TMC-2d-API-2f-ic-2f-TMC9660

