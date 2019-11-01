/*
 ============================================================================
 Name        : linkedListNode.h
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides implementation for node operations
 	 	 	   in a generic linked list. This module is a sub-module
 	 	 	   for 'linkedList' implementation.
 ============================================================================
 */

#ifndef LINKEDLISTNODE_H_
#define LINKEDLISTNODE_H_

struct LinkedListNode;

/* Allocate new node containing 'data' */
struct LinkedListNode* newLinkedListNode(void* data);

/* Returns the next node in a list (if the list is empty, it'll return NULL) */
struct LinkedListNode* getNext(const struct LinkedListNode* node);

/* Sets the next node of node 'node' to 'nextNode' */
void setNext(struct LinkedListNode* node, struct LinkedListNode* nextNode);

/* Returns the 'data' pointer of the node */
void* getData(const struct LinkedListNode* node);

#endif /* LINKEDLISTNODE_H_ */
