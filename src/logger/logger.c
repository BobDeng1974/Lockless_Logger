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

#include "logger.h"

enum statusCodes {
	STATUS_FAILURE = -1, STATUS_SUCCESS
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

static void initBufferData(bufferData* bd);
static int createLogFile();
static void* runLogger();
static void freeResources();

inline static bufferData* getPrivateBuffer(pthread_t tid);
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
inline static int writeToSharedBuffer(const messageInfo* msgInfo,
                                      const char* argsBuf);
inline static int bufferdWriteToFile(char* buf, const int lastRead,
                                     const int lastWrite, const int bufSize);
inline static void drainPrivateBuffers();
inline static void drainSharedBuffer();
inline static void directWriteToFile(const messageInfo* msgInfo,
                                     const char* argsBuf);
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    pthread_t tid);
inline static void discardFilenamePrefix(char** file);
inline static int writeInFormat(char* buf, const messageInfo* msgInfo,
                                const char* argsBuf);
inline static int writeSeqOrWrap(char* buf, const int lastWrite,
                                 const int lenToBufEnd,
                                 const messageInfo* msgInfo,
                                 const char* argsBuf);

/* Initialize all data required by the logger */
int initLogger(const int threadsNum, int privateBuffSize, int sharedBuffSize) {
	int i;

	if (0 >= threadsNum || 0 >= privateBuffSize || 0 >= sharedBuffSize) {
		return STATUS_FAILURE;
	}

	bufferDataArraySize = threadsNum;
	privateBufferSize = privateBuffSize;
	sharedBufferSize = sharedBuffSize;

	pthread_mutex_init(&loggerLock, NULL);

	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	/* Init private buffers */
//TODO: think if malloc failures need to be handled
	bufferDataArray = malloc(threadsNum * sizeof(bufferData*));
	for (i = 0; i < bufferDataArraySize; ++i) {
		bufferDataArray[i] = malloc(sizeof(bufferData));
		initBufferData(bufferDataArray[i]);
	}

	/* Init shared buffer */
	sharedBuf.bufSize = sharedBufferSize;
	sharedBuf.buf = malloc(sharedBufferSize * sizeof(char));
	sharedBuf.lastWrite = 1;
	pthread_mutex_init(&sharedBuf.lock, NULL);

	/* Run logger thread */
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* Initialize given bufferData struct */
void initBufferData(bufferData* bd) {
	bd->bufSize = privateBufferSize;
//TODO: think if malloc failures need to be handled
	bd->buf = malloc(privateBufferSize);
	bd->lastWrite = 1;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");

	if (NULL == logFile) {
		//TODO: handle error
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

/* Register a worker thread at the logger and assign one of the buffers to it */
int registerThread(pthread_t tid) {
	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		if (bufferDataArraySize == nextFreeCell) {
			return STATUS_FAILURE;
		}
		bufferDataArray[nextFreeCell++]->tid = tid;
	}
	pthread_mutex_unlock(&loggerLock); /* Unlock */

	return STATUS_SUCCESS;
}

/* Logger thread loop */
static void* runLogger() {
	bool isTerminateLoc = false;
	bool isCleanup = false;

	while (false == isTerminateLoc || true == isCleanup) {
		drainPrivateBuffers();
		drainSharedBuffer();
		/* At each iteration flush buffers to avoid data loss */
		fflush(logFile);
		if (true == isCleanup) {
			break;
		}
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		if (isTerminateLoc == true) {
			isCleanup = true;
			/* This allows the logger loop to run one last time, ensuring all
			 * data in the buffers will be written to the log file */
		}
	}

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
				__atomic_store_n(&bd->lastRead, newLastRead,
				__ATOMIC_SEQ_CST);
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
inline static int bufferdWriteToFile(char* buf, const int lastRead,
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
int logMessage(char* file, const char* func, const int line, const char* msg,
               ...) {
	bool isTerminateLoc;
	pthread_t tid;
	bufferData* bd;
	messageInfo msgInfo;
	va_list arg;
	char argsBuf[ARGS_LEN];

	/* Prevent writing new messages if logger is terminating */
	__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
	if (true == isTerminateLoc) {
		return STATUS_FAILURE;
	}

	if (NULL == msg) {
		return STATUS_FAILURE;
	}

	tid = pthread_self();

	bd = getPrivateBuffer(tid);
	if (NULL == bd) {
		//TODO: think if write from unregistered worker threads should be allowed
		return STATUS_FAILURE;
	}

	discardFilenamePrefix(&file);
	setMsgInfoValues(&msgInfo, file, func, line, tid);

	/* Get message arguments values */
	va_start(arg, msg);
	vsprintf(argsBuf, msg, arg);
	va_end(arg);

	/* Try each level of writing. If a level fails (buffer full), fall back to a
	 * lower & slower level */
	if (STATUS_SUCCESS != writeToPrivateBuffer(bd, &msgInfo, argsBuf)) {
		/* Recommended no to get here - Increase private buffers size */
		if (STATUS_SUCCESS != writeToSharedBuffer(&msgInfo, argsBuf)) {
			/* Recommended no to get here - Increase private and shared buffers size */
			directWriteToFile(&msgInfo, argsBuf);
			++cnt; //TODO: remove, for debug only
		}
	}

	return STATUS_SUCCESS;
}

/* Get a buffer assigned to a worker thread based on thread id */
inline static bufferData* getPrivateBuffer(pthread_t tid) {
	int i;
	bufferData* bd;

	for (i = 0; i < nextFreeCell; ++i) {
		bd = bufferDataArray[i];
		if (0 != pthread_equal(bd->tid, tid)) {
			return bd;
		}
	}

	return NULL;
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
                                    pthread_t tid) {
	gettimeofday(&msgInfo->tv, NULL);
	msgInfo->tm_info = localtime(&msgInfo->tv.tv_sec);
	msgInfo->millisec = msgInfo->tv.tv_usec;
	msgInfo->file = file;
	msgInfo->func = func;
	msgInfo->line = line;
	msgInfo->tid = tid;
}

/* Add a message from a worker thread to it's private buffer */
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
		newLastWrite = writeSeqOrWrap(bd->buf, lastWrite, lenToBufEnd, msgInfo,
		                              argsBuf);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&bd->lastWrite, newLastWrite,
		__ATOMIC_SEQ_CST);
		return STATUS_SUCCESS;
	}
	return STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
inline static int writeToSharedBuffer(const messageInfo* msgInfo,
                                      const char* argsBuf) {
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

/* Write to buffer in a sequential or wrap-around manner according to conditions */
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

	msgLen = writeInFormat(locBuf, msgInfo, argsBuf);

	if (lenToBufEnd >= msgLen) {
		memcpy(buf + lastWrite, locBuf, msgLen);
		return lastWrite + msgLen;
	} else {
		int bytesRemaining = msgLen - lenToBufEnd;
		memcpy(buf + lastWrite, locBuf, lenToBufEnd);
		memcpy(buf, locBuf + lenToBufEnd, bytesRemaining);
		return bytesRemaining;
	}

}

/* Worker thread directly writes to the log file */
inline static void directWriteToFile(const messageInfo* msgInfo,
                                     const char* argsBuf) {
	int msgLen;
	char locBuf[MAX_MSG_LEN];

	msgLen = writeInFormat(locBuf, msgInfo, argsBuf);

	fwrite(locBuf, 1, msgLen, logFile);
}

/* Log a message in a structured format:
 * mid - Message identifier. A time stamp in the format of
 * 		 (seconds + milliseconds):(milliseconds). It's logged this way
 * 		 so later on it can be parsed to (seconds):(milliseconds)
 * 		 format for an accurate time stamp
 * tid - Thread identifier
 * loc - Location in the format of (file):(line):(method)
 * msg - The message provided for the current log line */
inline static int writeInFormat(char* buf, const messageInfo* msgInfo,
                                const char* argsBuf) {
	int msgLen;

	msgLen = sprintf(buf,
	                 "[mid: %x:%.5x] [tid: %x] [loc: %s:%d:%s] [msg: %s]\n",
	                 (unsigned int) (msgInfo->tv.tv_sec + msgInfo->millisec),
	                 msgInfo->millisec, (unsigned int) msgInfo->tid,
	                 msgInfo->file, msgInfo->line, msgInfo->func, argsBuf);

	return msgLen;
}
