################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/test/unit/node/nodeTest.c 

OBJS += \
./src/test/unit/node/nodeTest.o 

C_DEPS += \
./src/test/unit/node/nodeTest.d 


# Each subdirectory must supply rules for building sources it contributes
src/test/unit/node/%.o: ../src/test/unit/node/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -D_POSIX_C_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


