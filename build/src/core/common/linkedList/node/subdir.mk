################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/common/linkedList/node/linkedListNode.c 

OBJS += \
./src/core/common/linkedList/node/linkedListNode.o 

C_DEPS += \
./src/core/common/linkedList/node/linkedListNode.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/common/linkedList/node/%.o: ../src/core/common/linkedList/node/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


