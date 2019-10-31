/*
 ============================================================================
 Name        : linkedListNode.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides implementation for node operations
 	 	 	   in a generic linked list. This module is a sub-module
 	 	 	   for 'linkedList' implementation.
 ============================================================================
 */

#include <stdlib.h>

#include "linkedListNode.h"

typedef struct LinkedListNode {
	void* data;
	struct LinkedListNode* next;
} LinkedListNode;

/* API method - Description located at .h file */
LinkedListNode* newLinkedListNode(void* data) {
	//TODO: think if malloc failures need to be handled
	LinkedListNode* node = malloc(sizeof(LinkedListNode));

	node->data = data;

	return node;
}

/* API method - Description located at .h file */
inline struct LinkedListNode* getNext(const LinkedListNode* node) {
	return node->next;
}

/* API method - Description located at .h file */
inline void setNext(LinkedListNode* node, LinkedListNode* nextNode) {
	node->next = nextNode;
}

/* API method - Description located at .h file */
inline void* getData(const LinkedListNode* node) {
	return node->data;
}
