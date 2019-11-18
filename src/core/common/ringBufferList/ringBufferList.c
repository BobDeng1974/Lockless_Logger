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
 * @file ringBufferList.c
 * @author Barak Sason Rofman
 * @brief This module provides implementation of a ring buffer list.
 * This module is based on the submodules - 'linkedList' and 'ringBuffer'.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include <stdlib.h>

#include "ringBufferList.h"
#include "ringBuffer/ringBuffer.h"

/* API method - Description located at .h file */
inline struct LinkedList* newRingBufferList() {
	return newLinkedList();
}

/* API method - Description located at .h file */
void deepDeleteRingBufferList(struct LinkedList* ll) {
	struct LinkedListNode* node;

	node = getHead(ll);

	while (NULL != node) {
		struct RingBuffer* rb;
		struct LinkedListNode* tmp;

		rb = getRingBuffer(node);
		deleteRingBuffer(rb);

		tmp = node;
		node = getNext(node);
		free(tmp);
	}

	free(ll);
}

/* API method - Description located at .h file */
void shallowDeleteRingBufferList(struct LinkedList* ll) {
	struct LinkedListNode* node;

	node = getHead(ll);

	while (NULL != node) {
		struct LinkedListNode* tmp;

		tmp = node;
		node = getNext(node);
		free(tmp);
	}

	free(ll);
}

/* API method - Description located at .h file */
inline struct RingBuffer* getRingBuffer(struct LinkedListNode* node) {
	return (struct RingBuffer*) getData(node);
}
