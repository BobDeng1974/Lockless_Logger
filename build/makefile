################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/writeMethods/subdir.mk
-include src/test/logger/subdir.mk
-include src/core/logger/messageQueue/subdir.mk
-include src/core/logger/subdir.mk
-include src/core/common/queue/subdir.mk
-include src/core/common/linkedList/node/subdir.mk
-include src/core/common/linkedList/subdir.mk
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
all: Logger

# Tool invocations
Logger: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: Cross GCC Linker'
	gcc -pthread -o "Logger" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) Logger
	-@echo ' '

.PHONY: all clean dependents

-include ../makefile.targets
