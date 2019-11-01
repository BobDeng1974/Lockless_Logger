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

/* Allocate a new ring buffer */
struct ringBuffer* newRingBuffer(int privateBuffSize);

/* Release all allocated resources to the given ring buffer */
void deleteRingBuffer(struct ringBuffer* rb);

/* Write 'data' to ring buffer 'rb' in the format created by 'formatMethod' and by using
 * theoretical message len 'safetyLen' */
int writeToRingBuffer(struct ringBuffer* rb, const int safetyLen, void* data,
                      const int (*formatMethod)());

/* Write to file 'file' from the buffer located in ringBuffer 'rb' */
void drainBufferToFile(struct ringBuffer* rb, const int file);

#endif /* RINGBUFFER_H */
