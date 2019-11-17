/*
 ============================================================================
 Name        : logger.h
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides an implementation of a logger utility
 which works on 3 levels:
 Level 1 - Lockless writing:
 Lockless writing is achieved by assigning each thread a
 private ring buffer. A worker threads write to that buffer
 and the logger thread drains that buffer into a log file.
 Level 2 - Shared buffer writing:
 In case the private ring buffer is full and not yet drained,
 a worker thread will fall down to writing to a shared buffer
 (which is shared across all workers). This is done in a
 synchronized manner.
 Level 3 - In case the shared buffer is also full and not yet
 drained, a worker thread will fall to the lowest (and slowest)
 form of writing - direct file write.
 ============================================================================
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

/* Initialize all data required by the logger.
 * Note: This method must be called before any other api is used, and it can be
 * called only once
 * Parameters:
 * 'threadsNum' - must be a non-negative value
 * 'privateBuffSize' - must be a positive value
 * 'sharedBuffSize' - must be a positive value
 * 'loggingLevel' - must be one of the defined logging levels
 * NOTE: this API has to be called before any other API */
int initLogger(const int threadsNum, const int privateBuffSize,
               const int sharedBuffSize, const int loggingLevel);

/* Register a worker thread at the logger and assign a private buffers to it */
int registerThread();

/* Unregister a thread from the logger and free the private buffer for another
 * thread's use
 * NOTE: each registered thread must unregister before termination
 * NOTE: this API may be called only after calling 'initLogger(...) API*/
void unregisterThread();

/* See description for 'LOG_MSG'
 * Note: 'logMessage' should be called only by using the macro 'LOG_LEVEL_MSG'
 * NOTE: this API may be called only after calling 'initLogger(...) API */
int logMessage(const int loggingLevel, char* file, const char* func,
               const int line, const char* msg, ...);

/* Add a message from a worker thread to a buffer or write it directly to file
 * if buffers are full.
 * 'loggingLevel' - must be a level specified in 'logLevels'
 * 'msg' - must be a null-terminated string
 * NOTE: this API may be called only after calling 'initLogger(...) API*/
#define LOG_MSG(loggingLevel, msg ...) logMessage(loggingLevel, __FILE__,__PRETTY_FUNCTION__,  __LINE__  , msg)

/* Terminate the logger thread and release resources
 * NOTE: this API may be called only after calling 'initLogger(...) API */
void terminateLogger();

/* Sets logging level to the specified value
 * NOTE: this API may be called only after calling 'initLogger(...) API*/
void setLoggingLevel(const int loggingLevel);

#endif /* LOGGER_H */
