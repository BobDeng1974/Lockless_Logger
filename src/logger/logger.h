/*
 ============================================================================
 Name        : logger.h
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#ifndef LOGGER
#define LOGGER

#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <stdbool.h>

enum loggerStatusCodes {
	STATUS_LOGGER_FAILURE = -1, STATUS_LOGGER_SUCCESS
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

typedef struct privateBufferData {
	bool isFree;
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	char* buf;
} privateBufferData;

typedef struct messageInfo {
	int line;
	int logLevel;
	int loggingMethod;
	char* file;
	const char* func;
	struct timeval tv;
	pthread_t tid;
} messageInfo;

/* Initialize all data required by the logger.
 * Note: This method must be called before any other api is used, and it can be
 * called only once
 * Parameters:
 * threadsNum - must be a non-negative value
 * privateBuffSize - must be a positive value
 * sharedBuffSize - must be a positive value
 * loggingLevel - must be one of the defined logging levels */
int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize,
               int loggingLevel);

/* Register a worker thread at the logger and assign a private buffers to it */
int registerThread();

/* Unregister a thread from the logger and free the private buffer for another thread's use */
void unregisterThread();

/* Add a message from a worker thread to a buffer or write it directly to file
 * if buffers are full.
 * Note: 'msg' must be a null-terminated string
 * Note: 'logMessage' should be called only by using the macro 'LOG_LEVEL_MSG' */
int logMessage(int loggingLevel, char* file, const int line, const char* func,
               const char* msg, ...);

/* Terminate the logger thread and release resources */
void terminateLogger();

/* Sets logging level to the specified value */
void setLoggingLevel(int loggingLevel);

#define LOG_MSG(loggingLevel, msg ...) logMessage(loggingLevel, __FILE__, __LINE__ ,__PRETTY_FUNCTION__ , msg)

#endif /* LOGGER */
