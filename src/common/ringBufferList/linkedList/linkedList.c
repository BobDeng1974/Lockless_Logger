/*
 ============================================================================
 Name        : abstractList.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include "linkedList.h"

#include <stdlib.h>
#include <pthread.h>

typedef struct LinkedList {
	struct LinkedListNode* head;
	struct LinkedListNode* tail;
	pthread_mutex_t lock;
	bool (*comparisonMethod)();
} LinkedList;

/* API method - Description located at .h file */
LinkedList* newLinkedList(bool (*comparisonMethod)()) {
	LinkedList* ll;

	//TODO: think if malloc failures need to be handled
	ll = calloc(1, sizeof(LinkedList));
	if (NULL != ll) {
		pthread_mutex_init(&ll->lock, NULL);
		ll->comparisonMethod = comparisonMethod;
		return ll;
	}

	return NULL;
}

/* API method - Description located at .h file */
void addNode(LinkedList* ll, struct LinkedListNode* node) {
	//TODO: think if malloc failures need to be handled

	pthread_mutex_lock(&ll->lock); /* Lock */
	{
		if (NULL == ll->head) {
			ll->head = node;
		} else {
			setNext(ll->tail, node);
		}
		ll->tail = node;
		setNext(ll->tail, NULL);
	}
	pthread_mutex_unlock(&ll->lock); /* Unlock */
}

/* API method - Description located at .h file */
struct LinkedListNode* removeHead(LinkedList* ll) {
	struct LinkedListNode* node;

	pthread_mutex_lock(&ll->lock); /* Lock */
	{
		node = ll->head;
		if (NULL != node) {
			/* List contains at least 1 node */
			ll->head = getNext(node);
			if (NULL == ll->head) {
				/* List is empty */
				ll->tail = NULL;
			}
		}
	}
	pthread_mutex_unlock(&ll->lock); /* Unlock */

	return node;
}

/* API method - Description located at .h file */
struct LinkedListNode* removeNode(LinkedList* ll, const void* data) {
	struct LinkedListNode* node;

	pthread_mutex_lock(&ll->lock); /* Lock */
	{
		struct LinkedListNode* prev = NULL;

		node = ll->head;
		while (NULL != node) {
			if (true == ll->comparisonMethod(node, data)) {
				if (NULL == prev) {
					/* Matching node is the first node */
					ll->head = ll->tail = NULL;
					goto Unlock;
				} else {
					/* Matching node is an arbitrary node */
					setNext(getNext(prev), getNext(node));
					goto Unlock;
				}
			}
			prev = node;
			node = getNext(node);
		}
	}
	Unlock: pthread_mutex_unlock(&ll->lock); /* Unlock */

	return node;
}

/* API method - Description located at .h file */
inline struct LinkedListNode* getHead(const LinkedList* ll) {
	return ll->head;
}

