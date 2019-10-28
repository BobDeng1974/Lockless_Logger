# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/common/ringBuffer/ringBuffer.c \
../src/common/ringBuffer/ringBufferList.c 

OBJS += \
./src/common/ringBuffer/ringBuffer.o \
./src/common/ringBuffer/ringBufferList.o 

C_DEPS += \
./src/common/ringBuffer/ringBuffer.d \
./src/common/ringBuffer/ringBufferList.d 


# Each subdirectory must supply rules for building sources it contributes
src/common/ringBuffer/%.o: ../src/common/ringBuffer/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	gcc -std=c11 -D_POSIX_C_SOURCE -O3 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


