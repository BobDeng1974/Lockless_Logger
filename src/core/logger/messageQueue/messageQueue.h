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
 * @file messageQueue.h
 * @author Barak Sason Rofman
 * @brief This module provides a MessageQueue implementation
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <stdio.h>
#include <sys/time.h>
#include <stdatomic.h>

enum MessageQueueStatusCodes {
	MQ_STATUS_FAILURE = -1, MQ_STATUS_SUCCESS
};

struct MessageQueue;

/**
 * Creates a new MessageQueue object
 * @param size desired MessageQueue size
 * @param maxArgsLen Maximum length of additional arguments to log message
 * @return
 */
struct MessageQueue* newMessageInfo(const int size, const int maxArgsLen);

/**
 * Adds a message from worker to queue
 * @param mq The MessageQueue to add the message to
 * @param loggingLevel Log level (one of the levels at 'logLevels')
 * @param file Filename to log
 * @param func Function name to log
 * @param line Line number to log
 * @param args Additional arguments to log message
 * @param msg The message
 * @param logMethod Logging method (private buffer, shared buffer or direct write)
 * @return MQ_STATUS_SUCCESS on success, MQ_STATUS_FAILURE on failure
 */
int addMessage(struct MessageQueue* mq, const int loggingLevel, char* file, const char* func,
               const int line, va_list* args, const char* msg, int logMethod);

/**
 * Drains all the message from a given queue
 * @param mq The MessageQueue to drain messages from
 * @param logFile The file to drain messages to
 * @param maxMsgLen Maximum length of a message
 * @param formatMethod A method to format the message
 */
void drainMessages(struct MessageQueue* mq, FILE* logFile, const int maxMsgLen,
                   const int (*formatMethod)());

/**
 * Directly write to a file
 * @param loggingLevel Log level (one of the levels at 'logLevels')
 * @param file Filename to log
 * @param func Function name to log
 * @param line Line number to log
 * @param args Additional arguments to log message
 * @param msg The message
 * @param logFile The file to write the messages to
 * @param maxMsgLen Maximum length of a message
 * @param maxArgsLen Maximum length of additional arguments to log message
 * @param logMethod Specifies the way logging is done
 * @param formatMethod A method to format the message
 */
void directWriteToFile(const int loggingLevel, char* file, const char* func, const int line,
                       va_list* args, char* msg, FILE* logFile, const int maxMsgLen,
                       const int maxArgsLen, const int logMethod, const int (*formatMethod)());

/**
 * Releases all resources associated with a given  MessageQueue
 * @param mq
 */
void messageDataQueueDestroy(struct MessageQueue* mq);

#endif /* MESSAGE_QUEUE_H */
