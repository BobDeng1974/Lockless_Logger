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
 * @file Queue.c
 * @author Barak Sason Rofman
 * @brief This module provides a generic Queue implementation
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include "Queue.h"

typedef struct Queue {
	int capacity;
	int size;
	int head;
	int tail;
	void** elements;
	pthread_mutex_t queueLock;
} Queue;

void initQueue(Queue* queue, int capacity);
static inline bool isEmpty(Queue* queue);
static inline bool isFull(Queue* queue);
static inline int getNextPos(int curPos, const int queueSize);

/* API method - Description located at .h file */
Queue* newQueue(int capacity) {
	Queue* queue;

	if (1 > capacity) {
		return NULL;
	}

	//TODO: think if malloc failures need to be handled
	queue = malloc(sizeof(*queue));
	initQueue(queue, capacity);

	return queue;
}

/* API method - Description located at .h file */
void initQueue(Queue* queue, int capacity) {
	queue->size = queue->head = queue->tail = 0;
	queue->capacity = capacity;
	//TODO: think if malloc failures need to be handled
	queue->elements = malloc(capacity * sizeof(*queue->elements));
	pthread_mutex_init(&queue->queueLock, NULL);
}

/**
 * Returns true if the given queue is empty or false otherwise
 * @param queue
 * @return true if the given queue is empty or false otherwise
 */
static bool isEmpty(Queue* queue) {
	return 0 == queue->size;
}

/**
 * Returns true if the given queue is full or false otherwise
 * @param queue
 * @return true if the given queue is full or false otherwise
 */
static bool isFull(Queue* queue) {
	return queue->size == queue->capacity;
}

/* API method - Description located at .h file */
int enqueue(Queue* queue, void* element) {
	int res;

	pthread_mutex_lock(&queue->queueLock); /* Lock */
	{
		if (false == isFull(queue)) {
			queue->elements[queue->tail] = element;
			queue->tail = getNextPos(queue->tail, queue->capacity);
			++queue->size;
			res = Q_STATUS_SUCCESS;
		} else {
			res = Q_STATUS_FAILURE;
		}
	}
	pthread_mutex_unlock(&queue->queueLock); /* Unlock */

	return res;
}

/* API method - Description located at .h file */
void* dequeue(Queue* queue) {
	void* element = NULL;

	pthread_mutex_lock(&queue->queueLock); /* Lock */
	{
		if (false == isEmpty(queue)) {
			element = queue->elements[queue->head];
			queue->elements[queue->head] = NULL;
			queue->head = getNextPos(queue->head, queue->capacity);
			--queue->size;
		}
	}
	pthread_mutex_unlock(&queue->queueLock); /* Unlock */

	return element;
}

/* API method - Description located at .h file */
void queueDestroy(Queue* queue) {
	pthread_mutex_destroy(&queue->queueLock);
	free(queue);
}

/**
 * Return the next position in a queue
 * @param curPos Current position
 * @param queueSize Queue size
 * @return The next position in a queue
 */
static inline int getNextPos(int curPos, const int queueSize) {
	return ++curPos >= queueSize ? 0 : curPos;
}
