/*
 ============================================================================
 Name        : ringBufferList.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include "ringBufferList.h"

typedef struct ringBufferListNode {
	struct ringBuffer* rb;
	struct ringBufferListNode* next;
} ringBufferListNode;

typedef struct ringBufferList {
	ringBufferListNode* head;
	ringBufferListNode* tail;
	pthread_mutex_t lock;
} ringBufferList;

/* Inlining light methods */
inline static bool isNodeContainingData(const ringBufferListNode* node,
                                        const struct ringBuffer* data);

/* API method - Description located at .h file */
ringBufferList* getNewList() {
	ringBufferList* rbl;

	//TODO: think if malloc failures need to be handled
	rbl = calloc(1, sizeof(ringBufferList));
	if (NULL != rbl) {
		pthread_mutex_init(&rbl->lock, NULL);
		return rbl;
	}

	return NULL;
}

/* API method - Description located at .h file */
void addNode(ringBufferList* rbl, struct ringBuffer* data) {
	//TODO: think if malloc failures need to be handled
	ringBufferListNode* newNode = malloc(sizeof(ringBufferListNode));

	newNode->rb = data;

	pthread_mutex_lock(&rbl->lock); /* Lock */
	{
		if (NULL == rbl->head) {
			rbl->head = newNode;
		} else {
			rbl->tail->next = newNode;
		}
		rbl->tail = newNode;
		rbl->tail->next = NULL;
	}
	pthread_mutex_unlock(&rbl->lock); /* Unlock */
}

/* API method - Description located at .h file */
struct ringBuffer* removeNode(ringBufferList* rbl,
                              const struct ringBuffer* data) {
	ringBufferListNode* next;

	pthread_mutex_lock(&rbl->lock); /* Lock */
	{
		ringBufferListNode* prev = NULL;

		next = rbl->head;
		while (NULL != next) {
			if (true == isNodeContainingData(next, data)) {
				if (NULL == prev) {
					/* Matching node is the first node */
					rbl->head = rbl->tail = NULL;
					goto Unlock;
				} else {
					/* Matching node is an arbitrary node */
					prev->next = next->next;
					goto Unlock;
				}
			}
			prev = next;
			next = next->next;
		}
	}
	Unlock: pthread_mutex_unlock(&rbl->lock); /* Unlock */

	return (NULL != next) ? next->rb : NULL;
}

/* API method - Description located at .h file */
ringBufferListNode* removeHead(ringBufferList* rbl) {
	ringBufferListNode* node;

	pthread_mutex_lock(&rbl->lock); /* Lock */
	{
		node = rbl->head;
		if (NULL != node) {
			rbl->head = node->next;
		}
	}
	pthread_mutex_unlock(&rbl->lock); /* Unlock */

	return node;
}

/* Return true if the data of 2 nodes is the same of false otherwise */
inline static bool isNodeContainingData(const ringBufferListNode* node,
                                        const struct ringBuffer* data) {
	return (node->rb == data) ? true : false;
}

/* API method - Description located at .h file */
void freeRingBufferList(ringBufferList* rbl) {
	ringBufferListNode* node;

	node = rbl->head;

	while (NULL != node) {
		struct ringBuffer* rb;

		rb = node->rb;
		deleteRingBuffer(rb);

		node = node->next;
	}
}

/* API method - Description located at .h file */
ringBufferListNode* getHead(const ringBufferList* rbl) {
	return rbl->head;
}

/* API method - Description located at .h file */
ringBufferListNode* getNext(const ringBufferListNode* node) {
	return node->next;
}

/* API method - Description located at .h file */
struct ringBuffer* getNodeRingBuffer(const ringBufferListNode* node) {
	return node->rb;
}
