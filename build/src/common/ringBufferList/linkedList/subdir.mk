################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/common/ringBufferList/linkedList/linkedList.c 

OBJS += \
./src/common/ringBufferList/linkedList/linkedList.o 

C_DEPS += \
./src/common/ringBufferList/linkedList/linkedList.d 


# Each subdirectory must supply rules for building sources it contributes
src/common/ringBufferList/linkedList/%.o: ../src/common/ringBufferList/linkedList/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -D_POSIX_C_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


