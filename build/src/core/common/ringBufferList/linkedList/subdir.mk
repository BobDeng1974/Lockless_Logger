################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/common/ringBufferList/linkedList/linkedList.c 

OBJS += \
./src/core/common/ringBufferList/linkedList/linkedList.o 

C_DEPS += \
./src/core/common/ringBufferList/linkedList/linkedList.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/common/ringBufferList/linkedList/%.o: ../src/core/common/ringBufferList/linkedList/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	musl-gcc -std=c11 -D_POSIX_C_SOURCE -D_GNU_SOURCE -O0 -g3 -Wall -c -fmessage-length=0  -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


