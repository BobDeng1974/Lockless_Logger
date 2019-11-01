################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/test/logger/subdir.mk
-include src/core/logger/subdir.mk
-include src/core/common/ringBufferList/ringBuffer/subdir.mk
-include src/core/common/ringBufferList/linkedList/node/subdir.mk
-include src/core/common/ringBufferList/linkedList/subdir.mk
-include src/core/common/ringBufferList/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: Loggrer

# Tool invocations
Loggrer: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	gcc -pthread -o "Loggrer" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) Loggrer
	-@echo ' '

.PHONY: all clean dependents

-include ../makefile.targets
