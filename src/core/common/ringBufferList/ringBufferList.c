/*
 ============================================================================
 Name        : ringBufferList.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides implementation of a ring buffer list.
 This module is based on the submodules - 'linkedList' and
 'ringBuffer'
 ============================================================================
 */

#include <pthread.h>
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
		struct ringBuffer* rb;
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
inline struct ringBuffer* getRingBuffer(struct LinkedListNode* node) {
	return (struct ringBuffer*) getData(node);
}
