################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/writeMethods/writeMethods.c 

OBJS += \
./src/writeMethods/writeMethods.o 

C_DEPS += \
./src/writeMethods/writeMethods.d 


# Each subdirectory must supply rules for building sources it contributes
src/writeMethods/%.o: ../src/writeMethods/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


