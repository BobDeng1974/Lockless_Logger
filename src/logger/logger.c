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
#include <stdbool.h>
#include <stdarg.h>
#include <threads.h>

#include "logger.h"

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

typedef struct sharedBuffer {
	int lastRead;
	int lastWrite;
	int bufSize;
	char* buf;
	pthread_mutex_t lock;
} sharedBuffer;

static atomic_bool isTerminate;
static int privateBufferSize;
static int sharedBufferSize;
static FILE* logFile;
static int bufferDataArraySize;
static int nextFreeCell;
static bufferData** bufferDataArray;
static pthread_mutex_t loggerLock;
static pthread_t loggerThread;
static sharedBuffer sharedBuf;
static atomic_int logLevel;
thread_local bufferData* bd;

static void initPrivateBuffer(bufferData* bd);
static int createLogFile();
static void* runLogger();
static void freeResources();
static void initSharedBuffer();

inline static bool isNextWriteOverwrite(const int lastRead,
                                        const atomic_int lastWrite,
                                        const int lenToBufEnd);
inline static bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen);
inline static bool isWraparoundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);
inline static int writeToPrivateBuffer(bufferData* bd, messageInfo* msgInfo,
                                       const char* argsBuf);
inline static int writeSeq(char* buf, const int lastWrite,
                           const messageInfo* msgInfo, const char* argsBuf);
inline static int writeWrap(char* buf, const int lastWrite,
                            const int lenToBufEnd, const messageInfo* msgInfo,
                            const char* argsBuf);
inline static int writeToSharedBuffer(messageInfo* msgInfo, const char* argsBuf);
inline static int bufferdWriteToFile(const char* buf, const int lastRead,
                                     const int lastWrite, const int bufSize);
inline static void drainPrivateBuffers();
inline static void drainSharedBuffer();
inline static void directWriteToFile(messageInfo* msgInfo, const char* argsBuf);
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    const pthread_t tid, const int loggingLevel);
inline static void discardFilenamePrefix(char** file);
inline static int writeInFormat(char* buf, const messageInfo* msgInfo,
                                const char* argsBuf);
inline static int writeSeqOrWrap(char* buf, const int lastWrite,
                                 const int lenToBufEnd,
                                 const messageInfo* msgInfo,
                                 const char* argsBuf);

/* Initialize all data required by the logger.
 * Note: This method must be called before any other api is used
 * Parameters:
 * threadsNum - must be a non-negative value
 * privateBuffSize - must be a positive value
 * sharedBuffSize - must be a positive value
 * loggingLevel - must be one of the defined logging levels */
int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize,
               int loggingLevel) {
	int i;

	if (0 > threadsNum || 0 >= privateBuffSize || 0 >= sharedBuffSize
	        || (loggingLevel < LOG_LEVEL_NONE || loggingLevel > LOG_LEVEL_TRACE)) {
		return STATUS_FAILURE;
	}

	//TODO: add an option to dynamically change all of these:
	bufferDataArraySize = threadsNum;
	privateBufferSize = privateBuffSize;
	sharedBufferSize = sharedBuffSize;
	setLoggingLevel(loggingLevel);


	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	/* Init private buffers */
	//TODO: think if malloc failures need to be handled
	bufferDataArray = malloc(threadsNum * sizeof(bufferData*));
	for (i = 0; i < bufferDataArraySize; ++i) {
		bufferDataArray[i] = malloc(sizeof(bufferData));
		initPrivateBuffer(bufferDataArray[i]);
	}

	/* Init shared buffer */
	initSharedBuffer();

	/* Init mutexes */
	pthread_mutex_init(&loggerLock, NULL);
	pthread_mutex_init(&sharedBuf.lock, NULL);

	/* Run logger thread */
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* Sets logging level to the specified value */
void setLoggingLevel(int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/* Initialize given bufferData struct */
void initPrivateBuffer(bufferData* bd) {
	bd->bufSize = privateBufferSize;
	//TODO: think if malloc failures need to be handled
	bd->buf = malloc(privateBufferSize);

	/* Advance to 1, as an empty buffer is defined by having a difference of 1 between
	 * lastWrite and lastRead*/
	bd->lastWrite = 1;
}

void initSharedBuffer() {
	sharedBuf.bufSize = sharedBufferSize;
	sharedBuf.buf = malloc(sharedBufferSize);
	sharedBuf.lastWrite = 1;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");

	if (NULL == logFile) {
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

/* Register a worker thread at the logger and assign one of the buffers to it */
int registerThread() {
	int res;

	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		if (bufferDataArraySize != nextFreeCell) {
			/* There are still free cells to register to */
			bd = bufferDataArray[nextFreeCell++];
			res = STATUS_SUCCESS;
		} else {
			/* No more free cells - new threads cannot register for a
			 * private buffer */
			res = STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&loggerLock); /* Unlock */

	return res;
}

/* Logger thread loop */
static void* runLogger() {
	bool isTerminateLoc = false;

	do {
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainSharedBuffer();
		/* At each iteration flush buffers to avoid data loss */
		fflush(logFile);
	} while (!isTerminateLoc);

	return NULL;
}

/* Drain all private buffers assigned to worker threads to the log file */
inline static void drainPrivateBuffers() {
	int i;
	int newLastRead;
	atomic_int lastWrite;
	bufferData* bd;

	for (i = 0; i < bufferDataArraySize; ++i) {
		bd = bufferDataArray[i];
		if (NULL != bd) {
			/* Atomic load lastWrite, as it is changed by the worker thread */
			__atomic_load(&bd->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);

			newLastRead = bufferdWriteToFile(bd->buf, bd->lastRead, lastWrite,
			                                 bd->bufSize);
			if (STATUS_FAILURE != newLastRead) {
				/* Atomic store lastRead, as it is read by the worker thread */
				__atomic_store_n(&bd->lastRead, newLastRead, __ATOMIC_SEQ_CST);
			}
		}
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
inline static void drainSharedBuffer() {
	int newLastRead;

	pthread_mutex_lock(&sharedBuf.lock); /* Lock */
	{
		newLastRead = bufferdWriteToFile(sharedBuf.buf, sharedBuf.lastRead,
		                                 sharedBuf.lastWrite,
		                                 sharedBuf.bufSize);
		if (STATUS_FAILURE != newLastRead) {
			sharedBuf.lastRead = newLastRead;
		}
	}
	pthread_mutex_unlock(&sharedBuf.lock); /* Unlock */
}

/* Perform actual write to log file from a given buffer.
 * In a case of successful write, the method returns the new position
 * of lastRead, otherwise, the method returns STATUS_FAILURE. */
inline static int bufferdWriteToFile(const char* buf, const int lastRead,
                                     const int lastWrite, const int bufSize) {
	int dataLen;
	int lenToBufEnd;

	if (lastWrite > lastRead) {
		dataLen = lastWrite - lastRead - 1;

		if (dataLen > 0) {
			fwrite(buf + lastRead + 1, 1, dataLen, logFile);

			return lastWrite - 1;
		}
	} else {
		lenToBufEnd = bufSize - lastRead - 1;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			fwrite(buf + lastRead + 1, 1, lenToBufEnd, logFile);
			fwrite(buf, 1, lastWrite, logFile);

			return lastWrite - 1;
		}
	}

	return STATUS_FAILURE;
}

/* Terminate the logger thread and release resources */
void terminateLogger() {
	__atomic_store_n(&isTerminate, true, __ATOMIC_SEQ_CST);
	pthread_join(loggerThread, NULL);
	freeResources();
}

/* Release all malloc'ed resources, destroy mutexs and close open file */
static void freeResources() {
	int i;
	bufferData* bd;

	free(sharedBuf.buf);

	pthread_mutex_destroy(&sharedBuf.lock);

	for (i = 0; i < bufferDataArraySize; ++i) {
		bd = bufferDataArray[i];
		free(bd->buf);
		free(bd);
	}

	pthread_mutex_destroy(&loggerLock);

	fclose(logFile);
}

/* Add a message from a worker thread to a buffer or write it directly to file
 * if buffers are full.
 * Note: 'msg' must be a null-terminated string */
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
	__atomic_load(&logLevel, &loggingLevelLoc, __ATOMIC_SEQ_CST);
	if (LOG_LEVEL_NONE == loggingLevelLoc || loggingLevel > loggingLevelLoc) {
		return STATUS_FAILURE;
	}

	__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
	/* Don't log if:
	 * 1) Logger is terminating
	 * 2) 'msg' has an invalid value */
	if (true == isTerminateLoc || NULL == msg) {
		return STATUS_FAILURE;
	}

	/* Prepare all logging information */
	discardFilenamePrefix(&file);
	setMsgInfoValues(&msgInfo, file, func, line, pthread_self(), loggingLevel);

	/* Get message arguments values */
	va_start(arg, msg);
	vsprintf(argsBuf, msg, arg);
	va_end(arg);

	/* Try each level of writing. If a level fails (buffer full), fall back to a
	 * lower & slower level.
	 * First, try private buffer writing. If private buffer doesn't exist
	 * (unregistered thread) or unable to write in this method, fall to
	 * next methods */
	//TODO: still think if write from unregistered worker threads should be allowed
	if (NULL != bd) {
		writeToPrivateBufferRes = writeToPrivateBuffer(bd, &msgInfo, argsBuf);
	} else {
		writeToPrivateBufferRes = STATUS_FAILURE;
	}

	if (STATUS_SUCCESS != writeToPrivateBufferRes) {
		/* Recommended not to get here - Register all threads and/or increase
		 * private buffers size */
		if (STATUS_SUCCESS != writeToSharedBuffer(&msgInfo, argsBuf)) {
			/* Recommended not to get here - Increase private and shared buffers size */
			directWriteToFile(&msgInfo, argsBuf);
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
                                    const pthread_t tid, const int loggingLevel) {
	gettimeofday(&msgInfo->tv, NULL);
	msgInfo->file = file;
	msgInfo->func = func;
	msgInfo->line = line;
	msgInfo->tid = tid;
	msgInfo->logLevel = loggingLevel;
}

/* Add a message from a worker thread to its private buffer */
inline static int writeToPrivateBuffer(bufferData* bd, messageInfo* msgInfo,
                                       const char* argsBuf) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	__atomic_load(&bd->lastRead, &lastRead, __ATOMIC_SEQ_CST);

	lastWrite = bd->lastWrite;
	lenToBufEnd = bd->bufSize - lastWrite;

	if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd)) {
		msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
		newLastWrite = writeSeqOrWrap(bd->buf, lastWrite, lenToBufEnd, msgInfo,
		                              argsBuf);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&bd->lastWrite, newLastWrite, __ATOMIC_SEQ_CST);

		return STATUS_SUCCESS;
	}
	return STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
inline static int writeToSharedBuffer(messageInfo* msgInfo, const char* argsBuf) {
	int res;
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	pthread_mutex_lock(&sharedBuf.lock); /* Lock */
	{
		lastRead = sharedBuf.lastRead;
		lastWrite = sharedBuf.lastWrite;
		lenToBufEnd = sharedBuf.bufSize - sharedBuf.lastWrite;

		if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd)) {
			msgInfo->loggingMethod = LS_SHARED_BUFFER;
			newLastWrite = writeSeqOrWrap(sharedBuf.buf, lastWrite, lenToBufEnd,
			                              msgInfo, argsBuf);

			sharedBuf.lastWrite = newLastWrite;
			res = STATUS_SUCCESS;
		} else {
			res = STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&sharedBuf.lock); /* Unlock */

	return res;
}

/* Write to buffer in one of 2 ways:
 * Sequentially - If there is enough space at the end of the buffer
 * Wrap-around - If space at the end of the buffer is insufficient
 * 'space at the end of the buffer' is regarded as 'MAX_MSG_LEN' at this point,
 * as the true length of the message is unknown until it's fully composed*/
inline static int writeSeqOrWrap(char* buf, const int lastWrite,
                                 const int lenToBufEnd,
                                 const messageInfo* msgInfo,
                                 const char* argsBuf) {
	int newLastWrite;

	newLastWrite =
	        lenToBufEnd >= MAX_MSG_LEN ?
	                writeSeq(buf, lastWrite, msgInfo, argsBuf) :
	                writeWrap(buf, lastWrite, lenToBufEnd, msgInfo, argsBuf);

	return newLastWrite;
}

/* Check for 2 potential cases data override */
inline static bool isNextWriteOverwrite(const int lastRead,
                                        const atomic_int lastWrite,
                                        const int lenToBufEnd) {
	return (isSequentialOverwrite(lastRead, lastWrite, MAX_MSG_LEN)
	        || isWraparoundOverwrite(lastRead, MAX_MSG_LEN, lenToBufEnd));
}

/* Check for sequential data override */
inline static bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen) {
	return (lastWrite < lastRead && ((lastWrite + msgLen) >= lastRead));
}

/* Check for wrap-around data override */
inline static bool isWraparoundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd) {
	if (msgLen > lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* Write sequentially to a designated buffer */
inline static int writeSeq(char* buf, const int lastWrite,
                           const messageInfo* msgInfo, const char* argsBuf) {
	int msgLen;

	msgLen = writeInFormat(buf + lastWrite, msgInfo, argsBuf);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
inline static int writeWrap(char* buf, const int lastWrite,
                            const int lenToBufEnd, const messageInfo* msgInfo,
                            const char* argsBuf) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];
	/* local buffer is used as in the case of wrap-around write, writing is split to 2
	 * portions - write whatever possible at the end of the buffer and the rest write in
	 * the beginning. Since we don't know in advance the real length of the message we can't
	 * assume there is enough space at the end, therefore a temporary, long enough buffer is
	 * required into which data will be written sequentially and then copied back to the
	 * original buffer, either in sequential of wrap-around manner */

	msgLen = writeInFormat(locBuf, msgInfo, argsBuf);

	if (lenToBufEnd >= msgLen) {
		/* Space at the end of the buffer is sufficient - copy everything at once */
		memcpy(buf + lastWrite, locBuf, msgLen);
		return lastWrite + msgLen;
	} else {
		/* Space at the end of the buffer is insufficient - copy
		 * data written in the end and then data written back at the beginning */
		int bytesRemaining = msgLen - lenToBufEnd;
		memcpy(buf + lastWrite, locBuf, lenToBufEnd);
		memcpy(buf, locBuf + lenToBufEnd, bytesRemaining);
		return bytesRemaining;
	}

}

/* Worker thread directly writes to the log file */
inline static void directWriteToFile(messageInfo* msgInfo, const char* argsBuf) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];

	msgInfo->loggingMethod = LS_DIRECT_WRITE;
	msgLen = writeInFormat(locBuf, msgInfo, argsBuf);

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
inline static int writeInFormat(char* buf, const messageInfo* msgInfo,
                                const char* argsBuf) {
	int msgLen;
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

	msgLen =
	        sprintf(buf,
	                "[mid: %x:%.5x] [ll: %c] [lm: %s] [tid: %.8x] [loc: %s:%d:%s] [msg: %s]\n",
	                (unsigned int) msgInfo->tv.tv_sec,
	                (unsigned int) msgInfo->tv.tv_usec,
	                logLevelsIds[msgInfo->logLevel],
	                logMethods[msgInfo->loggingMethod],
	                (unsigned int) msgInfo->tid, msgInfo->file, msgInfo->line,
	                msgInfo->func, argsBuf);

	return msgLen;
}
