/*
 ============================================================================
 Name        : LinkedList.h
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#ifndef LinkedList_H
#define LinkedList_H

#include <stdbool.h>

#include "node/linkedListNode.h"

struct LinkedList;

/* Allocates new list and reset it's values */
struct LinkedList* newLinkedList(bool (*comparisonMethod)());

/* Add a new node 'node' to a ring buffer list 'rbl' */
void addNode(struct LinkedList* ll, struct LinkedListNode* node);

/* Removes the first node in the list and return it */
struct LinkedListNode* removeHead(struct LinkedList* ll);

/* Remove the first occurrence of a node containing 'data' from the list 'al', if not found - return NULL */
struct LinkedListNode* removeNode(struct LinkedList* ll, const void* data);

/* Returns the first node in a list (if the list is empty, it'll return NULL) */
struct LinkedListNode* getHead(const struct LinkedList* ll);

#endif /* LinkedList_H */
