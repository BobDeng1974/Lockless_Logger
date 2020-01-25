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
 * @file messageData.h
 * @author Barak Sason Rofman
 * @brief This module provides a message struct and related message-specific operations
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef MESSAGEDATA_H
#define MESSAGEDATA_H

#include <stdio.h>
#include <sys/time.h>

typedef struct MessageData {
	/** Line number to log */
	int line;
	/** Log level (one of the levels at 'logLevels') */
	int logLevel;
	/** Logging method (private buffer, shared buffer or direct write) */
	int logMethod;
	/** Filename to log */
	char* file;
	/** Additional arguments to log message */
	char* argsBuf;
	/** Function name to log */
	const char* func;
	/** Thread id */
	long tid;
	/** Time information */
	struct timeval tv;
} MessageData;

/* API method - Description located at .h file */
void setMsgValues(struct MessageData* md, const int loggingLevel, char* file, const char* func,
                  const int line, va_list* args, const char* msg, const int logMethod,
                  const int maxArgsLen);

#endif /* MESSAGEDATA_H */
