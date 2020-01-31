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
	gcc -std=c11 -D_POSIX_C_SOURCE -D_GNU_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


