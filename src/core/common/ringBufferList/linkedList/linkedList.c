/*
 ============================================================================
 Name        : linkedList.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides a generic linked list
 implementation and is based on 'linkedListNode' module.
 ============================================================================
 */

#include "linkedList.h"

#include <stdlib.h>
#include <pthread.h>

typedef struct LinkedList {
	struct LinkedListNode* head;
	struct LinkedListNode* tail;
} LinkedList;

static inline void resetList(LinkedList* ll);

/* API method - Description located at .h file */
LinkedList* newLinkedList() {
	LinkedList* ll = NULL;

	//TODO: think if malloc failures need to be handled
	ll = malloc(sizeof(LinkedList));
	if (NULL != ll) {
		resetList(ll);
	}

	return ll;
}

/* Reset a given list values */
static inline void resetList(LinkedList* ll) {
	ll->head = ll->tail = NULL;
}

/* API method - Description located at .h file */
void addNode(LinkedList* ll, struct LinkedListNode* node) {
	if (NULL == ll->head) {
		ll->head = node;
	} else {
		setNext(ll->tail, node);
	}
	ll->tail = node;
}

/* API method - Description located at .h file */
struct LinkedListNode* removeHead(LinkedList* ll) {
	struct LinkedListNode* node;
	struct LinkedListNode* nextNode;

	node = ll->head;

	if (NULL != node) {
		/* List contains at least 1 node */
		nextNode = getNext(node);
		if (NULL == nextNode) {
			/* List contains just 1 node, after removal it'll be empty */
			ll->head = ll->tail = NULL;
		} else {
			/* List contains multiple nodes */
			ll->head = nextNode;
			setNext(node, NULL);
		}
	}

	return node;
}

/* API method - Description located at .h file */
struct LinkedListNode* removeNode(LinkedList* ll,
                                  struct LinkedListNode* nodeToRemove) {
	struct LinkedListNode* node;
	struct LinkedListNode* prev = NULL;

	node = ll->head;

	while (NULL != node) {
		if (node == nodeToRemove) {
			if (NULL == prev) {
				if (NULL == getNext(node)) {
					/* Matching node is the only node */
					ll->head = ll->tail = NULL;
					break;
				} else {
					/* Matching node is the first node */
					ll->head = getNext(node);
					setNext(node, NULL);
					break;
				}
			} else {
				if (NULL == getNext(node)) {
					/* Matching node is the last node */
					setNext(prev, NULL);
					ll->tail = prev;
					break;
				} else {
					/* Matching node is an arbitrary node */
					setNext(prev, getNext(node));
					setNext(node, NULL);
					break;
				}
			}
		}
		prev = node;
		node = getNext(node);
	}

	return node;
}

/* API method - Description located at .h file */
inline struct LinkedListNode* getHead(const LinkedList* ll) {
	return ll->head;
}

/* API method - Description located at .h file */
inline struct LinkedListNode* getTail(const LinkedList* ll) {
	return ll->tail;
}

