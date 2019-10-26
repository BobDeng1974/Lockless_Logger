/*
 * ringBuffer.h
 *
 *  Created on: Oct 26, 2019
 *      Author: bsasonro
 */

#ifndef RINGBUFFER
#define RINGBUFFER

#include <stdbool.h>
#include <stdatomic.h>

typedef struct ringBuffer {
	atomic_int lastRead;
	atomic_int lastWrite;
	int bufSize;
	char* buf;
} ringBuffer;

/* Check for 2 potential cases data override */
bool isNextWriteOverwrite(const int lastRead, const atomic_int lastWrite,
                          const int lenToBufEnd, const int safetyLen);

/* Write to buffer in one of 2 ways:
 * Sequentially - If there is enough space at the end of the buffer
 * Wrap-around - If space at the end of the buffer is insufficient
 * Note: 'space at the end of the buffer' is regarded as 'safetyLen' at this point,
 * as the true length of the message might be unknown until it's fully composed */
int writeSeqOrWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                   const int safetyLen, void* data, int (*formatMethod)());

/* Perform actual write to file from a given buffer.
 * In a case of successful write, the method returns the new position
 * of lastRead, otherwise, the method returns STATUS_FAILURE. */
int drainBufferToFile(const int file, const char* buf, const int lastRead,
                      const int lastWrite, const int bufSize);

#endif /* RINGBUFFER */
