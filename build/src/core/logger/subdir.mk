################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/logger/logger.c 

OBJS += \
./src/core/logger/logger.o 

C_DEPS += \
./src/core/logger/logger.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/logger/%.o: ../src/core/logger/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


