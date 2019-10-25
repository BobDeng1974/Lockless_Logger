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

typedef struct sharedBufferData {
	int lastRead;
	int lastWrite;
	int bufSize;
	char* buf;
	pthread_mutex_t lock;
} sharedBufferData;

typedef struct privateBuffersManager {
	int freeSlots;
	int privateBufferSize;
	int bufferDataArraySize;
	privateBufferData** bufferDataArray;
} privateBuffersManager;

static atomic_bool isTerminate;
static FILE* logFile;
static int fileHandle;
static pthread_mutex_t loggerLock;
static pthread_t loggerThread;
static privateBuffersManager pbm;
static sharedBufferData sbd;
static atomic_int logLevel;
thread_local privateBufferData* tlpbd; /* Thread Local Private Buffer Data */

static void initPrivateBuffer(privateBufferData* pbd);
static int createLogFile();
static void* runLogger();
static void freeResources();
static void initsharedBufferData(int sharedBufferSize);
static void initPrivateBuffersManager(int threadsNum, int privateBuffSize);
static int writeToPrivateBuffer(privateBufferData* pbd, messageInfo* msgInfo,
                                const char* argsBuf);
static int writeSeq(char* buf, const int lastWrite, const messageInfo* msgInfo,
                    const char* argsBuf);
static int writeWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                     const messageInfo* msgInfo, const char* argsBuf);
static int writeTosharedBuffer(messageInfo* msgInfo, const char* argsBuf);
static int bufferdWriteToFile(const char* buf, const int lastRead,
                              const int lastWrite, const int bufSize);
static void drainPrivateBuffers();
static void drainsharedBufferData();
static void directWriteToFile(messageInfo* msgInfo, const char* argsBuf);
static int writeInFormat(char* buf, const messageInfo* msgInfo,
                         const char* argsBuf);
static int writeSeqOrWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                          const messageInfo* msgInfo, const char* argsBuf);

/* Inlining small methods */
inline static bool isNextWriteOverwrite(const int lastRead,
                                        const atomic_int lastWrite,
                                        const int lenToBufEnd);
inline static bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen);
inline static bool isWraparoundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);
inline static void setMsgInfoValues(messageInfo* msgInfo, char* file,
                                    const char* func, const int line,
                                    const pthread_t tid, const int loggingLevel);
inline static void discardFilenamePrefix(char** file);
inline static bool isPrivateBuffersAvailable();

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

	/* Init all logger parameters */
	//TODO: add an option to dynamically change all of these:
	initPrivateBuffersManager(threadsNum, privateBuffSize);
	initsharedBufferData(sharedBuffSize);
	setLoggingLevel(loggingLevel);

	if (STATUS_SUCCESS != createLogFile()) {
		return STATUS_FAILURE;
	}

	/* Init private buffers */
	//TODO: think if malloc failures need to be handled
	pbm.bufferDataArray = malloc(threadsNum * sizeof(privateBufferData*));
	for (i = 0; i < pbm.bufferDataArraySize; ++i) {
		pbm.bufferDataArray[i] = malloc(sizeof(privateBufferData));
		initPrivateBuffer(pbm.bufferDataArray[i]);
	}

	/* Init mutexes */
	pthread_mutex_init(&loggerLock, NULL);
	pthread_mutex_init(&sbd.lock, NULL);

	/* Run logger thread */
	pthread_create(&loggerThread, NULL, runLogger, NULL);

	return STATUS_SUCCESS;
}

/* Sets logging level to the specified value */
void setLoggingLevel(int loggingLevel) {
	__atomic_store_n(&logLevel, loggingLevel, __ATOMIC_SEQ_CST);
}

/* Initialize private buffer manager's parameters */
static void initPrivateBuffersManager(int threadsNum, int privateBuffSize) {
	pbm.freeSlots = pbm.bufferDataArraySize = threadsNum;
	pbm.privateBufferSize = privateBuffSize;
}

/* Initialize given privateBufferData struct */
void initPrivateBuffer(privateBufferData* pbd) {
	pbd->bufSize = pbm.privateBufferSize;
	//TODO: think if malloc failures need to be handled
	pbd->buf = malloc(pbm.privateBufferSize);

	/* Advance to 1, as an empty buffer is defined by having a difference of 1 between
	 * lastWrite and lastRead*/
	pbd->lastWrite = 1;

	pbd->isFree = true;
}

/* Initialize shared buffer data parameters */
static void initsharedBufferData(int sharedBufferSize) {
	sbd.bufSize = sharedBufferSize;
	sbd.buf = malloc(sharedBufferSize);
	sbd.lastWrite = 1;
}

/* Create log file */
static int createLogFile() {
	//TODO: implement rotating log
	logFile = fopen("logFile.txt", "w");
	fileHandle = fileno(logFile);

	if (NULL == logFile) {
		return STATUS_FAILURE;
	}

	return STATUS_SUCCESS;
}

/* Register a worker thread at the logger and assign a private buffers to it */
int registerThread() {
	int res;
	int i;

	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		if (true == isPrivateBuffersAvailable()) {
			for (i = 0; i < pbm.bufferDataArraySize; ++i) {
				if (pbm.bufferDataArray[i]->isFree) {
					tlpbd = pbm.bufferDataArray[i];
					--pbm.freeSlots;
					res = STATUS_SUCCESS;
					goto Unlock;
				}
			}
		}
		res = STATUS_FAILURE;
	}
	Unlock: pthread_mutex_unlock(&loggerLock); /* Unlock */

	return res;
}

/* Unregister a thread from the logger and free the private buffer for another thread's use */
void unregisterThread() {
	tlpbd = NULL;
	pthread_mutex_lock(&loggerLock); /* Lock */
	{
		++pbm.freeSlots;
	}
	pthread_mutex_unlock(&loggerLock); /* Unlock */
}

/* Returns true if private buffers are still available or false otherwise */
inline static bool isPrivateBuffersAvailable() {
	return 0 != pbm.freeSlots;
}

/* Logger thread loop */
static void* runLogger() {
	bool isTerminateLoc = false;

	do {
		__atomic_load(&isTerminate, &isTerminateLoc, __ATOMIC_SEQ_CST);
		drainPrivateBuffers();
		drainsharedBufferData();
		/* At each iteration flush buffers to avoid data loss */
		fflush(logFile);
	} while (!isTerminateLoc);

	return NULL;
}

/* Drain all private buffers assigned to worker threads to the log file */
static void drainPrivateBuffers() {
	int i;
	int newLastRead;
	atomic_int lastWrite;
	privateBufferData* pbd;

	for (i = 0; i < pbm.bufferDataArraySize; ++i) {
		pbd = pbm.bufferDataArray[i];
		if (NULL != pbd) {
			/* Atomic load lastWrite, as it is changed by the worker thread */
			__atomic_load(&pbd->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);

			newLastRead = bufferdWriteToFile(pbd->buf, pbd->lastRead, lastWrite,
			                                 pbd->bufSize);
			if (STATUS_FAILURE != newLastRead) {
				/* Atomic store lastRead, as it is read by the worker thread */
				__atomic_store_n(&pbd->lastRead, newLastRead, __ATOMIC_SEQ_CST);
			}
		}
	}
}

/* Drain the buffer which is shared by the worker threads to the log file */
static void drainsharedBufferData() {
	int newLastRead;

	pthread_mutex_lock(&sbd.lock); /* Lock */
	{
		newLastRead = bufferdWriteToFile(sbd.buf, sbd.lastRead, sbd.lastWrite,
		                                 sbd.bufSize);
		if (STATUS_FAILURE != newLastRead) {
			sbd.lastRead = newLastRead;
		}
	}
	pthread_mutex_unlock(&sbd.lock); /* Unlock */
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
			write(fileHandle, buf + lastRead + 1, dataLen);

			return lastWrite - 1;
		}
	} else {
		lenToBufEnd = bufSize - lastRead - 1;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			write(fileHandle, buf + lastRead + 1, lenToBufEnd);
			write(fileHandle, buf, lastWrite);

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
	privateBufferData* pbd;

	free(sbd.buf);

	pthread_mutex_destroy(&sbd.lock);

	for (i = 0; i < pbm.bufferDataArraySize; ++i) {
		pbd = pbm.bufferDataArray[i];
		free(pbd->buf);
		free(pbd);
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
	if (NULL != tlpbd) {
		writeToPrivateBufferRes = writeToPrivateBuffer(tlpbd, &msgInfo,
		                                               argsBuf);
	} else {
		/* The current thread doesn't have a private buffer - Try to register,
		 * maybe there's a free spot */
		if (STATUS_SUCCESS == registerThread()) {
			writeToPrivateBufferRes = writeToPrivateBuffer(tlpbd, &msgInfo,
			                                               argsBuf);
		} else {
			writeToPrivateBufferRes = STATUS_FAILURE;
		}
	}

	if (STATUS_SUCCESS != writeToPrivateBufferRes) {
		/* Recommended not to get here - Register all threads and/or increase
		 * private buffers size */
		if (STATUS_SUCCESS != writeTosharedBuffer(&msgInfo, argsBuf)) {
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

/* Add a message from a worker thread to it's private buffer */
static int writeToPrivateBuffer(privateBufferData* pbd, messageInfo* msgInfo,
                                const char* argsBuf) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	__atomic_load(&pbd->lastRead, &lastRead, __ATOMIC_SEQ_CST);

	lastWrite = pbd->lastWrite;
	lenToBufEnd = pbd->bufSize - lastWrite;

	if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd)) {
		msgInfo->loggingMethod = LS_PRIVATE_BUFFER;
		newLastWrite = writeSeqOrWrap(pbd->buf, lastWrite, lenToBufEnd, msgInfo,
		                              argsBuf);

		/* Atomic store lastWrite, as it is read by the logger thread */
		__atomic_store_n(&pbd->lastWrite, newLastWrite, __ATOMIC_SEQ_CST);

		return STATUS_SUCCESS;
	}
	return STATUS_FAILURE;
}

/* Add a message from a worker thread to the shared buffer */
static int writeTosharedBuffer(messageInfo* msgInfo, const char* argsBuf) {
	int res;
	int lastRead;
	int lastWrite;
	int lenToBufEnd;
	int newLastWrite;

	pthread_mutex_lock(&sbd.lock); /* Lock */
	{
		lastRead = sbd.lastRead;
		lastWrite = sbd.lastWrite;
		lenToBufEnd = sbd.bufSize - sbd.lastWrite;

		if (false == isNextWriteOverwrite(lastRead, lastWrite, lenToBufEnd)) {
			msgInfo->loggingMethod = LS_SHARED_BUFFER;
			newLastWrite = writeSeqOrWrap(sbd.buf, lastWrite, lenToBufEnd,
			                              msgInfo, argsBuf);

			sbd.lastWrite = newLastWrite;
			res = STATUS_SUCCESS;
		} else {
			res = STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&sbd.lock); /* Unlock */

	return res;
}

/* Write to buffer in one of 2 ways:
 * Sequentially - If there is enough space at the end of the buffer
 * Wrap-around - If space at the end of the buffer is insufficient
 * 'space at the end of the buffer' is regarded as 'MAX_MSG_LEN' at this point,
 * as the true length of the message is unknown until it's fully composed*/
static int writeSeqOrWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                          const messageInfo* msgInfo, const char* argsBuf) {
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
static int writeSeq(char* buf, const int lastWrite, const messageInfo* msgInfo,
                    const char* argsBuf) {
	int msgLen;

	msgLen = writeInFormat(buf + lastWrite, msgInfo, argsBuf);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
static int writeWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                     const messageInfo* msgInfo, const char* argsBuf) {
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
static void directWriteToFile(messageInfo* msgInfo, const char* argsBuf) {
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
static int writeInFormat(char* buf, const messageInfo* msgInfo,
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
