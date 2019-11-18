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
 * @file logger.h
 * @author Barak Sason Rofman
 * @brief This module provides an implementation of a logger utility which works on 3 levels:
 * Level 1 - Lockless writing:
 * 			Lockless writing is achieved by assigning each thread a private ring buffer.
 * 			A worker threads write to that buffer and the logger thread drains that buffer into
 * 			a log file.
 * Level 2 - Shared buffer writing:
 * 			In case the private ring buffer is full and not yet drained, a worker thread will
 * 			fall down to writing to a shared buffer (which is shared across all workers).
 * 			This is done in a synchronized manner.
 * Level 3 - In case the shared buffer is also full and not yet
 * 			drained, a worker thread will fall to the lowest (and slowest) form of writing - direct
 * 			file write.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef LOGGER_H
#define LOGGER_H

enum loggerStatusCodes {
	LOG_STATUS_FAILURE = -1, LOG_STATUS_SUCCESS
};

enum logLevels {
	LOG_LEVEL_NONE, /* No logging */
	LOG_LEVEL_EMERG, LOG_LEVEL_ALERT, LOG_LEVEL_CRITICAL, /* Fatal failure */
	LOG_LEVEL_ERROR, /* Non-fatal failure */
	LOG_LEVEL_WARNING, /* Attention required for normal operations */
	LOG_LEVEL_NOTICE, LOG_LEVEL_INFO, /* Normal information */
	LOG_LEVEL_DEBUG, /* Internal errors */
	LOG_LEVEL_TRACE, /* Code-flow tracing */
};

//TODO: remove, for debug only
long long cnt;

/**
 * Initialize all data required by the logger.
 * Note: This method must be called before any other API is used, and it can be
 * called only once
 * @param threadsNum Maximum number of threads that will be able to register
 * @param privateBuffSize Size of private buffers
 * @param sharedBuffSize Size of shared buffer
 * @param loggingLevel Desired logging level (only messages with lower or equal logging level will
 * be logged, one of the levels at 'logLevels')
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
int initLogger(const int threadsNum, const int privateBuffSize, const int sharedBuffSize,
               const int loggingLevel);

/**
 * Register a worker thread at the logger and assign a private buffers to it
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
int registerThread();

/**
 * Unregister a thread from the logger and free the private buffer for another thread's use
 * NOTE: Each registered thread should unregister before logger termination
 * NOTE: This API may be called only after calling 'initLogger(...) API
 */
void unregisterThread();

/**
 * Add a message from a worker thread to a private buffer (or write it directly to file if a
 * private buffer is unavailable)
 * NOTE: 'logMessage' should be called only by using the macro 'LOG_MSG'
 * NOTE: this API may be called only after calling 'initLogger(...) API
 * @param loggingLevel Logging level of the message (must be one of the levels at 'logLevels')
 * @param file File name that originated the call
 * @param func Method that originated the call
 * @param line Line that originated the call
 * @param msg Message data (must be a null-terminated string)
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
int logMessage(const int loggingLevel, char* file, const char* func, const int line,
               const char* msg, ...);

/** A macro that defines the usage for 'logMessage(...) API */
#define LOG_MSG(loggingLevel, msg ...) logMessage(loggingLevel, __FILE__,__PRETTY_FUNCTION__,  __LINE__  , msg)

/**
 * Terminate the logger thread and release resources
 * NOTE: this API may be called only after calling 'initLogger(...) API
 */
void terminateLogger();

/**
 * Sets logging level to the specified value
 * NOTE: this API may be called only after calling 'initLogger(...) API
 * @param loggingLevel New logging level of the message (must be one of the defined logging levels)
 */
void setLoggingLevel(const int loggingLevel);

#endif /* LOGGER_H */
