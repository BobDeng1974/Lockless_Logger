/*
 ============================================================================
 Name        : ringBuffer.h
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides a lockless implementation of a
 ring buffer for a single produce - single consumer
 use-case.
 ============================================================================
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdbool.h>

enum rinBuffertatusCodes {
	RB_STATUS_FAILURE = -1, RB_STATUS_SUCCESS
};

struct ringBuffer;

/* Allocate a new ring buffer and sets it's parameters.
 * 'privateBuffSize' - the size of the inner buffer.
 *  'maxMessageLen' - a length that is used to check whether or not the next write will
 *  need to wrap-around.
 *  NOTE: 'maxMessageLen' *MUST* be bigger than the maximum possible message
 *  length, otherwise a write-out-of-bound will occur.*/
struct ringBuffer* newRingBuffer(const int privateBuffSize, const int maxMessageLen);

/* Write 'data' to ring buffer 'rb' in the format created by 'formatMethod' and by using
 * theoretical message len 'maxMessageLen' */
int writeToRingBuffer(struct ringBuffer* rb, void* data,
                      const int (*formatMethod)());

/* Write to file 'file' from the buffer located in ringBuffer 'rb' */
void drainBufferToFile(struct ringBuffer* rb, const int file);

/* Release all allocated resources to the given ring buffer */
void deleteRingBuffer(struct ringBuffer* rb);

#endif /* RINGBUFFER_H */
