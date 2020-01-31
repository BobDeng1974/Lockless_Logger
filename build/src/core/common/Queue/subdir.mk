################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/common/Queue/Queue.c 

OBJS += \
./src/core/common/Queue/Queue.o 

C_DEPS += \
./src/core/common/Queue/Queue.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/common/Queue/%.o: ../src/core/common/Queue/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -D_GNU_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


