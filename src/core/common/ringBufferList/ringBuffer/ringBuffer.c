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
 * @file ringBuffer.c
 * @author Barak Sason Rofman
 * @brief This module provides a lockless implementation of a ring buffer for a
 * single produce - single consumer use-case.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "ringBuffer.h"

typedef struct RingBuffer {
	/** The position which data was last read from */
	atomic_int lastRead;
	/** The position which data was last written to */
	atomic_int lastWrite;
	/** The size of the buffer */
	int bufSize;
	/** The length to the end of the buffer from the lastWrite position */
	int lenToBufEnd;
	/** The maximum message length that can be written to the buffer */
	int maxMessageLen;
	/** Pointer to the internal buffer */
	char* buf;
} RingBuffer;

static inline bool isSequentialOverwrite(const int lastRead, const int lastWrite,
                                         const int maxMessageLen);
static inline bool isWrapAroundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);
static inline void setLastRead(RingBuffer* rb, int lastWrite);
static int writeSeq(RingBuffer* rb, void* data, const int (*formatMethod)());
static int checkWriteWrap(RingBuffer* rb, const int maxMessageLen, void* data,
                          const int (*formatMethod)());
static void initRingBuffer(struct RingBuffer* rb, int bufSize, const int maxMessageLen);
static bool isNextWriteOverwrite(struct RingBuffer* rb, const int maxMessageLen);
static int writeSeqOrWrap(struct RingBuffer* rb, const int maxMessageLen, void* data,
                          const int (*formatMethod)());
static void drainSeq(RingBuffer* rb, const int file, int lastRead, int lastWrite);
static void drainWrap(RingBuffer* rb, const int file, int lastRead, int lastWrite);
static int copySeq(RingBuffer* rb, char* locBuf, int msgLen);
static int writeWrap(RingBuffer* rb, char* locBuf, int msgLen, int lenToBufEnd);

/* API method - Description located at .h file */
RingBuffer* newRingBuffer(const int bufSize, const int maxMessageLen) {
	RingBuffer* rb = NULL;

	//TODO: think if malloc failures need to be handled
	rb = malloc(sizeof(RingBuffer));
	if (NULL != rb) {
		initRingBuffer(rb, bufSize, maxMessageLen);
	}

	return rb;
}

/**
 * Initialize a given RingBuffer
 * @param rb The RingBuffer to initialize
 * @param bufSize Size of the buffer
 * @param maxMessageLen Maximum length of a message that can be written
 */
static void initRingBuffer(RingBuffer* rb, const int bufSize, const int maxMessageLen) {
	rb->bufSize = bufSize;
	//TODO: think if malloc failures need to be handled
	rb->buf = malloc(bufSize);

	rb->lastRead = 0;
	rb->lastWrite = 1; // Advance to 1, as an empty buffer is defined by having a difference of 1 between
	                   // lastWrite and lastRead
	rb->maxMessageLen = maxMessageLen;
}

/* API method - Description located at .h file */
int writeToRingBuffer(RingBuffer* rb, void* data, const int (*formatMethod)()) {
	int maxMessageLen;

	maxMessageLen = rb->maxMessageLen;

	if (false == isNextWriteOverwrite(rb, maxMessageLen)) {
		int newLastWrite;

		newLastWrite = writeSeqOrWrap(rb, maxMessageLen, data, formatMethod);

		/* Atomic store lastWrite, as it's read by a different thread */
		__atomic_store_n(&rb->lastWrite, newLastWrite, __ATOMIC_SEQ_CST);

		return RB_STATUS_SUCCESS;
	}
	return RB_STATUS_FAILURE;
}

/**
 *  Check for 2 potential cases data override (sequential or warp-around)
 * @param rb The RingBuffer to test
 * @param maxMessageLen Maximum length of a message that can be written
 * @return True if next write will cause overwrite or false otherwise
 */
static bool isNextWriteOverwrite(RingBuffer* rb, const int maxMessageLen) {
	int lastRead;
	int lastWrite;

	/* Atomic load lastRead, as it's written by a different thread */
	__atomic_load(&rb->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = rb->lastWrite;
	rb->lenToBufEnd = rb->bufSize - lastWrite;

	return (isSequentialOverwrite(lastRead, lastWrite, maxMessageLen)
	        || isWrapAroundOverwrite(lastRead, maxMessageLen, rb->lenToBufEnd));
}

/**
 * Check for sequential data override
 * @param lastRead Position last read from
 * @param lastWrite Position last written to
 * @param maxMessageLen Maximum length of a message that can be written
 * @return True if next write will cause sequential overwrite or false otherwise
 */
static inline bool isSequentialOverwrite(const int lastRead, const int lastWrite,
                                         const int maxMessageLen) {
	return (lastWrite < lastRead && ((lastWrite + maxMessageLen) >= lastRead));
}

/**
 * Check for wrap-around  data override
 * @param lastRead Position last read from
 * @param maxMessageLen Maximum length of a message that can be written
 * @param lenToBufEnd Length to the end of the buffer from the lastWrite position
 * @return
 */
static inline bool isWrapAroundOverwrite(const int lastRead, const int maxMessageLen,
                                         const int lenToBufEnd) {
	if (maxMessageLen > lenToBufEnd) {
		int bytesRemaining = maxMessageLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/**
 * Write to buffer in one of 2 ways:
 * Sequentially - If there is enough space at the end of the buffer
 * Wrap-around - If space at the end of the buffer is insufficient
 * Note: 'space at the end of the buffer' is regarded as 'maxMessageLen' at this point,
 * as the true length of the message might be unknown until it's fully composed
 * @param rb The RingBuffer to write to
 * @param maxMessageLen Maximum length of a message that can be written
 * @param data Data to write
 * @param formatMethod Formatter method for the data
 * @return Position of the new last write
 */
static int writeSeqOrWrap(RingBuffer* rb, const int maxMessageLen, void* data,
                          const int (*formatMethod)()) {
	int newLastWrite;

	newLastWrite =
	        (rb->lenToBufEnd >= maxMessageLen) ?
	                writeSeq(rb, data, formatMethod) :
	                checkWriteWrap(rb, maxMessageLen, data, formatMethod);

	return newLastWrite;
}

/**
 * Write sequentially to a designated buffer
 * @param rb The RingBuffer to write to
 * @param data Data to write
 * @param formatMethod Formatter method for the data
 * @return Position of the new last write
 */
static int writeSeq(RingBuffer* rb, void* data, const int (*formatMethod)()) {
	int msgLen;
	int lastWrite;

	lastWrite = rb->lastWrite;

	msgLen = formatMethod(rb->buf + lastWrite, data, rb->maxMessageLen);

	return lastWrite + msgLen;
}

/**
 *  Space at the end of the buffer might be insufficient - calculate the length of the
 * message and based on the length decide whether to use sequential or wrap-around method
 * @param rb The RingBuffer to write to
 * @param maxMessageLen Maximum length of a message that can be written
 * @param data Data to write
 * @param formatMethod Formatter method for the data
 * @return Position of the new last write
 */
static int checkWriteWrap(RingBuffer* rb, const int maxMessageLen, void* data,
                          const int (*formatMethod)()) {
	int msgLen;
	int lenToBufEnd;
	char locBuf[maxMessageLen]; // local buffer is used since we don't know in advance the
	                            // real length of the message we can't assume there is enough
	                            // space at the end, therefore a temporary, long enough buffer
	                            // is required into which data will be written sequentially
	                            // and then copied back to the original buffer, either in
	                            // sequential of wrap-around manner
	msgLen = formatMethod(locBuf, data, maxMessageLen);
	lenToBufEnd = rb->lenToBufEnd;

	return (lenToBufEnd >= msgLen) ?
	        copySeq(rb, locBuf, msgLen) : writeWrap(rb, locBuf, msgLen, lenToBufEnd);
}

/**
 * Space at the end of the buffer is sufficient - copy message back to buffer
 * sequentially
 * @param rb The RingBuffer to write to
 * @param locBuf The local buffer to copy written data from
 * @param msgLen Actual length of written data
 * @return Position of the new last write
 */
static int copySeq(RingBuffer* rb, char* locBuf, int msgLen) {
	int lastWrite;

	lastWrite = rb->lastWrite;

	memcpy(rb->buf + lastWrite, locBuf, msgLen);
	return lastWrite + msgLen;
}

/**
 * Space at the end of the buffer is insufficient - copy back to buffer
 * in a wrap-around manner
 * @param rb The RingBuffer to write to
 * @param locBuf The local buffer to copy written data from
 * @param msgLen Actual length of written data
 * @param lenToBufEnd Length to the end of the buffer from the lastWrite position
 * @return Number of bytes that wrapped around
 */
static int writeWrap(RingBuffer* rb, char* locBuf, int msgLen, int lenToBufEnd) {
	int bytesRemaining;
	char* buf;

	bytesRemaining = msgLen - lenToBufEnd;
	buf = rb->buf;

	memcpy(buf + rb->lastWrite, locBuf, lenToBufEnd);
	memcpy(buf, locBuf + lenToBufEnd, bytesRemaining);
	return bytesRemaining;
}

/* API method - Description located at .h file */
void drainBufferToFile(RingBuffer* rb, const int file) {
	int lastWrite;
	int lastRead;

	/* Atomic load lastWrite, as it's read by a different thread */
	__atomic_load(&rb->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);
	lastRead = rb->lastRead;

	lastWrite > lastRead ?
	        drainSeq(rb, file, lastRead, lastWrite) : drainWrap(rb, file, lastRead, lastWrite);
}

/**
 * Drain data stored in buffer to file in a sequential manner
 * @param rb The RingBuffer to read from
 * @param file The file to write to
 * @param lastRead The position which data was last read from in 'rb'
 * @param lastWrite The position which data was last written to in 'rb'
 */
static void drainSeq(RingBuffer* rb, const int file, int lastRead, int lastWrite) {
	int dataLen;

	dataLen = lastWrite - lastRead - 1;

	if (dataLen > 0) {
		write(file, rb->buf + lastRead + 1, dataLen);

		setLastRead(rb, lastWrite);
	}
}

/**
 * Drain data stored in buffer to file in a wrap-around manner
 * @param rb The RingBuffer to read from
 * @param file The file to write to
 * @param lastRead The position which data was last read from in 'rb'
 * @param lastWrite The position which data was last written to in 'rb'
 */
static void drainWrap(RingBuffer* rb, const int file, int lastRead, int lastWrite) {
	int lenToBufEnd;
	int dataLen;

	lenToBufEnd = rb->bufSize - lastRead - 1;
	dataLen = lenToBufEnd + lastWrite - 1;

	if (dataLen > 0) {
		char* buf;

		buf = rb->buf;
		write(file, buf + lastRead + 1, lenToBufEnd);
		write(file, buf, lastWrite);

		setLastRead(rb, lastWrite);
	}
}

/**
 * Update the lastWrite value for a given RingBuffer 'rb'
 * @param rb The RingBuffer to update
 * @param lastWrite The value of the new lastWrite
 */
static inline void setLastRead(RingBuffer* rb, int lastWrite) {
	int newLastRead;

	newLastRead = lastWrite - 1;
	/* Atomic store lastRead, as it's read by a different thread */
	__atomic_store_n(&rb->lastRead, newLastRead, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
inline void deleteRingBuffer(RingBuffer* rb) {
	free(rb->buf);
	free(rb);
}
