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
 * @file Queue.h
 * @author Barak Sason Rofman
 * @brief This module provides a generic Queue implementation
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef QUEUE_H
#define QUEUE_H

enum QueueStatusCodes {
	Q_STATUS_FAILURE = -1, Q_STATUS_SUCCESS
};

struct Queue;

/**
 * Creates a new Queue
 * @param capacity Queue's capacity
 * @return A pointer to the newly allocated Queue, or NULL on failure
 */
struct Queue* newQueue(int capacity);

/**
 * Adds an element to the queue
 * @param queue The Queue to add the element to
 * @param element The element to add
 * @return Q_STATUS_SUCCESS on success, Q_STATUS_FAILURE on failure (if queue is full)
 */
int enqueue(struct Queue* queue, void* element);

/**
 * Removes an element from the queue
 * @return The removed element, or NULL if nothing was removed (queue empty)
 */
void* dequeue();

/**
 * Releases all resources associated with the given Queue
 * @param queue The Queue to destroy
 */
void queueDestroy(struct Queue* queue);

#endif /* QUEUE_H */
