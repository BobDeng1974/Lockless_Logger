/*
 ============================================================================
 Name        : ringBuffer.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#include "ringBuffer.h"

typedef struct ringBuffer {
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	int lenToBufEnd;
	char* buf;
} ringBuffer;

/* Inlining light methods */
static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen);
static inline bool isWrapAroundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);

static int writeSeq(ringBuffer* rb, void* data, const int (*formatMethod)());
static int writeWrap(ringBuffer* rb, const int safetyLen, void* data,
                     const int (*formatMethod)());

/* API method - Description located at .h file */
ringBuffer* newRingBuffer(int privateBuffSize) {
	ringBuffer* rb;

	rb = malloc(sizeof(ringBuffer));
	initRingBuffer(rb, privateBuffSize);

	return rb;
}

/* API method - Description located at .h file */
void deleteRingBuffer(ringBuffer* rb) {
	free(rb->buf);
	free(rb);
}

/* API method - Description located at .h file */
void initRingBuffer(ringBuffer* rb, int privateBuffSize) {
	rb->bufSize = privateBuffSize;
	//TODO: think if malloc failures need to be handled
	rb->buf = malloc(privateBuffSize);

	/* Advance to 1, as an empty buffer is defined by having a difference of 1 between
	 * lastWrite and lastRead*/
	rb->lastWrite = 1;
}

/* API method - Description located at .h file */
int writeToRingBuffer(ringBuffer* rb, const int safetyLen, void* data,
                      const int (*formatMethod)()) {
	if (false == isNextWriteOverwrite(rb, safetyLen)) {
		int newLastWrite;

		newLastWrite = writeSeqOrWrap(rb, safetyLen, data, formatMethod);

		/* Atomic store lastWrite, as it's read by a different thread */
		__atomic_store_n(&rb->lastWrite, newLastWrite,
		__ATOMIC_SEQ_CST);

		return RB_STATUS_SUCCESS;
	}
	return RB_STATUS_FAILURE;
}

/* API method - Description located at .h file */
bool isNextWriteOverwrite(ringBuffer* rb, const int safetyLen) {
	int lastRead;
	int lastWrite;
	int lenToBufEnd;

	/* Atomic load lastRead, as it's written by a different thread */
	__atomic_load(&rb->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = rb->lastWrite;
	lenToBufEnd = rb->lenToBufEnd = rb->bufSize - lastWrite;

	return (isSequentialOverwrite(lastRead, lastWrite, safetyLen)
	        || isWrapAroundOverwrite(lastRead, safetyLen, lenToBufEnd));
}

/* Check for sequential data override */
static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen) {
	return (lastWrite < lastRead && ((lastWrite + msgLen) >= lastRead));
}

/* Check for wrap-around data override */
static inline bool isWrapAroundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd) {
	if (msgLen > lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* API method - Description located at .h file */
int writeSeqOrWrap(ringBuffer* rb, const int safetyLen, void* data,
                   const int (*formatMethod)()) {
	int newLastWrite;

	newLastWrite =
	        (rb->lenToBufEnd >= safetyLen) ?
	                writeSeq(rb, data, formatMethod) :
	                writeWrap(rb, safetyLen, data, formatMethod);

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

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
static int writeWrap(ringBuffer* rb, const int safetyLen, void* data,
                     const int (*formatMethod)()) {
	int msgLen;
	int lastWrite;
	int lenToBufEnd;
	char locBuf[safetyLen];
	/* local buffer is used as in the case of wrap-around write, writing is split to 2
	 * portions - write whatever possible at the end of the buffer and the rest write in
	 * the beginning. Since we don't know in advance the real length of the message we can't
	 * assume there is enough space at the end, therefore a temporary, long enough buffer is
	 * required into which data will be written sequentially and then copied back to the
	 * original buffer, either in sequential of wrap-around manner */

	lastWrite = rb->lastWrite;
	lenToBufEnd = rb->lenToBufEnd;
	msgLen = formatMethod(locBuf, data);

	if (rb->lenToBufEnd >= msgLen) {
		/* Space at the end of the buffer is sufficient - copy everything at once */
		memcpy(rb->buf + lastWrite, locBuf, msgLen);
		return rb->lastWrite + msgLen;
	} else {
		/* Space at the end of the buffer is insufficient - copy
		 * data written in the end and then data written back at the beginning */
		int bytesRemaining = msgLen - lenToBufEnd;
		memcpy(rb->buf + lastWrite, locBuf, lenToBufEnd);
		memcpy(rb->buf, locBuf + lenToBufEnd, bytesRemaining);
		return bytesRemaining;
	}

}

/* API method - Description located at .h file */
void drainBufferToFile(ringBuffer* rb, const int file) {
	int dataLen;
	int lenToBufEnd;
	int lastWrite;
	int newLastRead;

	/* Atomic load lastWrite, as it's read by a different thread */
	__atomic_load(&rb->lastWrite, &lastWrite,
	__ATOMIC_SEQ_CST);

	if (lastWrite > rb->lastRead) {
		dataLen = lastWrite - rb->lastRead - 1;

		if (dataLen > 0) {
			write(file, rb->buf + rb->lastRead + 1, dataLen);

			newLastRead = lastWrite - 1;
			/* Atomic store lastRead, as it's read by a different thread */
			__atomic_store_n(&rb->lastRead, newLastRead,
			__ATOMIC_SEQ_CST);
		}
	} else {
		lenToBufEnd = rb->bufSize - rb->lastRead - 1;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			write(file, rb->buf + rb->lastRead + 1, lenToBufEnd);
			write(file, rb->buf, lastWrite);

			newLastRead = lastWrite - 1;
			/* Atomic store lastRead, as it's read by a different thread */
			__atomic_store_n(&rb->lastRead, newLastRead,
			__ATOMIC_SEQ_CST);
		}
	}
}
