/*
 ============================================================================
 Name        : ringBufferList.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include <pthread.h>
#include <stdlib.h>

#include "ringBufferList.h"

#include "ringBuffer/ringBuffer.h"

inline static bool compareMethod(const struct LinkedListNode* node,
                                 const void* data);

/* API method - Description located at .h file */
inline struct LinkedList* newRingBufferList() {
	return newLinkedList(compareMethod);
}

/* Returns true if the the node 'node' contains data 'data' or false otherwise */
inline static bool compareMethod(const struct LinkedListNode* node,
                                 const void* data) {
	return (getData(node) == data) ? true : false;
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
