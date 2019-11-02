/*
 ============================================================================
 Name        : abstractList.h
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module provides a generic linked list
 implementation and is based on 'linkedListNode' module.
 ============================================================================
 */

#ifndef LinkedList_H
#define LinkedList_H

#include <stdbool.h>

#include "node/linkedListNode.h"

struct LinkedList;

/* Allocates new list and reset it's values */
struct LinkedList* newLinkedList(bool (*comparisonMethod)());

/* Add a new node 'node' to a ring buffer list 'll' */
void addNode(struct LinkedList* ll, struct LinkedListNode* node);

/* Removes the first node in the list and return it */
struct LinkedListNode* removeHead(struct LinkedList* ll);

/* Remove the first occurrence of a node containing 'data' from the list 'll',
 * if not found - return NULL */
struct LinkedListNode* removeNode(struct LinkedList* ll, const void* data);

/* Returns (without removing) the first node in a list (if the list is empty,
 * it'll return NULL) */
struct LinkedListNode* getHead(const struct LinkedList* ll);

#endif /* LinkedList_H */
