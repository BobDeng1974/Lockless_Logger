################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/test/unit/linkedList/linkedListTest.c 

OBJS += \
./src/test/unit/linkedList/linkedListTest.o 

C_DEPS += \
./src/test/unit/linkedList/linkedListTest.d 


# Each subdirectory must supply rules for building sources it contributes
src/test/unit/linkedList/%.o: ../src/test/unit/linkedList/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	musl-gcc -D_POSIX_C_SOURCE -D_GNU_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


