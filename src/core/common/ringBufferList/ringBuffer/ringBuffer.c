/*
 ============================================================================
 Name        : ringBuffer.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides a lockless implementation of a
 ring buffer for a single produce - single consumer
 use-case.
 ============================================================================
 */

#include "../ringBuffer/ringBuffer.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

typedef struct ringBuffer {
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	int lenToBufEnd;
	int safetyLen;
	char* buf;
} ringBuffer;

static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int safetyLen);
static inline bool isWrapAroundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);
static int writeSeq(ringBuffer* rb, void* data, const int (*formatMethod)());
static int checkWriteWrap(ringBuffer* rb, const int safetyLen, void* data,
                          const int (*formatMethod)());
static void initRingBuffer(struct ringBuffer* rb, int privateBuffSize,
                           const int safetyLen);
static bool isNextWriteOverwrite(struct ringBuffer* rb, const int safetyLen);
static int writeSeqOrWrap(struct ringBuffer* rb, const int safetyLen,
                          void* data, const int (*formatMethod)());
static void drainSeq(ringBuffer* rb, const int file, int lastRead,
                     int lastWrite);
static void drainWrap(ringBuffer* rb, const int file, int lastRead,
                      int lastWrite);
static int copySeq(ringBuffer* rb, char* locBuf, int msgLen);
static int writeWrap(ringBuffer* rb, char* locBuf, int msgLen, int lenToBufEnd);

/* API method - Description located at .h file */
ringBuffer* newRingBuffer(const int privateBuffSize, const int safetyLen) {
	ringBuffer* rb;

	//TODO: think if malloc failures need to be handled
	rb = malloc(sizeof(ringBuffer));
	initRingBuffer(rb, privateBuffSize, safetyLen);

	return rb;
}

/* Initialize given privateBufferData struct */
static void initRingBuffer(ringBuffer* rb, const int privateBuffSize,
                           const int safetyLen) {
	rb->bufSize = privateBuffSize;
	//TODO: think if malloc failures need to be handled
	rb->buf = malloc(privateBuffSize);

	rb->lastWrite = 1; // Advance to 1, as an empty buffer is defined by having a difference of 1 between
	                   // lastWrite and lastRead
	rb->safetyLen = safetyLen;
}

/* API method - Description located at .h file */
int writeToRingBuffer(ringBuffer* rb, void* data, const int (*formatMethod)()) {
	int safetyLen;

	safetyLen = rb->safetyLen;

	if (false == isNextWriteOverwrite(rb, safetyLen)) {
		int newLastWrite;

		newLastWrite = writeSeqOrWrap(rb, safetyLen, data, formatMethod);

		/* Atomic store lastWrite, as it's read by a different thread */
		__atomic_store_n(&rb->lastWrite, newLastWrite, __ATOMIC_SEQ_CST);

		return RB_STATUS_SUCCESS;
	}
	return RB_STATUS_FAILURE;
}

/* Check for 2 potential cases data override */
static bool isNextWriteOverwrite(ringBuffer* rb, const int safetyLen) {
	int lastRead;
	int lastWrite;

	/* Atomic load lastRead, as it's written by a different thread */
	__atomic_load(&rb->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = rb->lastWrite;
	rb->lenToBufEnd = rb->bufSize - lastWrite;

	return (isSequentialOverwrite(lastRead, lastWrite, safetyLen)
	        || isWrapAroundOverwrite(lastRead, safetyLen, rb->lenToBufEnd));
}

/* Check for sequential data override */
static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int safetyLen) {
	return (lastWrite < lastRead && ((lastWrite + safetyLen) >= lastRead));
}

/* Check for wrap-around data override */
static inline bool isWrapAroundOverwrite(const int lastRead, const int safetyLen,
                                         const int lenToBufEnd) {
	if (safetyLen > lenToBufEnd) {
		int bytesRemaining = safetyLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* Write to buffer in one of 2 ways:
 * Sequentially - If there is enough space at the end of the buffer
 * Wrap-around - If space at the end of the buffer is insufficient
 * Note: 'space at the end of the buffer' is regarded as 'safetyLen' at this point,
 * as the true length of the message might be unknown until it's fully composed */
static int writeSeqOrWrap(ringBuffer* rb, const int safetyLen, void* data,
                          const int (*formatMethod)()) {
	int newLastWrite;

	newLastWrite =
	        (rb->lenToBufEnd >= safetyLen) ?
	                writeSeq(rb, data, formatMethod) :
	                checkWriteWrap(rb, safetyLen, data, formatMethod);

	return newLastWrite;
}

/* Write sequentially to a designated buffer */
static int writeSeq(ringBuffer* rb, void* data, const int (*formatMethod)()) {
	int msgLen;
	int lastWrite;

	lastWrite = rb->lastWrite;

	msgLen = formatMethod(rb->buf + lastWrite, data);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer might be insufficient - calculate the length of the
 * message and based on the length decide whether to use sequential or wrap-around method */
static int checkWriteWrap(ringBuffer* rb, const int safetyLen, void* data,
                          const int (*formatMethod)()) {
	int msgLen;
	int lenToBufEnd;
	char locBuf[safetyLen]; // local buffer is used since we don't know in advance the
	                        // real length of the message we can't assume there is enough
	                        // space at the end, therefore a temporary, long enough buffer
	                        // is required into which data will be written sequentially
	                        // and then copied back to the original buffer, either in
	                        // sequential of wrap-around manner
	msgLen = formatMethod(locBuf, data);
	lenToBufEnd = rb->lenToBufEnd;

	return (lenToBufEnd >= msgLen) ?
	        copySeq(rb, locBuf, msgLen) :
	        writeWrap(rb, locBuf, msgLen, lenToBufEnd);
}

/* Space at the end of the buffer is sufficient - copy message back to buffer
 * sequentially */
static int copySeq(ringBuffer* rb, char* locBuf, int msgLen) {
	int lastWrite;

	lastWrite = rb->lastWrite;

	memcpy(rb->buf + lastWrite, locBuf, msgLen);
	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - copy back to buffer
 * in a wrap-around manner */
static int writeWrap(ringBuffer* rb, char* locBuf, int msgLen, int lenToBufEnd) {
	int bytesRemaining;
	char* buf;

	bytesRemaining = msgLen - lenToBufEnd;
	buf = rb->buf;

	memcpy(buf + rb->lastWrite, locBuf, lenToBufEnd);
	memcpy(buf, locBuf + lenToBufEnd, bytesRemaining);
	return bytesRemaining;
}

/* API method - Description located at .h file */
void drainBufferToFile(ringBuffer* rb, const int file) {
	int lastWrite;
	int lastRead;

	/* Atomic load lastWrite, as it's read by a different thread */
	__atomic_load(&rb->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);
	lastRead = rb->lastRead;

	lastWrite > lastRead ?
	        drainSeq(rb, file, lastRead, lastWrite) :
	        drainWrap(rb, file, lastRead, lastWrite);
}

/* Drain data stored in buffer to file in a sequential manner */
static void drainSeq(ringBuffer* rb, const int file, int lastRead,
                     int lastWrite) {
	int dataLen;
	int newLastRead;

	dataLen = lastWrite - lastRead - 1;

	if (dataLen > 0) {
		write(file, rb->buf + lastRead + 1, dataLen);

		newLastRead = lastWrite - 1;
		/* Atomic store lastRead, as it's read by a different thread */
		__atomic_store_n(&rb->lastRead, newLastRead, __ATOMIC_SEQ_CST);
	}
}

/* Drain data stored in buffer to file in a wrap-around manner */
static void drainWrap(ringBuffer* rb, const int file, int lastRead,
                      int lastWrite) {
	int lenToBufEnd;
	int dataLen;
	int newLastRead;

	lenToBufEnd = rb->bufSize - lastRead - 1;
	dataLen = lenToBufEnd + lastWrite - 1;

	if (dataLen > 0) {
		char* buf;

		buf = rb->buf;
		write(file, buf + lastRead + 1, lenToBufEnd);
		write(file, buf, lastWrite);

		newLastRead = lastWrite - 1;
		/* Atomic store lastRead, as it's read by a different thread */
		__atomic_store_n(&rb->lastRead, newLastRead, __ATOMIC_SEQ_CST);
	}
}

/* API method - Description located at .h file */
inline void deleteRingBuffer(ringBuffer* rb) {
	free(rb->buf);
	free(rb);
}
