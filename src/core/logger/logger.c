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
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <threads.h>
#include <stdbool.h>
#include <syscall.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/time.h>

#include "../api/logger.h"
#include "../common/ringBufferList/linkedList/linkedList.h"
#include "../common/ringBufferList/ringBuffer/ringBuffer.h"
#include "../common/ringBufferList/ringBufferList.h"

enum logSource {
	LS_PRIVATE_BUFFER, LS_SHARED_BUFFER, LS_DIRECT_WRITE
};

typedef struct MessageInfo {
	/** Line number to log */
	int line;
	/** Log level (one of the levels at 'logLevels') */
	int logLevel;
	/** Logging method (private buffer, shared buffer or direct write) */
	int loggingMethod;
	/** Log file */
	char* file;
	/** Additional arguments to log message */
	char* argsBuf;
	/** Function name to log */
	const char* func;
	/** Time information */
	struct timeval tv;
} MessageInfo;

/* Defines maximum allowed message length.
 * This value is primarily used to prevent data overwrite in the ring buffer,
 * as the true length of the message is not known until the message is
 * fully constructed */
#define MAX_MSG_LEN 512

/* Defines the length of the additional arguments that might be supplied
 * with the message */
#define ARGS_LEN 256

static atomic_bool isTerminate;
static atomic_int logLevel;
static int fileHandle;
static FILE* logFile;
static pthread_mutex_t loggerLock;
static pthread_t loggerThread;
static struct LinkedList* privateBuffers; // A list of all private buffers in the system
static struct LinkedList* availablePrivateBuffers; ///A list of available private buffers for threads to register to
static struct LinkedList* inUsePrivateBuffers; // A list of private buffers currently in use by threads
static struct RingBuffer* sharedBuffer;
static pthread_mutex_t sharedBufferlock;

thread_local struct LinkedListNode* rbn; // Ring Buffer Node

static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(const int sharedBufferSize);
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize);
static int writeToPrivateBuffer(struct RingBuffer* rb, MessageInfo* msgInfo);
static int writeTosharedBuffer(MessageInfo* msgInfo);
static void drainPrivateBuffers();
static void drainsharedBuffer();
static void directWriteToFile(MessageInfo* msgInfo);
static int writeInFormat(char* buf, const void* data, const int maxMessageLen);

/* Inlining light methods */
inline static void setMsgInfoValues(MessageInfo* msgInfo, char* file, const char* func,
                                    const int line, const int loggingLevel, char* argsBuf);
inline static void discardFilenamePrefix(char** file);

/* API method - Description located at .h file */
int initLogger(const int threadsNum, const int privateBuffSize, const int sharedBuffSize,
               const int loggingLevel) {
	if (0 > threadsNum || 0 >= privateBuffSize || 0 >= sharedBuffSize
	        || (loggingLevel < LOG_LEVEL_NONE || loggingLevel > LOG_LEVEL_TRACE)) {
		return LOG_STATUS_FAILURE;
	}

	/* Init all logger parameters */
	//TODO: add an option to dynamically change all of these:
	initPrivateBuffers(threadsNum, privateBuffSize);
	initsharedBuffer(sharedBuffSize);
	setLoggingLevel(loggingLevel);

	if (LOG_STATUS_SUCCESS != createLogFile()) {
		return LOG_STATUS_FAILURE;
	}

	/* Init mutexes */
	pthread_mutex_init(&loggerLock, NULL);
	pthread_mutex_init(&sharedBufferlock, NULL);

	/* Run logger thread */
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return LOG_STATUS_SUCCESS;
}

/* API method - Description located at .h file */
inline void setLoggingLevel(const int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/**
 * Initialize private buffers parameters
 * @param threadsNum NUmber of buffers
 * @param privateBuffSize Size of buffers
 */
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize) {
	int i;

	privateBuffers = newRingBufferList();
	availablePrivateBuffers = newRingBufferList();
	inUsePrivateBuffers = newRingBufferList();

	for (i = 0; i < threadsNum; ++i) {
		struct RingBuffer* rb;
		struct LinkedListNode* node;

		rb = newRingBuffer(privateBuffSize, MAX_MSG_LEN);

		node = newLinkedListNode(rb);
		addNode(privateBuffers, node); // This list will hold pointers to *all* allocated buffers.
		                               // This is required in order to keep track of all allocated
		                               // buffer, so they may be freed even in not all threads unregistered

		node = newLinkedListNode(rb);
		addNode(availablePrivateBuffers, node); // Filling this list up so threads may take buffers from it
	}
}

/**
 * Initialize shared buffer data parameters
 * @param sharedBufferSize Size of buffer
 */
static void initsharedBuffer(const int sharedBufferSize) {
	sharedBuffer = newRingBuffer(sharedBufferSize, MAX_MSG_LEN);
}

/**
 * Creates a log file
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");

	if (NULL == logFile) {
		return LOG_STATUS_FAILURE;
	}

	fileHandle = fileno(logFile);

	return (-1 != fileHandle) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
int registerThread() {
	if (NULL != getHead(availablePrivateBuffers)) { // check if it's worth fighting
	                                                // for the mutex
		pthread_mutex_lock(&loggerLock); /* Lock */
		{
			/* Take a node from the 'available buffers' pool */
			rbn = removeHead(availablePrivateBuffers);

			if (NULL != rbn) {
				/* Add this node to the 'inUse' pool without allocating additional space */
				addNode(inUsePrivateBuffers, rbn);
			}
		}
		pthread_mutex_unlock(&loggerLock); /* Unlock */
	}

	return (NULL != rbn) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void unregisterThread() {
	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		struct LinkedListNode* node;

		/* Find the note which contains 'rb' and remove it from the 'inUse' pool */
		node = removeNode(inUsePrivateBuffers, rbn);
		if (NULL != node) {
			/* Return this node to the 'available buffers' pool */
			addNode(availablePrivateBuffers, node);
			rbn = NULL;
		}
	}
	pthread_mutex_unlock(&loggerLock); /* Unlock */
}

/* Logger thread loop */
/**
 * Logger thread loop -At each iteration, go over all the buffers and drain them to the log file
 */
static void* runLogger() {
	bool isTerminateLoc = false;

	do {
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainsharedBuffer();
		/* At each iteration flush buffers to avoid data loss */
		fflush(logFile);
	} while (!isTerminateLoc);

	return NULL;
}

/**
 * Drain all private buffers assigned to worker threads to the log file
 */
static void drainPrivateBuffers() {
	struct LinkedListNode* node;

	node = getHead(privateBuffers); // Access is lockless, because if this list changes, private
	                                // buffers will be temporarily blocked

	while (NULL != node) {
		struct RingBuffer* rb;

		rb = getRingBuffer(node);
		drainBufferToFile(rb, fileHandle);
		node = getNext(node);
	}
}

/**
 * Drain the buffer which is shared by the worker threads to the log file
 */
static void drainsharedBuffer() {
	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		drainBufferToFile(sharedBuffer, fileHandle);
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */
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
	/* Freeing 'privateBuffers' using the 'freeRingBufferList' API takes care of freeing all allocated
	 * nodes *and ring buffers* so for 'availablePrivateBuffers' there is no need to free ring buffers
	 * as they are the same object, and for 'inUsePrivateBuffers'there is no need to free anything
	 * except the list object itself */
	deepDeleteRingBufferList(privateBuffers);
	shallowDeleteRingBufferList(availablePrivateBuffers);
	free(inUsePrivateBuffers);

	deleteRingBuffer(sharedBuffer);

	pthread_mutex_destroy(&sharedBufferlock);
	pthread_mutex_destroy(&loggerLock);

	fclose(logFile);
}

/* API method - Description located at .h file */
int logMessage(const int loggingLevel, char* file, const char* func, const int line,
               const char* msg, ...) {
	bool isTerminateLoc;
	int writeToPrivateBufferRes;
	int loggingLevelLoc;
	MessageInfo msgInfo;
	va_list arg;
	char argsBuf[ARGS_LEN];

	/* Don't log if trying to log messages with higher level than requested
	 * of log level was set to LOG_LEVEL_NONE */
	__atomic_load(&logLevel, &loggingLevelLoc, __ATOMIC_SEQ_CST);
	if (LOG_LEVEL_NONE == loggingLevelLoc || loggingLevel > loggingLevelLoc) {
		return LOG_STATUS_FAILURE;
	}

	__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
	/* Don't log if:
	 * 1) Logger is terminating
	 * 2) 'msg' has an invalid value */
	if (true == isTerminateLoc || NULL == msg) {
		return LOG_STATUS_FAILURE;
	}

	/* Get message arguments values */
	va_start(arg, msg);
	vsprintf(argsBuf, msg, arg);
	va_end(arg);

	/* Prepare all logging information */
	discardFilenamePrefix(&file);
	setMsgInfoValues(&msgInfo, file, func, line, loggingLevel, argsBuf);

	/* Try each level of writing. If a level fails (buffer full), fall back to a
	 * lower & slower level.
	 * First, try private buffer writing. If private buffer doesn't exist
	 * (unregistered thread) or unable to write in this method, fall to
	 * next methods */
	//TODO: still think if write from unregistered worker threads should be allowed
	if (NULL != rbn) {
		writeToPrivateBufferRes = writeToPrivateBuffer(getRingBuffer(rbn), &msgInfo);
	} else {
		/* The current thread doesn't have a private buffer - Try to register,
		 * maybe there's a free spot */
		if (LOG_STATUS_SUCCESS == registerThread()) {
			writeToPrivateBufferRes = writeToPrivateBuffer(getRingBuffer(rbn), &msgInfo);
		} else {
			writeToPrivateBufferRes = LOG_STATUS_FAILURE;
		}
	}

	if (LOG_STATUS_SUCCESS != writeToPrivateBufferRes) {
		/* Recommended not to get here - Register all threads and/or increase
		 * private buffers size */
		if (LOG_STATUS_SUCCESS != writeTosharedBuffer(&msgInfo)) {
			/* Recommended not to get here - Increase private and shared buffers size */
			directWriteToFile(&msgInfo);
			++cnt; //TODO: remove, for debug only (also - not accurate as not synchronized)
		}
	}

	return LOG_STATUS_SUCCESS;
}

/**
 * Get only the filename out of the full path
 * @param file The full file name
 */
inline static void discardFilenamePrefix(char** file) {
	char* lastSlash;

	lastSlash = strrchr(*file, '/');
	if (NULL != lastSlash) {
		*file = lastSlash + 1;
	}
}

/**
 * Populate messageInfo struct
 * @param msgInfo The struct to be populated
 * @param file File name that originated the call
 * @param func Method that originated the call
 * @param line Line that originated the call
 * @param loggingLevel Logging level of the message (must be one of the levels at 'logLevels')
 * @param argsBuf Additional arguments to log message
 */
inline static void setMsgInfoValues(MessageInfo* msgInfo, char* file, const char* func,
                                    const int line, const int loggingLevel, char* argsBuf) {
	gettimeofday(&msgInfo->tv, NULL);
	msgInfo->file = file;
	msgInfo->func = func;
	msgInfo->line = line;
	msgInfo->logLevel = loggingLevel;
	msgInfo->argsBuf = argsBuf;
}

/**
 * Add a message from a worker thread to it's private buffer
 * @param rb The RingBuffer to write to
 * @param msgInfo The message information
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
static int writeToPrivateBuffer(struct RingBuffer* rb, MessageInfo* msgInfo) {
	int res;

	msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
	res = writeToRingBuffer(rb, msgInfo, writeInFormat);

	return (RB_STATUS_SUCCESS == res) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/**
 * Add a message from a worker thread to the shared buffer
 * @param msgInfo The message information
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
static int writeTosharedBuffer(MessageInfo* msgInfo) {
	int res;

	msgInfo->loggingMethod = LS_SHARED_BUFFER;

	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		res = writeToRingBuffer(sharedBuffer, msgInfo, writeInFormat);
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */

	return (RB_STATUS_SUCCESS == res) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/**
 * Worker thread directly writes to the log file
 * @param msgInfo The message information
 */
static void directWriteToFile(MessageInfo* msgInfo) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];

	msgInfo->loggingMethod = LS_DIRECT_WRITE;
	msgLen = writeInFormat(locBuf, msgInfo, MAX_MSG_LEN);

	fwrite(locBuf, 1, msgLen, logFile);
}

/**
 * Log a message in a structured format:
 * mid - Message identifier. A time stamp in the format of
 * 		 (seconds):(microseconds). A timestamp in yyyy-mm-dd can later be parsed
 * 		 using this information
 * ll  - Logging level
 * lm  - Logging method (private buffer, shared buffer, direct write)
 * lwp - Thread identifier (LWP)
 * loc - Location in the format of (file):(line):(method)
 * msg - The message provided for the current log line
 * @param buf The buffer in which to write to
 * @param data Data to write
 * @param maxMessageLen The maximum message length that can be written to the buffer
 * @return
 */
static int writeInFormat(char* buf, const void* data, const int maxMessageLen) {
	int msgLen;
	MessageInfo* msgInfo;

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

	msgInfo = (MessageInfo*) data;

	msgLen = snprintf(buf, maxMessageLen,
	                  "[mid: %x:%.5x] [ll: %c] [lm: %s] [lwp: %ld] [loc: %s:%s:%d] [msg: %s]\n",
	                  (unsigned int) msgInfo->tv.tv_sec, (unsigned int) msgInfo->tv.tv_usec,
	                  logLevelsIds[msgInfo->logLevel], logMethods[msgInfo->loggingMethod],
	                  syscall(SYS_gettid), msgInfo->file, msgInfo->func, msgInfo->line,
	                  msgInfo->argsBuf);

	return msgLen;
}
