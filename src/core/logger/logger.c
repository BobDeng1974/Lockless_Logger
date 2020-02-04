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

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <string.h>

#include "../api/logger.h"
#include "messageQueue/messageQueue.h"
#include "messageQueue/messageData.h"
#include "../common/linkedList/linkedList.h"
#include "../common/queue/queue.h"
#include "../../writeMethods/writeMethods.h"

enum logMethod {
	LM_PRIVATE_BUFFER, LM_SHARED_BUFFER, LM_DIRECT_WRITE
};

#define BUFFSIZE 65536 /* Used for buffering for the IO of log file */
#define LOGGERTHREADNAME "LoggerThread" /* Name of the logger thread */

static char* logFileBuff;
static int threadsNum;
static int maxMsgLen;
static int maxArgsLen;
static int privateBuffSize;
static atomic_bool isTerminate;
static atomic_bool isNewData;
static atomic_bool isDynamicAllocation;
static atomic_int logLevel;
static FILE* logFile;
static pthread_mutex_t loggerLock;
static pthread_mutex_t sharedBufferlock;
static pthread_mutex_t dynamicllyAllocaedLock;
static pthread_t loggerThread;
static struct Queue* privateBuffersQueue; /* Threads take and return buffers from this */
static struct MessageQueue** privateBuffers; /* Logger thread iterated over this */
static struct MessageQueue* sharedBuffer;
__thread struct MessageQueue* tlmq; /* Thread Local Message Queue */
static struct LinkedList* dynamicllyAllocaedPrivateBuffers;
static sem_t loggerLoopSem;
static sem_t loggerWaitingSem;
static void (*writeMethod)();

static bool isValitInitConditions(const int threadsNumArg,
                                  const int privateBuffSize,
                                  const int sharedBuffSize,
                                  const int loggingLevel);
static void setStaticValues(int privateBuffSizeArg, const int threadsNumArg,
                            const int maxArgsLenArg, const int maxMsgLenArg,
                            const int loggingLevel, void (*writeMethodArg)(),
                            const bool isDynamicAllocation);
static void initSynchronizationElements();
static void initMessageQueues(const int sharedBuffSize, const int maxArgsLenArg);
static void startLoggerThread();
static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(const int sharedBuffSize);
static void initPrivateBuffers(const int privateBuffSize);
static int writeTosharedBuffer(const int loggingLevel, char* file,
                               const char* func, const int line, va_list* args,
                               const char* msg);
static inline void drainPrivateBuffers();
static inline void drainSharedBuffer();
static inline bool isLoggingValid(const int loggingLevel, char* msg);
static inline char* getFileName(char* filePath);
static inline void drainDynamicllyAllocaedPrivateBuffers();
static void destroyDynamicallyAllocatedBuffers();

/* API method - Description located at .h file */
int initLogger(const int threadsNumArg, const int privateBuffSize,
               const int sharedBuffSize, const int loggingLevel,
               const int maxMsgLenArg, const int maxArgsLenArg,
               const bool isDynamicAllocationArg, void (*writeMethod)()) {
	if (true
	        == isValitInitConditions(threadsNumArg, privateBuffSize,
	                                 sharedBuffSize, loggingLevel)) {
		setStaticValues(privateBuffSize, threadsNumArg, maxArgsLenArg,
		                maxMsgLenArg, loggingLevel, writeMethod,
		                isDynamicAllocationArg);
		initSynchronizationElements();
		initMessageQueues(sharedBuffSize, maxArgsLenArg);
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
	if ((0 > threadsNumArg) || (2 > privateBuffSize) || (2 > sharedBuffSize)
	        || (loggingLevel < LOG_LEVEL_NONE)
	        || (loggingLevel > LOG_LEVEL_TRACE)
	        || (LOG_STATUS_SUCCESS != createLogFile())) {
		return false;
	}

	return true;
}

/**
 * Set static parameters
 * @param privateBuffSizeArg Size of private buffers
 * @param threadsNumArg Numbers of threads (available buffers)
 * @param maxArgsLenArg Maximum message length
 * @param maxMsgLenArg Maximum message arguments length
 * @param loggingLevel Logging level (one of the levels at 'logLevels')
 * @param writeMethodArg A pointer to a method that writes a message to a file
 * @param isDynamicAllocation Whether or not to enable dynamic buffers allocation
 */
static void setStaticValues(int privateBuffSizeArg, const int threadsNumArg,
                            const int maxArgsLenArg, const int maxMsgLenArg,
                            const int loggingLevel, void (*writeMethodArg)(),
                            const bool isDynamicAllocation) {
	privateBuffSize = privateBuffSizeArg;
	threadsNum = threadsNumArg;
	maxMsgLen = maxMsgLenArg;
	maxArgsLen = maxArgsLenArg;
	setLoggingLevel(loggingLevel);
	writeMethod = writeMethodArg;
	setDynamicAllocation(isDynamicAllocation);
	dynamicllyAllocaedPrivateBuffers = newLinkedList();
}

/**
 * Initialize static mutexes and semaphore
 */
static void initSynchronizationElements() {
	pthread_mutex_init(&loggerLock, NULL);
	pthread_mutex_init(&sharedBufferlock, NULL);
	pthread_mutex_init(&dynamicllyAllocaedLock, NULL);
	initDirectWriteLock();
	sem_init(&loggerLoopSem, 0, 0);
	sem_init(&loggerWaitingSem, 0, 0);
}

/**
 * Create message queues
 * @param sharedBuffSize Size of shared buffer
 * @param maxArgsLenArg Maximum message arguments length
 */
static void initMessageQueues(const int sharedBuffSize, const int maxArgsLenArg) {
	//TODO: add an option to dynamically change all of these:
	initPrivateBuffers(privateBuffSize);
	initsharedBuffer(sharedBuffSize);
}

/**
 * Starts (and name) the internal logger threads which drains buffers
 * to the log file
 */
static void startLoggerThread() {
	pthread_create(&loggerThread, NULL, runLogger, NULL);
	pthread_setname_np(loggerThread, LOGGERTHREADNAME);
}

/* API method - Description located at .h file */
inline void setLoggingLevel(const int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
inline void setDynamicAllocation(const bool isDynamicAllocationArg) {
	__atomic_store_n(&isDynamicAllocation, isDynamicAllocationArg,
	__ATOMIC_SEQ_CST);
}

/**
 * Initialize private buffers parameters
 * @param privateBuffSize Number of buffers
 */
static void initPrivateBuffers(const int privateBuffSize) {
	int i;

	privateBuffersQueue = newQueue(threadsNum);
	//TODO: think if malloc failures need to be handled
	privateBuffers = malloc(threadsNum * sizeof(*privateBuffers));

	for (i = 0; i < threadsNum; ++i) {
		struct MessageQueue* mq;

		mq = newMessageQueue(privateBuffSize, maxArgsLen, false);
		privateBuffers[i] = mq;

		/* Add a reference of this MessageQueue to privateBuffersQueue so threads may
		 * register and take it */
		enqueue(privateBuffersQueue, mq);
	}
}

/**
 * Initialize shared buffer data parameters
 * @param sharedBuffSize Size of buffer
 */
static void initsharedBuffer(const int sharedBuffSize) {
	sharedBuffer = newMessageQueue(sharedBuffSize, maxArgsLen, false);
}

/**
 * Creates a log file and set a buffer for it
 * @return LOG_STATUS_SUCCESS on success, LOG_STATUS_FAILURE on failure
 */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w+");

	/* Set a big buffer to avoid frequent flushing */
	if (NULL != logFile) {
		//TODO: think if malloc failures need to be handled
		logFileBuff = malloc(BUFFSIZE);
		setvbuf(logFile, logFileBuff, _IOFBF, BUFFSIZE);
		return LOG_STATUS_SUCCESS;
	}

	return LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
int registerThread() {
	tlmq = dequeue(privateBuffersQueue);

	/* No more pre-allocated buffers available. If dynamic allocation is enabled, allocate a buffer*/
	if (NULL == tlmq) {
		bool isDynamicAllocationLoc;

		__atomic_load(&isDynamicAllocation, &isDynamicAllocationLoc,
		__ATOMIC_SEQ_CST);
		if (true == isDynamicAllocationLoc) {
			struct LinkedListNode* node;
			struct MessageQueue* mq;

			mq = newMessageQueue(privateBuffSize, maxArgsLen, true);
			node = newLinkedListNode(mq);

			pthread_mutex_lock(&dynamicllyAllocaedLock); /* Lock */
			{
				addNode(dynamicllyAllocaedPrivateBuffers, node);
			}
			pthread_mutex_unlock(&dynamicllyAllocaedLock); /* Unlock */
			tlmq = mq;
		}
	}

	return (NULL != tlmq) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void unregisterThread() {
	if (NULL != tlmq) {
		if (false == getIsDynamicallyAllocated(tlmq)) {
			enqueue(privateBuffersQueue, tlmq);
		} else {
			decommisionBuffer(tlmq);
		}
		tlmq = NULL;
	}
}

/**
 * Logger thread loop - At each iteration, go over all the buffers and drain them to the log file,
 * flush buffer and sleep for a while
 */
static void* runLogger() {
	bool isTerminateLoc;
	bool isNewDataLoc;

	isTerminateLoc = false;
	isNewDataLoc = false;

	do {
		__atomic_store_n(&isNewData, false, __ATOMIC_SEQ_CST);
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainDynamicllyAllocaedPrivateBuffers();
		drainSharedBuffer();
		fflush(logFile); /* Flush buffer at the end of the iteration to avoid data staying in buffer long */

		/* The following is done to avoid wasting CPU in case no logging is being done
		 * (the main concern are 1-core CPU's and the current mechanism solves the issue) */
		//TODO: At the current state, it's enough that just 1 thread will try to log data
		// on order to prevent the logger thread from sleeping, need to think about
		// changing it so maybe some % of the threads need to log data instead.
		// On the other hand, in a real application, logging is performed constantly,
		// so this may not be a concern.
		__atomic_load(&isNewData, &isNewDataLoc, __ATOMIC_SEQ_CST);
		if (false == isNewDataLoc && false == isTerminateLoc) {
			sem_post(&loggerWaitingSem);
			sem_wait(&loggerLoopSem);
		}
	} while (!isTerminateLoc);

	return NULL;
}

/**
 * Iterate private buffers and drain them to file
 */
static inline void drainPrivateBuffers() {
	int i;

	for (i = 0; i < threadsNum; ++i) {
		drainMessages(privateBuffers[i], logFile, maxMsgLen, writeMethod);
	}
}

/**
 * Drain shared buffer to file
 */
static inline void drainSharedBuffer() {
	drainMessages(sharedBuffer, logFile, maxMsgLen, writeMethod);
}

static inline void drainDynamicllyAllocaedPrivateBuffers() {
	struct LinkedListNode* node = getHead(dynamicllyAllocaedPrivateBuffers);

	/* Check if it's worth locking the mutex */
	if (NULL != node) {
		pthread_mutex_lock(&dynamicllyAllocaedLock); /* Lock */
		{
			while (NULL != node) {
				struct MessageQueue* mq = getData(node);
				struct LinkedListNode* nextNode;

				drainMessages(mq, logFile, maxMsgLen, writeMethod);
				nextNode = getNext(node);

				if (true == isDecommisionedBuffer(mq)) {
					drainMessages(mq, logFile, maxMsgLen, writeMethod);
					free(removeNode(dynamicllyAllocaedPrivateBuffers, mq));
					free(mq);
				}

				node = nextNode;
			}
		}
	}
	pthread_mutex_unlock(&dynamicllyAllocaedLock); /* Unlock */
}

/* API method - Description located at .h file */
void terminateLogger() {
	__atomic_store_n(&isTerminate, true, __ATOMIC_SEQ_CST);
	sem_post(&loggerLoopSem);
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
	destroyDynamicallyAllocatedBuffers();
	sem_destroy(&loggerLoopSem);
	pthread_mutex_destroy(&dynamicllyAllocaedLock);
	pthread_mutex_destroy(&sharedBufferlock);
	pthread_mutex_destroy(&loggerLock);
	destroyDirectWriteLock();
	queueDestroy(privateBuffersQueue);
	fclose(logFile);
	free(logFileBuff);
}

/* API method - Description located at .h file */
int logMessage(const int loggingLevel, char* file, const char* func,
               const int line, char* msg, ...) {
	if (true == isLoggingValid(loggingLevel, msg)) {
		int writeToPrivateBuffer;
		va_list arg;

		/* Try each level of writing. If a level fails (buffer full), fall back to a
		 * lower & slower level.
		 * First, try private buffer writing. If private buffer doesn't exist
		 * (unregistered thread) or unable to write in this method, fall to
		 * next methods */
		file = getFileName(file);
		va_start(arg, msg);
		if (NULL != tlmq && false == isDecommisionedBuffer(tlmq)) {
			writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file, func,
			                                  line, &arg, msg,
			                                  LM_PRIVATE_BUFFER, maxArgsLen);
		} else {
			/* The current thread doesn't have a private buffer - Try to register */
			if (LOG_STATUS_SUCCESS == registerThread()) {
				/* Managed to get a private buffer - write to it */
				writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file,
				                                  func, line, &arg, msg,
				                                  LM_PRIVATE_BUFFER,
				                                  maxArgsLen);
			} else {
				/* Can't use private buffer */
				writeToPrivateBuffer = LOG_STATUS_FAILURE;
			}
		}

		if (LOG_STATUS_SUCCESS == writeToPrivateBuffer) {
			/* Communicate with logger thread */
			__atomic_store_n(&isNewData, true, __ATOMIC_SEQ_CST);
			if (0 == sem_trywait(&loggerWaitingSem)) {
				sem_post(&loggerLoopSem);
			}
		} else {
			/* Unable to write to private buffer
			 * Recommended not to get here - Register all threads and/or increase
			 * private buffers size */
			if (MQ_STATUS_SUCCESS
			        != writeTosharedBuffer(loggingLevel, file, func, line, &arg,
			                               msg)) {
				/* Unable to write to shared buffer
				 * Recommended not to get here - Increase private and shared buffers sizes */
				directWriteToFile(loggingLevel, file, func, line, &arg, msg,
				                  logFile, maxMsgLen, maxArgsLen,
				                  LM_DIRECT_WRITE, writeMethod);
				++cnt; //TODO: remove
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
static inline bool isLoggingValid(const int loggingLevel, char* msg) {
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
		                 msg, LM_SHARED_BUFFER, maxArgsLen);
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */

	/* Communicate with logger thread */
	if (MQ_STATUS_SUCCESS == ret) {
		__atomic_store_n(&isNewData, true, __ATOMIC_SEQ_CST);
	}

	return ret;
}

/* API method - Description located at .h file */
int getMaxMsgLen() {
	return maxMsgLen;
}

/**
 * Returns a file name from a file path
 * @param filePath The file path
 * @return The file name from a file path
 */
static inline char* getFileName(char* filePath) {
	char* c = strrchr(filePath, '/');
	return (NULL == c) ? filePath : ++c;
}

/**
 * Release all dynamically allocated buffers
 */
static void destroyDynamicallyAllocatedBuffers() {
	pthread_mutex_lock(&dynamicllyAllocaedLock); /* Lock */
	{
		struct LinkedListNode* node = getHead(dynamicllyAllocaedPrivateBuffers);

		while (NULL != node) {
			struct MessageQueue* mq = getData(node);
			struct LinkedListNode* nextNode;

			nextNode = getNext(node);
			free(removeNode(dynamicllyAllocaedPrivateBuffers, mq));
			free(mq);
			node = nextNode;
		}
	}
	pthread_mutex_unlock(&dynamicllyAllocaedLock); /* Unlock */
}
