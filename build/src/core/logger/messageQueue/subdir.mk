################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/core/logger/messageQueue/messageData.c \
../src/core/logger/messageQueue/messageQueue.c 

OBJS += \
./src/core/logger/messageQueue/messageData.o \
./src/core/logger/messageQueue/messageQueue.o 

C_DEPS += \
./src/core/logger/messageQueue/messageData.d \
./src/core/logger/messageQueue/messageQueue.d 


# Each subdirectory must supply rules for building sources it contributes
src/core/logger/messageQueue/%.o: ../src/core/logger/messageQueue/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


