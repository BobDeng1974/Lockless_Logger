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
 * @file logger.c
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <threads.h>
#include <stdbool.h>
#include <syscall.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>
#include <string.h>

#include "../api/logger.h"
#include "../common/Queue/Queue.h"
#include "messageQueue/messageQueue.h"
#include "messageQueue/messageData.h"

enum logMethod {
	LM_PRIVATE_BUFFER, LM_SHARED_BUFFER, LM_DIRECT_WRITE
};

static int threadsNum;
static int maxMsgLen;
static int maxArgsLen;
static atomic_bool isTerminate;
static atomic_int logLevel;
static FILE* logFile;
static pthread_mutex_t loggerLock;
static pthread_mutex_t sharedBufferlock;
static pthread_mutex_t directWriteLock;
static pthread_t loggerThread;
static struct Queue* privateBuffersQueue;
static struct MessageQueue** privateBuffers;
static struct MessageQueue* sharedBuffer;
thread_local struct MessageQueue* tlmq; /* Thread Local Message Queue */

static bool isValitInitConditions(const int threadsNumArg,
                                  const int privateBuffSize,
                                  const int sharedBuffSize,
                                  const int loggingLevel);
static void setStaticValues(const int threadsNumArg, const int maxArgsLenArg,
                            const int maxMsgLenArg, const int loggingLevel);
static void initLocks();
static void initMessageQueues(const int privateBuffSize,
                              const int sharedBuffSize, const int maxArgsLenArg);
static void startLoggerThread();
static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(const int privateBuffSize, const int argsBufSize);
static void initPrivateBuffers(const int privateBuffSize, const int argsBufSize);
static int writeTosharedBuffer(const int loggingLevel, char* file,
                               const char* func, const int line, va_list* args,
                               const char* msg);
static inline void drainPrivateBuffers();
static inline void drainSharedBuffer();
static int formatMsg(char* buf, const MessageData* md);
static inline bool isLoggingConditionsValid(const int loggingLevel, char* msg);

/* API method - Description located at .h file */
int initLogger(const int threadsNumArg, const int privateBuffSize,
               const int sharedBuffSize, const int loggingLevel,
               const int maxMsgLenArg, const int maxArgsLenArg) {
	if (true
	        == isValitInitConditions(threadsNumArg, privateBuffSize,
	                                 sharedBuffSize, loggingLevel)) {
		setStaticValues(threadsNumArg, maxArgsLenArg, maxMsgLenArg,
		                loggingLevel);
		initLocks();
		initMessageQueues(privateBuffSize, sharedBuffSize, maxArgsLenArg);
		startLoggerThread();

		return LOG_STATUS_SUCCESS;
	}

	return LOG_STATUS_FAILURE;
}

/**
 *
 * @param threadsNumArg Numbers of threads (available buffers)
 * @param privateBuffSize Size of private buffers
 * @param sharedBuffSize Size of shared buffer
 * @param loggingLevel Logging level (one of the levels at 'logLevels')
 * @return True of conditions are valid of false otherwise
 */
static bool isValitInitConditions(const int threadsNumArg,
                                  const int privateBuffSize,
                                  const int sharedBuffSize,
                                  const int loggingLevel) {
	if ((0 >= threadsNumArg) || (2 > privateBuffSize) || (0 >= sharedBuffSize)
	        || (loggingLevel < LOG_LEVEL_NONE)
	        || (loggingLevel > LOG_LEVEL_TRACE)
	        || (LOG_STATUS_SUCCESS != createLogFile())) {
		return false;
	}

	return true;
}

/**
 * Set static parameters
 * @param threadsNumArg Numbers of threads (available buffers)
 * @param maxArgsLenArg Maximum message length
 * @param maxMsgLenArg Maximum message arguments length
 * @param loggingLevel Logging level (one of the levels at 'logLevels')
 */
static void setStaticValues(const int threadsNumArg, const int maxArgsLenArg,
                            const int maxMsgLenArg, const int loggingLevel) {
	threadsNum = threadsNumArg;
	maxMsgLen = maxMsgLenArg;
	maxArgsLen = maxArgsLenArg;
	setLoggingLevel(loggingLevel);
}

/**
 * Initialize static mutexes
 */
static void initLocks() {
	pthread_mutex_init(&loggerLock, NULL);
	pthread_mutex_init(&sharedBufferlock, NULL);
	pthread_mutex_init(&directWriteLock, NULL);
}

/**
 * Create message queues
 * @param privateBuffSize Size of private buffers
 * @param sharedBuffSize Size of shared buffer
 * @param maxArgsLenArg Maximum message arguments length
 */
static void initMessageQueues(const int privateBuffSize,
                              const int sharedBuffSize, const int maxArgsLenArg) {
	//TODO: add an option to dynamically change all of these:
	initPrivateBuffers(privateBuffSize, maxArgsLenArg);
	initsharedBuffer(sharedBuffSize, maxArgsLenArg);
}

/**
 * Starts the internal logger threads which drains buffers to file
 */
static void startLoggerThread() {
	pthread_create(&loggerThread, NULL, runLogger, NULL);
}

/* API method - Description located at .h file */
inline void setLoggingLevel(const int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/**
 * Initialize private buffers parameters
 * @param privateBuffSize Number of buffers
 * @param argsBufSize Size of additional message arguments
 */
static void initPrivateBuffers(const int privateBuffSize, const int argsBufSize) {
	int i;

	privateBuffersQueue = newQueue(threadsNum);
	//TODO: think if malloc failures need to be handled
	privateBuffers = malloc(threadsNum * sizeof(*privateBuffers));

	for (i = 0; i < threadsNum; ++i) {
		struct MessageQueue* mq;

		mq = newMessageInfo(privateBuffSize, argsBufSize);
		privateBuffers[i] = mq;
		enqueue(privateBuffersQueue, mq);
	}
}

/**
 * Initialize shared buffer data parameters
 * @param sharedBufferSize Size of buffer
 * @param argsBufSize Size of additional message arguments
 */
static void initsharedBuffer(const int sharedBuffSize, const int argsBufSize) {
	sharedBuffer = newMessageInfo(sharedBuffSize, argsBufSize);
}

/**
 * Creates a log file
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w+");

	return (NULL == logFile) ? LOG_STATUS_FAILURE : LOG_STATUS_SUCCESS;
}

/* API method - Description located at .h file */
int registerThread() {
	tlmq = dequeue(privateBuffersQueue);

	return (NULL != tlmq) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void unregisterThread() {
	if (NULL != tlmq) {
		enqueue(privateBuffersQueue, tlmq);
	}
}

/**
 * Logger thread loop - At each iteration, go over all the buffers and drain them to the log file,
 * flush buffer and sleep for a while
 */
static void* runLogger() {
	bool isTerminateLoc = false;
	do {
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainSharedBuffer();
		fflush(logFile); /* Flush buffer only at the end of the iteration */
//		sleep(1); /* Sleep to avoid wasting CPU */ //TODO: come up with a better mechanism
	} while (!isTerminateLoc);

	//TODO: Commented out code is for binary data check
//	unsigned char buf[8];
//	long decVal;
//	int i;
//
//	fclose(logFile);
//	logFile = fopen("logFile.txt", "r+");
//	fread(buf, 8, 1, logFile);
//
//	for (int i = 0; i < 8; i++)
//		printf("%u\n", buf[i]);
//
//	for (i = 0; i < 8; i++) {
//		decVal |= ((long) buf[i]) << (i * 8);
//	}

	return NULL;
}

/**
 * Iterate private buffers and drain them to file
 */
static inline void drainPrivateBuffers() {
	int i;

	for (i = 0; i < threadsNum; ++i) {
		drainMessages(privateBuffers[i], logFile, maxMsgLen, formatMsg);
	}
}

/**
 * Drain shared buffer to file
 */
static inline void drainSharedBuffer() {
	drainMessages(sharedBuffer, logFile, maxMsgLen, formatMsg);
}

/* API method - Description located at .h file */
void terminateLogger() {
	__atomic_store_n(&isTerminate, true, __ATOMIC_SEQ_CST);
	pthread_join(loggerThread, NULL);
	freeResources();
}

/**
 * Release all malloc'ed resources, destroy mutexs and close the open file
 */
static void freeResources() {
	int i;

	for (i = 0; i < threadsNum; ++i) {
		messageDataQueueDestroy(privateBuffers[i]);
	}
	free(privateBuffers);

	pthread_mutex_destroy(&sharedBufferlock);
	pthread_mutex_destroy(&loggerLock);
	pthread_mutex_destroy(&directWriteLock);

	queueDestroy(privateBuffersQueue);

	fclose(logFile);
}

/* API method - Description located at .h file */
int logMessage(const int loggingLevel, char* file, const char* func,
               const int line, char* msg, ...) {
	if (true == isLoggingConditionsValid(loggingLevel, msg)) {
		int writeToPrivateBuffer;
		va_list arg;

		/* Try each level of writing. If a level fails (buffer full), fall back to a
		 * lower & slower level.
		 * First, try private buffer writing. If private buffer doesn't exist
		 * (unregistered thread) or unable to write in this method, fall to
		 * next methods */
		va_start(arg, msg);
		if (NULL != tlmq) {
			writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file, func,
			                                  line, &arg, msg,
			                                  LM_PRIVATE_BUFFER);
		} else {
			/* The current thread doesn't have a private buffer - Try to register */
			if (LOG_STATUS_SUCCESS == registerThread()) {
				/* Managed to get a private buffer - write to it */
				writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file,
				                                  func, line, &arg, msg,
				                                  LM_PRIVATE_BUFFER);
			} else {
				/* Can't use private buffer */
				writeToPrivateBuffer = LOG_STATUS_FAILURE;
			}
		}

		if (LOG_STATUS_SUCCESS != writeToPrivateBuffer) {
			/* Recommended not to get here - Register all threads and/or increase
			 * private buffers size */
			if (MQ_STATUS_SUCCESS
			        != writeTosharedBuffer(loggingLevel, file, func, line, &arg,
			                               msg)) {
				/* Recommended not to get here - Increase private and shared buffers size */
				pthread_mutex_lock(&directWriteLock); /* Lock */
				{
					directWriteToFile(loggingLevel, file, func, line, &arg, msg,
					                  logFile, maxMsgLen, maxArgsLen,
					                  LM_DIRECT_WRITE, formatMsg);
					++cnt; //TODO: remove
				}
				pthread_mutex_unlock(&directWriteLock); /* Unlock */
			}
		}
		va_end(arg);
	}

	return LOG_STATUS_SUCCESS;
}

/**
 * Check whether or not the logging conditions of the current message are valid
 * @param loggingLevel The logging level of the current message
 * @param msg The message itself
 * @return True of conditions are valid of false otherwise
 */
static inline bool isLoggingConditionsValid(const int loggingLevel, char* msg) {
	bool isTerminateLoc;
	int loggingLevelLoc;

	/* Don't log if trying to log messages with higher level than requested
	 * of log level was set to LOG_LEVEL_NONE */
	__atomic_load(&logLevel, &loggingLevelLoc, __ATOMIC_SEQ_CST);
	if (LOG_LEVEL_NONE == loggingLevelLoc || loggingLevel > loggingLevelLoc) {
		return false;
	}

	__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
	/* Don't log if:
	 * 1) Logger is terminating
	 * 2) 'msg' has an invalid value */
	if (true == isTerminateLoc || NULL == msg) {
		return false;
	}

	return true;
}

/**
 * Adds a message to the shared buffer
 * @param loggingLevel Log level (one of the levels at 'logLevels')
 * @param file Filename to log
 * @param func Function name to log
 * @param line Line number to log
 * @param args Additional arguments to log message
 * @param msg The message
 * @return
 */
static int writeTosharedBuffer(const int loggingLevel, char* file,
                               const char* func, const int line, va_list* args,
                               const char* msg) {
	int ret;

	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		ret = addMessage(sharedBuffer, loggingLevel, file, func, line, args,
		                 msg, LM_SHARED_BUFFER);
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */

	return ret;
}

/**
 * Formats a message
 * @param buf Buffer to save the message
 * @param md MessageData struct containing message info
 * @return Formatted message length
 */
static int formatMsg(char* buf, const MessageData* md) {
	static char logLevelsIds[] = { ' ', /* NONE */
	                               'M', /* EMERGENCY */
	                               'A', /* ALERT */
	                               'C', /* CRITICAL */
	                               'E', /* ERROR */
	                               'W', /* WARNING */
	                               'N', /* NOTICE */
	                               'I', /* INFO */
	                               'D', /* DEBUG */
	                               'T', /* TRACE */};

	static char* logMethods[] = { "pb", "sb", "dw" };

	return snprintf(
	        buf,
	        maxMsgLen,
	        "[mid: %x:%.5x] [ll: %c] [lm: %s] [lwp: %.5ld] [loc: %s:%s:%d] [msg: %s]\n",
	        (unsigned int) md->tv.tv_sec, (unsigned int) md->tv.tv_usec,
	        logLevelsIds[md->logLevel], logMethods[md->logMethod], md->tid,
	        md->file, md->func, md->line, md->argsBuf);

//TODO: Add an option to choose writing method (text, binary, etc')
//	int fileNameLen = strlen(md->file);
//	int methodNameLen = strlen(md->func);
//	int argsBufLen = strlen(md->argsBuf);
//
//	fwrite(&md->tv.tv_sec, sizeof(md->tv.tv_sec), 1, logFile);
//	fwrite(&md->tv.tv_usec, sizeof(md->tv.tv_usec), 1, logFile);
//	fwrite(&logLevelsIds[md->logLevel], 1, 1, logFile);
//	fwrite(logMethods[md->logMethod], 2, 1, logFile);
//	fwrite(&md->tid, sizeof(md->tid), 1, logFile);
//	fwrite(&fileNameLen, sizeof(fileNameLen), 1, logFile);
//	fwrite(md->file, 1, fileNameLen, logFile);
//	fwrite(&methodNameLen, sizeof(methodNameLen), 1, logFile);
//	fwrite(md->func, 1, methodNameLen, logFile);
//	fwrite(&md->line, 1, sizeof(md->line), logFile);
//	fwrite(&argsBufLen, sizeof(argsBufLen), 1, logFile);
//	fwrite(md->argsBuf, 1, argsBufLen, logFile);
//	return 0;
}
