/****************************************************************************
 * Copyright (C) [2019] [Barak Sason Rofman]								*
 *																			*
 * Licensed under the Apache License, Version 2.0 (the "License");			*
 * you may not use this file except in compliance with the License.			*
 * You may obtain a copy of the License at:									*
 *																			*
 * http://www.apache.org/licenses/LICENSE-2.0								*
 *																			*
 * Unless required by applicable law or agreed to in writing, software		*
 * distributed under the License is distributed on an "AS IS" BASIS,		*
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.	*
 * See the License for the specific language governing permissions and		*
 * limitations under the License.											*
 ****************************************************************************/

/**
 * @file messageData.c
 * @author Barak Sason Rofman
 * @brief This module provides a message struct and related message-specific operations
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <syscall.h>

#include "messageData.h"

/**
 * Saves message information
 * @param md MessageData struct to save info in
 * @param loggingLevel Log level (one of the levels at 'logLevels')
 * @param file Filename to log
 * @param func Function name to log
 * @param line Line number to log
 * @param args Additional arguments to log message
 * @param msg The message
 * @param logMethod Logging method (private buffer, shared buffer or direct write)
 * @param maxArgsLen Maximum length of additional message arguments
 */
void setMsgValues(struct MessageData* md, const int loggingLevel, char* file, const char* func,
                         const int line, va_list* args, const char* msg, const int logMethod,
                         const int maxArgsLen) {
	gettimeofday(&md->tv, NULL);
	md->file = file;
	md->func = func;
	md->line = line;
	md->logLevel = loggingLevel;
	md->logMethod = logMethod;
	md->tid = syscall(SYS_gettid);
	vsnprintf(md->argsBuf, maxArgsLen, msg, *args);
}
