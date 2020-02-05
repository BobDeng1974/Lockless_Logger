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
#include <unistd.h>

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
static int privateBuffersNum;
static int maxMsgLen;
static int maxArgsLen;
static int privateBuffSize;
static int newPrivateBuffSize;
static int newPrivateBuffersNumber;
static atomic_bool isTerminate;
static atomic_bool isNewData;
static atomic_bool isDynamicAllocation;
static atomic_bool arePrivateBuffersActive;
static atomic_bool arePrivateBuffersChangingSize;
static atomic_bool arePrivateBuffersChangingNumber;
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
static sem_t buffersChangingSizeSem;
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
static void drainDynamicllyAllocaedPrivateBuffers();
static void destroyDynamicallyAllocatedBuffers();
static void setArePrivateBuffersActive(const bool arePrivateBuffersActiveArg);
static inline bool arePrivateBuffersUsed();
static void doChangePrivateBuffersSize();
static inline void setArePrivateBuffersChangingSize(
        const bool arePrivateBuffersChangingSizeArg);
static void doChangePrivateBuffersNumber();
static inline void setArePrivateBuffersChangingNumber(
        const bool arePrivateBuffersChangingNumberArg);

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
	if ((threadsNumArg < 0) || (privateBuffSize < 2) || (sharedBuffSize < 2)
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
	privateBuffersNum = threadsNumArg;
	maxMsgLen = maxMsgLenArg;
	maxArgsLen = maxArgsLenArg;
	setLoggingLevel(loggingLevel);
	writeMethod = writeMethodArg;
	setDynamicAllocation(isDynamicAllocation);
	setArePrivateBuffersActive(true);
	setArePrivateBuffersChangingSize(false);
	setArePrivateBuffersChangingNumber(false);
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
	sem_init(&buffersChangingSizeSem, 0, 0);
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
 * Starts (and names) the internal logger threads which drains buffers
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
 * Set whether private buffers are active or not
 * @param arePrivateBuffersActiveArg Whether private buffers are active or not
 */
static inline void setArePrivateBuffersActive(
        const bool arePrivateBuffersActiveArg) {
	__atomic_store_n(&arePrivateBuffersActive, arePrivateBuffersActiveArg,
	__ATOMIC_SEQ_CST);
}

/**
 * Set whether private buffers are changing size
 * @param arePrivateBuffersChangingSizeArg Whether private buffers are changing size
 */
static inline void setArePrivateBuffersChangingSize(
        const bool arePrivateBuffersChangingSizeArg) {
	__atomic_store_n(&arePrivateBuffersChangingSize,
	                 arePrivateBuffersChangingSizeArg,
	                 __ATOMIC_SEQ_CST);
}

/**
 * Set whether private buffers are changing number
 * @param arePrivateBuffersChangingNumberArg Whether private buffers are changing number
 */
static inline void setArePrivateBuffersChangingNumber(
        const bool arePrivateBuffersChangingNumberArg) {
	__atomic_store_n(&arePrivateBuffersChangingNumber,
	                 arePrivateBuffersChangingNumberArg,
	                 __ATOMIC_SEQ_CST);
}

/**
 * Initialize private buffers parameters
 * @param privateBuffSize Number of buffers
 */
static void initPrivateBuffers(const int privateBuffSize) {
	int i;

	privateBuffersQueue = newQueue(privateBuffersNum);
	//TODO: think if malloc failures need to be handled
	privateBuffers = malloc(privateBuffersNum * sizeof(*privateBuffers));

	for (i = 0; i < privateBuffersNum; ++i) {
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
	} else {
		setIsTaken(tlmq, true);
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

		setIsTaken(tlmq, false);
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
	bool isFirstLevelActiveLoc;

	isTerminateLoc = false;
	isNewDataLoc = false;

	do {
		__atomic_load(&arePrivateBuffersActive, &isFirstLevelActiveLoc,
		__ATOMIC_SEQ_CST);
		if (true == arePrivateBuffersActive) {
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
		} else {
			bool arePrivateBuffersChangingSizeLoc;

			while (true == arePrivateBuffersUsed()) {
				/* Sleeping is not the optimal solution, but as buffer-changing events should be rare,
				 * in this case simplicity is probably preferable to efficiency */
				sleep(1);
			}

			__atomic_load(&arePrivateBuffersChangingSize,
			              &arePrivateBuffersChangingSizeLoc,
			              __ATOMIC_SEQ_CST);

			if (true == arePrivateBuffersChangingSizeLoc) {
				doChangePrivateBuffersSize();
				setArePrivateBuffersChangingSize(false);
			} else {
				bool arePrivateBuffersChangingNumberLoc;

				__atomic_load(&arePrivateBuffersChangingNumber,
				              &arePrivateBuffersChangingNumberLoc,
				              __ATOMIC_SEQ_CST);
				if (true == arePrivateBuffersChangingNumberLoc) {
					/* Waiting for 1 second in order to allow threads to unregister (will be
					 * performed automatically the next time they try to log a message).
					 * The buffer of any thread that doesn't unregister and release it's buffer
					 * will be moved to the dynamically allocated list in order not to loose
					 * it's pointer */
					struct timespec timeout = { .tv_nsec = 0 };

					timeout.tv_sec = time(NULL) + 1;
					sem_timedwait(&buffersChangingSizeSem, &timeout);
					doChangePrivateBuffersNumber();
					setArePrivateBuffersChangingNumber(false);
				}
			}
		}
	} while (!isTerminateLoc);

	return NULL;
}

/**
 * Initiates the process of changing private buffers size
 */
static void doChangePrivateBuffersSize() {
	int i;

	/* Changing private buffers size only affects buffers that are pre-allocated and not buffers
	 * that were dynamically allocated.
	 * In order to modify the size of dynamically allocated buffers, merge the layout and
	 * then modify buffers size */
	for (i = 0; i < privateBuffersNum; ++i) {
		struct MessageQueue* mq = privateBuffers[i];

		drainMessages(mq, logFile, maxMsgLen, writeMethod);
		changeBufferSize(mq, newPrivateBuffSize, maxArgsLen);
	}

	privateBuffSize = newPrivateBuffSize;
	setArePrivateBuffersActive(true);
}

/**
 * Initiates the process of changing private buffers number
 */
static void doChangePrivateBuffersNumber() {
	int i;

	/* Changing private buffers number only affects buffers that are pre-allocated and not buffers
	 * that were dynamically allocated */
	for (i = 0; i < privateBuffersNum; ++i) {
		struct MessageQueue* mq = privateBuffers[i];

		drainMessages(mq, logFile, maxMsgLen, writeMethod);
	}

	queueDestroy(privateBuffersQueue);
	privateBuffersQueue = newQueue(newPrivateBuffersNumber);

	for (i = 0; i < privateBuffersNum; ++i) {
		struct MessageQueue* mq;

		mq = privateBuffers[i];

		if (false == getIsPrivateBufferTaken(mq)) {
			messageDataQueueDestroy(mq);
		} else {
			enqueue(privateBuffersQueue, mq);
		}
	}

	free(privateBuffers);
	privateBuffers = malloc(newPrivateBuffersNumber * sizeof(*privateBuffers));

	for (i = 0; i < newPrivateBuffersNumber; ++i) {
		struct MessageQueue* mq;

		mq = newMessageQueue(privateBuffSize, maxArgsLen, false);
		privateBuffers[i] = mq;

		/* Add a reference of this MessageQueue to privateBuffersQueue so threads may
		 * register and take it */
		enqueue(privateBuffersQueue, mq);
	}

	privateBuffersNum = newPrivateBuffersNumber;
	setArePrivateBuffersActive(true);
}

/**
 * Checks if any of the private buffers is currently being used
 * @return True if any of the private buffers is currently being used of flase otherwise
 */
static inline bool arePrivateBuffersUsed() {
	int i;

	for (i = 0; i < privateBuffersNum; ++i) {
		if (true == getIsPrivateBufferBeingUsed(privateBuffers[i])) {
			return true;
		}
	}

	return false;
}

/**
 * Iterate private buffers and drain them to file
 */
static inline void drainPrivateBuffers() {
	int i;

	for (i = 0; i < privateBuffersNum; ++i) {
		drainMessages(privateBuffers[i], logFile, maxMsgLen, writeMethod);
	}
}

/**
 * Drain shared buffer to file
 */
static inline void drainSharedBuffer() {
	drainMessages(sharedBuffer, logFile, maxMsgLen, writeMethod);
}

/**
 * Drain data from the dynamically allocated buffers to the log file
 */
static void drainDynamicllyAllocaedPrivateBuffers() {
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

	for (i = 0; i < privateBuffersNum; ++i) {
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
void logMessage(const int loggingLevel, char* file, const char* func,
                const int line, char* msg, ...) {
	if (true == isLoggingValid(loggingLevel, msg)) {
		bool arePrivateBuffersActiveLoc;
		int writeToPrivateBuffer;
		va_list arg;

		/* Don't use private buffer if first level is disabled */
		__atomic_load(&arePrivateBuffersActive, &arePrivateBuffersActiveLoc,
		__ATOMIC_SEQ_CST);

		writeToPrivateBuffer = LOG_STATUS_FAILURE;
		file = getFileName(file);
		va_start(arg, msg);

		if (true == arePrivateBuffersActiveLoc) {
			/* Try each level of writing. If a level fails (buffer full), fall back to a
			 * lower & slower level.
			 * First, try private buffer writing. If private buffer doesn't exist
			 * (unregistered thread) or unable to write in this method, fall to
			 * next methods */
			if (NULL != tlmq) {
				setIsBeingUsed(tlmq, true);
			}

			if (NULL != tlmq) {
				if (false == isDecommisionedBuffer(tlmq)) {
					writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file,
					                                  func, line, &arg, msg,
					                                  LM_PRIVATE_BUFFER,
					                                  maxArgsLen);
				}
				setIsBeingUsed(tlmq, false);
			} else {
				/* The current thread doesn't have a private buffer - Try to register */
				if (LOG_STATUS_SUCCESS == registerThread()) {
					setIsBeingUsed(tlmq, true);

					/* Managed to get a private buffer - write to it */
					writeToPrivateBuffer = addMessage(tlmq, loggingLevel, file,
					                                  func, line, &arg, msg,
					                                  LM_PRIVATE_BUFFER,
					                                  maxArgsLen);
					setIsBeingUsed(tlmq, false);
				}
			}
		}

		if (LOG_STATUS_SUCCESS == writeToPrivateBuffer) {
			/* Communicate with logger thread */
			__atomic_store_n(&isNewData, true, __ATOMIC_SEQ_CST);
			if (0 == sem_trywait(&loggerWaitingSem)) {
				sem_post(&loggerLoopSem);
			}
		} else {
			bool arePrivateBuffersChangingNumberLoc;

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

			__atomic_load(&arePrivateBuffersChangingNumber,
			              &arePrivateBuffersChangingNumberLoc,
			              __ATOMIC_SEQ_CST);

			if (true == arePrivateBuffersChangingNumberLoc) {
				unregisterThread();
			}
		}
		va_end(arg);
	}
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
	/* Don't log if logger is terminating or msg' has an invalid value */
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

/* API method - Description located at .h file */
void changePrivateBuffersSize(const int newSize) {
	if (2 > newSize) {
		setArePrivateBuffersChangingSize(true);
		setArePrivateBuffersActive(false);
		sem_post(&loggerLoopSem);
		newPrivateBuffSize = newSize;
	}
}

/* API method - Description located at .h file */
void changePrivateBuffersNumber(const int newNumber) {
	if (newNumber > 0) {
		setArePrivateBuffersChangingNumber(true);
		setArePrivateBuffersActive(false);
		sem_post(&loggerLoopSem);
		newPrivateBuffersNumber = newNumber;
	}
}
