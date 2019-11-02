/*
 ============================================================================
 Name        : logger.c
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <threads.h>
#include <stdbool.h>

#include "../api/logger.h"
#include "../common/ringBufferList/linkedList/linkedList.h"
#include "../common/ringBufferList/ringBuffer/ringBuffer.h"
#include "../common/ringBufferList/ringBufferList.h"

enum logSource {
	LS_PRIVATE_BUFFER, LS_SHARED_BUFFER, LS_DIRECT_WRITE
};

typedef struct messageInfo {
	int line;
	int logLevel;
	int loggingMethod;
	char* file;
	char* argsBuf;
	const char* func;
	struct timeval tv;
	pthread_t tid;
} messageInfo;

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
static struct ringBuffer* sharedBuffer;
static pthread_mutex_t sharedBufferlock;

thread_local struct ringBuffer* tlrb; // Thread Local Private Buffer

static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(const int sharedBufferSize);
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize);
static int writeToPrivateBuffer(struct ringBuffer* rb, messageInfo* msgInfo);
static int writeTosharedBuffer(messageInfo* msgInfo);
static void drainPrivateBuffers();
static void drainsharedBuffer();
static void directWriteToFile(messageInfo* msgInfo);
static int writeInFormat(char* buf, void* data);

/* Inlining light methods */
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    const pthread_t tid, const int loggingLevel,
                                    char* argsBuf);
inline static void discardFilenamePrefix(char** file);

/* API method - Description located at .h file */
int initLogger(const int threadsNum, const int privateBuffSize,
               const int sharedBuffSize, const int loggingLevel) {
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

/* Initialize private buffers parameters */
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize) {
	int i;

	privateBuffers = newRingBufferList();
	availablePrivateBuffers = newRingBufferList();
	inUsePrivateBuffers = newRingBufferList();

	for (i = 0; i < threadsNum; ++i) {
		struct ringBuffer* rb;
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

/* Initialize shared buffer data parameters */
static void initsharedBuffer(const int sharedBufferSize) {
	sharedBuffer = newRingBuffer(sharedBufferSize, MAX_MSG_LEN);
}

/* Create log file */
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
	struct LinkedListNode* node;

	/* Take a node from the 'available buffers' pool */
	node = removeHead(availablePrivateBuffers);

	if (NULL != node) {
		tlrb = getRingBuffer(node);
		/* Add this node to the 'inUse' pool without allocating additional space */
		addNode(inUsePrivateBuffers, node);
	}

	return (NULL != tlrb) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void unregisterThread() {
	struct LinkedListNode* node;

	/* Find the note which contains 'rb' and remove it from the 'inUse' pool */
	node = removeNode(inUsePrivateBuffers, tlrb);
	if (NULL != node) {
		/* Return this node to the 'available buffers' pool */
		addNode(availablePrivateBuffers, node);
		tlrb = NULL;
	}
}

/* Logger thread loop */
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

/* Drain all private buffers assigned to worker threads to the log file */
static void drainPrivateBuffers() {
	struct LinkedListNode* node;

	node = getHead(privateBuffers); // Access is lockless, because if this list changes, private
	                                // buffers will be temporarily blocked

	while (NULL != node) {
		struct ringBuffer* rb;

		rb = getRingBuffer(node);
		drainBufferToFile(rb, fileHandle);
		node = getNext(node);
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
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

/* Release all malloc'ed resources, destroy mutexs and close the open file */
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
int logMessage(const int loggingLevel, char* file, const char* func,
               const int line, const char* msg, ...) {
	bool isTerminateLoc;
	int writeToPrivateBufferRes;
	int loggingLevelLoc;
	messageInfo msgInfo;
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
	setMsgInfoValues(&msgInfo, file, func, line, pthread_self(), loggingLevel,
	                 argsBuf);

	/* Try each level of writing. If a level fails (buffer full), fall back to a
	 * lower & slower level.
	 * First, try private buffer writing. If private buffer doesn't exist
	 * (unregistered thread) or unable to write in this method, fall to
	 * next methods */
	//TODO: still think if write from unregistered worker threads should be allowed
	if (NULL != tlrb) {
		writeToPrivateBufferRes = writeToPrivateBuffer(tlrb, &msgInfo);
	} else {
		/* The current thread doesn't have a private buffer - Try to register,
		 * maybe there's a free spot */
		if (LOG_STATUS_SUCCESS == registerThread()) {
			writeToPrivateBufferRes = writeToPrivateBuffer(tlrb, &msgInfo);
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

/* Get only the filename out of the full path */
inline static void discardFilenamePrefix(char** file) {
	char* lastSlash;

	lastSlash = strrchr(*file, '/');
	if (NULL != lastSlash) {
		*file = lastSlash + 1;
	}
}

/* Populate messageInfo struct */
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    const pthread_t tid, const int loggingLevel,
                                    char* argsBuf) {
	gettimeofday(&msgInfo->tv, NULL);
	msgInfo->file = file;
	msgInfo->func = func;
	msgInfo->line = line;
	msgInfo->tid = tid;
	msgInfo->logLevel = loggingLevel;
	msgInfo->argsBuf = argsBuf;
}

/* Add a message from a worker thread to it's private buffer */
static int writeToPrivateBuffer(struct ringBuffer* rb, messageInfo* msgInfo) {
	int res;

	msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
	res = writeToRingBuffer(rb, msgInfo, writeInFormat);

	return (RB_STATUS_SUCCESS == res) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
static int writeTosharedBuffer(messageInfo* msgInfo) {
	int res;

	msgInfo->loggingMethod = LS_SHARED_BUFFER;

	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		res = writeToRingBuffer(sharedBuffer, msgInfo, writeInFormat);
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */

	return (RB_STATUS_SUCCESS == res) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* Worker thread directly writes to the log file */
static void directWriteToFile(messageInfo* msgInfo) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];

	msgInfo->loggingMethod = LS_DIRECT_WRITE;
	msgLen = writeInFormat(locBuf, msgInfo);

	fwrite(locBuf, 1, msgLen, logFile);
}

/* Log a message in a structured format:
 * mid - Message identifier. A time stamp in the format of
 * 		 (seconds):(microseconds). A timestamp in yyyy-mm-dd can later be parsed
 * 		 using this information
 * ll  - Logging level
 * lm  - Logging method (private buffer, shared buffer, direct write)
 * tid - Thread identifier
 * loc - Location in the format of (file):(line):(method)
 * msg - The message provided for the current log line */
static int writeInFormat(char* buf, void* data) {
	int msgLen;
	messageInfo* msgInfo;

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

	msgInfo = (messageInfo*) data;

	msgLen =
	        sprintf(buf,
	                "[mid: %x:%.5x] [ll: %c] [lm: %s] [tid: %.8x] [loc: %s:%s:%d] [msg: %s]\n",
	                (unsigned int) msgInfo->tv.tv_sec,
	                (unsigned int) msgInfo->tv.tv_usec,
	                logLevelsIds[msgInfo->logLevel],
	                logMethods[msgInfo->loggingMethod],
	                (unsigned int) msgInfo->tid, msgInfo->file, msgInfo->func,
	                msgInfo->line, msgInfo->argsBuf);

	return msgLen;
}
