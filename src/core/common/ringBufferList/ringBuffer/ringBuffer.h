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
 * @file ringBuffer.h
 * @author Barak Sason Rofman
 * @brief This module provides a lockless implementation of a ring buffer for a
 * single produce - single consumer use-case.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H

enum rinBuffertatusCodes {
	RB_STATUS_FAILURE = -1, RB_STATUS_SUCCESS
};

struct RingBuffer;

/**
 * Allocate a new ring buffer and sets it's parameters
 * @param bufSize The size of the inner buffer
 * @param maxMessageLen A length that is used to check whether or not the next write will
 *  need to wrap-around. If a message longer than this value is written, only the first maxMessageLen
 *  will actually be written.
 * @return The newly allocated RingBuffer
 */
struct RingBuffer* newRingBuffer(const int bufSize, const int maxMessageLen);

/**
 * Write 'data' to RingBuffer 'rb' in the format created by 'formatMethod' and by using
 * theoretical message length 'maxMessageLen'
 * @param rb The RingBuffer to write to
 * @param data Data to write
 * @param formatMethod Formatter method for the data
 * @return RB_STATUS_SUCCESS on success, RB_STATUS_FAILURE on failure
 */
int writeToRingBuffer(struct RingBuffer* rb, void* data, const int (*formatMethod)());

/**
 * Write to file 'file' from the buffer located in RingBuffer 'rb'
 * @param rb The RingBuffer to read from
 * @param file The file to write to
 */
void drainBufferToFile(struct RingBuffer* rb, const int file);

/**
 * Release all allocated resources to the given ring buffer
 * @param rb The RingBuffer to deallocate
 */
void deleteRingBuffer(struct RingBuffer* rb);

#endif /* RINGBUFFER_H */
