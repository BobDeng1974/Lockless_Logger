/*
 ============================================================================
 Name        : c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <threads.h>

#include "logger.h"
#include "ringBuffer.h"
#include "ringBufferList.h"

enum logSource {
	LS_PRIVATE_BUFFER, LS_SHARED_BUFFER, LS_DIRECT_WRITE
};

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
static struct ringBufferList* privateBuffers;
static struct ringBufferList* availablePrivateBuffers;
static struct ringBufferList* inUsePrivateBuffers;
static ringBuffer sharedBuffer;
static pthread_mutex_t sharedBufferlock;

thread_local ringBuffer* tlrb; /* Thread Local Private Buffer */

static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(const int sharedBufferSize);
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize);
static int writeToPrivateBuffer(ringBuffer* rb, messageInfo* msgInfo);
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
void setLoggingLevel(const int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/* Initialize private buffers parameters */
static void initPrivateBuffers(const int threadsNum, const int privateBuffSize) {
	int i;

	privateBuffers = getNewList();
	availablePrivateBuffers = getNewList();
	inUsePrivateBuffers = getNewList();

	/* Init private buffers */
	//TODO: think if malloc failures need to be handled
	for (i = 0; i < threadsNum; ++i) {
		ringBuffer* rb;

		rb = malloc(sizeof(ringBuffer));
		initRingBuffer(rb, privateBuffSize);
		addNode(privateBuffers, rb);
		addNode(availablePrivateBuffers, rb);
	}
}

/* Initialize shared buffer data parameters */
static void initsharedBuffer(const int sharedBufferSize) {
	initRingBuffer(&sharedBuffer, sharedBufferSize);
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");
	fileHandle = fileno(logFile);

	if (NULL == logFile) {
		return LOG_STATUS_FAILURE;
	}

	return LOG_STATUS_SUCCESS;
}

/* API method - Description located at .h file */
int registerThread() {
	tlrb = getRingBuffer(removeHead(availablePrivateBuffers));

	if (NULL != tlrb) {
		addNode(inUsePrivateBuffers, tlrb);
	}

	return (NULL != tlrb) ? LOG_STATUS_SUCCESS : LOG_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void unregisterThread() {
	ringBuffer* rb;

	rb = removeNode(inUsePrivateBuffers, tlrb);
	if (NULL != rb) {
		addNode(availablePrivateBuffers, rb);
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
	struct ringBufferListNode* node;

	node = getHead(privateBuffers);

	while (NULL != node) {
		ringBuffer* rb;
		int newLastRead;
		atomic_int lastWrite;

		rb = getRingBuffer(node);
		/* Atomic load lastWrite, as it is changed by the worker thread */
		__atomic_load(&rb->lastWrite, &lastWrite,
		__ATOMIC_SEQ_CST);

		newLastRead = drainBufferToFile(fileHandle, rb->buf, rb->lastRead,
		                                lastWrite, rb->bufSize);
		if (LOG_STATUS_FAILURE != newLastRead) {
			/* Atomic store lastRead, as it is read by the worker thread */
			__atomic_store_n(&rb->lastRead, newLastRead,
			__ATOMIC_SEQ_CST);
		}

		node = getNext(node);
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
static void drainsharedBuffer() {
	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		int newLastRead;

		newLastRead = drainBufferToFile(fileHandle, sharedBuffer.buf,
		                                sharedBuffer.lastRead,
		                                sharedBuffer.lastWrite,
		                                sharedBuffer.bufSize);
		if (LOG_STATUS_FAILURE != newLastRead) {
			sharedBuffer.lastRead = newLastRead;
		}
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */
}

/* API method - Description located at .h file */
void terminateLogger() {
	__atomic_store_n(&isTerminate, true, __ATOMIC_SEQ_CST);
	pthread_join(loggerThread, NULL);
	freeResources();
}

/* Release all malloc'ed resources, destroy mutexs and close open file */
static void freeResources() {
	freeRingBufferList(privateBuffers);

	free(sharedBuffer.buf);

	pthread_mutex_destroy(&sharedBufferlock);
	pthread_mutex_destroy(&loggerLock);

	fclose(logFile);
}

/* API method - Description located at .h file */
int logMessage(const int loggingLevel, char* file, const int line, const char* func,
               const char* msg, ...) {
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
static int writeToPrivateBuffer(ringBuffer* rb, messageInfo* msgInfo) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	__atomic_load(&rb->lastRead, &lastRead, __ATOMIC_SEQ_CST);

	lastWrite = rb->lastWrite;
	lenToBufEnd = rb->bufSize - lastWrite;

	if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd,
	MAX_MSG_LEN)) {
		msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
		newLastWrite = writeSeqOrWrap(rb->buf, lastWrite, lenToBufEnd,
		MAX_MSG_LEN,
		                              msgInfo, writeInFormat);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&rb->lastWrite, newLastWrite,
		__ATOMIC_SEQ_CST);

		return LOG_STATUS_SUCCESS;
	}
	return LOG_STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
static int writeTosharedBuffer(messageInfo* msgInfo) {
	int res;

	pthread_mutex_lock(&sharedBufferlock); /* Lock */
	{
		int lastRead;
		int lastWrite;
		int lenToBufEnd;
		int newLastWrite;

		lastRead = sharedBuffer.lastRead;
		lastWrite = sharedBuffer.lastWrite;
		lenToBufEnd = sharedBuffer.bufSize - sharedBuffer.lastWrite;

		if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd,
		MAX_MSG_LEN)) {
			msgInfo->loggingMethod = LS_SHARED_BUFFER;
			newLastWrite = writeSeqOrWrap(sharedBuffer.buf, lastWrite,
			                              lenToBufEnd, MAX_MSG_LEN, msgInfo,
			                              writeInFormat);

			sharedBuffer.lastWrite = newLastWrite;
			res = LOG_STATUS_SUCCESS;
		} else {
			res = LOG_STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&sharedBufferlock); /* Unlock */

	return res;
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
	                "[mid: %x:%.5x] [ll: %c] [lm: %s] [tid: %.8x] [loc: %s:%d:%s] [msg: %s]\n",
	                (unsigned int) msgInfo->tv.tv_sec,
	                (unsigned int) msgInfo->tv.tv_usec,
	                logLevelsIds[msgInfo->logLevel],
	                logMethods[msgInfo->loggingMethod],
	                (unsigned int) msgInfo->tid, msgInfo->file, msgInfo->line,
	                msgInfo->func, msgInfo->argsBuf);

	return msgLen;
}
