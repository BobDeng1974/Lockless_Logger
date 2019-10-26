/*
 ============================================================================
 Name        : logger.c
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

//TODO: add additional status codes to describe different errors that occur
enum statusCodes {
	STATUS_FAILURE = -1, STATUS_SUCCESS
};

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

typedef struct privateBufferData {
	bool isFree;
	ringBuffer ringBuf;
} privateBufferData;

typedef struct sharedBufferData {
	pthread_mutex_t lock;
	ringBuffer ringBuf;
} sharedBufferData;

typedef struct privateBuffersManager {
	int freeSlots;
	int privateBufferSize;
	int bufferDataArraySize;
	privateBufferData** bufferDataArray;
} privateBuffersManager;

typedef struct loggerData {
	atomic_bool isTerminate;
	atomic_int logLevel;
	int fileHandle;
	FILE* logFile;
	pthread_mutex_t loggerLock;
	pthread_t loggerThread;
	privateBuffersManager pbm;
	sharedBufferData sbd;
} loggerData;

static loggerData logger;
thread_local privateBufferData* tlpbd; /* Thread Local Private Buffer Data */

static void initPrivateBuffer(privateBufferData* pbd);
static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBuffer(int sharedBufferSize);
static void initPrivateBuffersManager(int threadsNum, int privateBuffSize);
static int writeToPrivateBuffer(privateBufferData* pbd, messageInfo* msgInfo);
static int writeTosharedBuffer(messageInfo* msgInfo);
static void drainPrivateBuffers();
static void drainsharedBuffer();
static void directWriteToFile(messageInfo* msgInfo);
static int writeInFormat(char* buf, void* data);

/* Inlining small methods */
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    const pthread_t tid, const int loggingLevel,
                                    char* argsBuf);
inline static void discardFilenamePrefix(char** file);
inline static bool isPrivateBuffersAvailable();

/* API method - Description located at .h file */
int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize,
               int loggingLevel) {
	int i;

	if (0 > threadsNum || 0 >= privateBuffSize || 0 >= sharedBuffSize
	        || (loggingLevel < LOG_LEVEL_NONE || loggingLevel > LOG_LEVEL_TRACE)) {
		return STATUS_FAILURE;
	}

	/* Init all logger parameters */
	//TODO: add an option to dynamically change all of these:
	initPrivateBuffersManager(threadsNum, privateBuffSize);
	initsharedBuffer(sharedBuffSize);
	setLoggingLevel(loggingLevel);

	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	/* Init private buffers */
	//TODO: think if malloc failures need to be handled
	logger.pbm.bufferDataArray = malloc(
	        threadsNum * sizeof(privateBufferData*));
	for (i = 0; i < logger.pbm.bufferDataArraySize; ++i) {
		logger.pbm.bufferDataArray[i] = malloc(sizeof(privateBufferData));
		initPrivateBuffer(logger.pbm.bufferDataArray[i]);
	}

	/* Init mutexes */
	pthread_mutex_init(&logger.loggerLock, NULL);
	pthread_mutex_init(&logger.sbd.lock, NULL);

	/* Run logger thread */
	pthread_create(&logger.loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* API method - Description located at .h file */
void setLoggingLevel(int loggingLevel) {
	__atomic_store_n(&logger.logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/* Initialize private buffer manager's parameters */
static void initPrivateBuffersManager(int threadsNum, int privateBuffSize) {
	logger.pbm.freeSlots = logger.pbm.bufferDataArraySize = threadsNum;
	logger.pbm.privateBufferSize = privateBuffSize;
}

/* Initialize given privateBufferData struct */
void initPrivateBuffer(privateBufferData* pbd) {
	pbd->ringBuf.bufSize = logger.pbm.privateBufferSize;
	//TODO: think if malloc failures need to be handled
	pbd->ringBuf.buf = malloc(logger.pbm.privateBufferSize);

	/* Advance to 1, as an empty buffer is defined by having a difference of 1 between
	 * lastWrite and lastRead*/
	pbd->ringBuf.lastWrite = 1;

	pbd->isFree = true;
}

/* Initialize shared buffer data parameters */
static void initsharedBuffer(int sharedBufferSize) {
	logger.sbd.ringBuf.bufSize = sharedBufferSize;
	logger.sbd.ringBuf.buf = malloc(sharedBufferSize);
	logger.sbd.ringBuf.lastWrite = 1;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logger.logFile = fopen("logFile.txt", "w");
	logger.fileHandle = fileno(logger.logFile);

	if (NULL == logger.logFile) {
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

/* API method - Description located at .h file */
int registerThread() {
	int res;
	int i;

	//TODO: change data structure from array to linked list for better performance
	pthread_mutex_lock(&logger.loggerLock); /* Lock */
	{
		if (true == isPrivateBuffersAvailable()) {
			for (i = 0; i < logger.pbm.bufferDataArraySize; ++i) {
				if (true == logger.pbm.bufferDataArray[i]->isFree) {
					logger.pbm.bufferDataArray[i]->isFree = false;
					tlpbd = logger.pbm.bufferDataArray[i];
					--logger.pbm.freeSlots;
					res = STATUS_SUCCESS;
					goto Unlock;
				}
			}
		}
		res = STATUS_FAILURE;
	}
	Unlock: pthread_mutex_unlock(&logger.loggerLock); /* Unlock */

	return res;
}

/* API method - Description located at .h file */
void unregisterThread() {
	tlpbd = NULL;
	pthread_mutex_lock(&logger.loggerLock); /* Lock */
	{
		++logger.pbm.freeSlots;
	}
	pthread_mutex_unlock(&logger.loggerLock); /* Unlock */
}

/* Returns true if private buffers are still available or false otherwise */
inline static bool isPrivateBuffersAvailable() {
	return 0 != logger.pbm.freeSlots;
}

/* Logger thread loop */
static void* runLogger() {
	bool isTerminateLoc = false;

	do {
		__atomic_load(&logger.isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainsharedBuffer();
		/* At each iteration flush buffers to avoid data loss */
		fflush(logger.logFile);
	} while (!isTerminateLoc);

	return NULL;
}

/* Drain all private buffers assigned to worker threads to the log file */
static void drainPrivateBuffers() {
	int i;
	int newLastRead;
	atomic_int lastWrite;
	privateBufferData* pbd;

	for (i = 0; i < logger.pbm.bufferDataArraySize; ++i) {
		pbd = logger.pbm.bufferDataArray[i];
		if (NULL != pbd) {
			/* Atomic load lastWrite, as it is changed by the worker thread */
			__atomic_load(&pbd->ringBuf.lastWrite, &lastWrite,
			__ATOMIC_SEQ_CST);

			newLastRead = drainBufferToFile(logger.fileHandle, pbd->ringBuf.buf,
			                                pbd->ringBuf.lastRead, lastWrite,
			                                pbd->ringBuf.bufSize);
			if (STATUS_FAILURE != newLastRead) {
				/* Atomic store lastRead, as it is read by the worker thread */
				__atomic_store_n(&pbd->ringBuf.lastRead, newLastRead,
				__ATOMIC_SEQ_CST);
			}
		}
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
static void drainsharedBuffer() {
	int newLastRead;

	pthread_mutex_lock(&logger.sbd.lock); /* Lock */
	{
		newLastRead = drainBufferToFile(logger.fileHandle,
		                                logger.sbd.ringBuf.buf,
		                                logger.sbd.ringBuf.lastRead,
		                                logger.sbd.ringBuf.lastWrite,
		                                logger.sbd.ringBuf.bufSize);
		if (STATUS_FAILURE != newLastRead) {
			logger.sbd.ringBuf.lastRead = newLastRead;
		}
	}
	pthread_mutex_unlock(&logger.sbd.lock); /* Unlock */
}

/* API method - Description located at .h file */
void terminateLogger() {
	__atomic_store_n(&logger.isTerminate, true, __ATOMIC_SEQ_CST);
	pthread_join(logger.loggerThread, NULL);
	freeResources();
}

/* Release all malloc'ed resources, destroy mutexs and close open file */
static void freeResources() {
	int i;
	privateBufferData* pbd;

	free(logger.sbd.ringBuf.buf);

	pthread_mutex_destroy(&logger.sbd.lock);

	for (i = 0; i < logger.pbm.bufferDataArraySize; ++i) {
		pbd = logger.pbm.bufferDataArray[i];
		free(pbd->ringBuf.buf);
		free(pbd);
	}

	pthread_mutex_destroy(&logger.loggerLock);

	fclose(logger.logFile);
}

/* API method - Description located at .h file */
int logMessage(int loggingLevel, char* file, const int line, const char* func,
               const char* msg, ...) {
	bool isTerminateLoc;
	int writeToPrivateBufferRes;
	int loggingLevelLoc;
	messageInfo msgInfo;
	va_list arg;
	char argsBuf[ARGS_LEN];

	/* Don't log if trying to log messages with higher level than requested
	 * of log level was set to LOG_LEVEL_NONE */
	__atomic_load(&logger.logLevel, &loggingLevelLoc, __ATOMIC_SEQ_CST);
	if (LOG_LEVEL_NONE == loggingLevelLoc || loggingLevel > loggingLevelLoc) {
		return STATUS_FAILURE;
	}

	__atomic_load(&logger.isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
	/* Don't log if:
	 * 1) Logger is terminating
	 * 2) 'msg' has an invalid value */
	if (true == isTerminateLoc || NULL == msg) {
		return STATUS_FAILURE;
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
	if (NULL != tlpbd) {
		writeToPrivateBufferRes = writeToPrivateBuffer(tlpbd, &msgInfo);
	} else {
		/* The current thread doesn't have a private buffer - Try to register,
		 * maybe there's a free spot */
		if (STATUS_SUCCESS == registerThread()) {
			writeToPrivateBufferRes = writeToPrivateBuffer(tlpbd, &msgInfo);
		} else {
			writeToPrivateBufferRes = STATUS_FAILURE;
		}
	}

	if (STATUS_SUCCESS != writeToPrivateBufferRes) {
		/* Recommended not to get here - Register all threads and/or increase
		 * private buffers size */
		if (STATUS_SUCCESS != writeTosharedBuffer(&msgInfo)) {
			/* Recommended not to get here - Increase private and shared buffers size */
			directWriteToFile(&msgInfo);
			++cnt; //TODO: remove, for debug only (also - not accurate as not synchronized)
		}
	}

	return STATUS_SUCCESS;
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
static int writeToPrivateBuffer(privateBufferData* pbd, messageInfo* msgInfo) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	__atomic_load(&pbd->ringBuf.lastRead, &lastRead, __ATOMIC_SEQ_CST);

	lastWrite = pbd->ringBuf.lastWrite;
	lenToBufEnd = pbd->ringBuf.bufSize - lastWrite;

	if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd,
	MAX_MSG_LEN)) {
		msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
		newLastWrite = writeSeqOrWrap(pbd->ringBuf.buf, lastWrite, lenToBufEnd,
		                              MAX_MSG_LEN, msgInfo, writeInFormat);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&pbd->ringBuf.lastWrite, newLastWrite,
		__ATOMIC_SEQ_CST);

		return STATUS_SUCCESS;
	}
	return STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
static int writeTosharedBuffer(messageInfo* msgInfo) {
	int res;
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	pthread_mutex_lock(&logger.sbd.lock); /* Lock */
	{
		lastRead = logger.sbd.ringBuf.lastRead;
		lastWrite = logger.sbd.ringBuf.lastWrite;
		lenToBufEnd = logger.sbd.ringBuf.bufSize - logger.sbd.ringBuf.lastWrite;

		if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd,
		MAX_MSG_LEN)) {
			msgInfo->loggingMethod = LS_SHARED_BUFFER;
			newLastWrite = writeSeqOrWrap(logger.sbd.ringBuf.buf, lastWrite,
			                              lenToBufEnd, MAX_MSG_LEN, msgInfo,
			                              writeInFormat);

			logger.sbd.ringBuf.lastWrite = newLastWrite;
			res = STATUS_SUCCESS;
		} else {
			res = STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&logger.sbd.lock); /* Unlock */

	return res;
}

/* Worker thread directly writes to the log file */
static void directWriteToFile(messageInfo* msgInfo) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];

	msgInfo->loggingMethod = LS_DIRECT_WRITE;
	msgLen = writeInFormat(locBuf, msgInfo);

	fwrite(locBuf, 1, msgLen, logger.logFile);
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
