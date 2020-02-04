################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/common/queue/queue.c 

OBJS += \
./src/core/common/queue/queue.o 

C_DEPS += \
./src/core/common/queue/queue.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/common/queue/%.o: ../src/core/common/queue/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


