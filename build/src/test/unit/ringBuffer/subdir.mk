################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/test/unit/ringBuffer/ringBufferTest.c 

OBJS += \
./src/test/unit/ringBuffer/ringBufferTest.o 

C_DEPS += \
./src/test/unit/ringBuffer/ringBufferTest.d 


# Each subdirectory must supply rules for building sources it contributes
src/test/unit/ringBuffer/%.o: ../src/test/unit/ringBuffer/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -D_POSIX_C_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


