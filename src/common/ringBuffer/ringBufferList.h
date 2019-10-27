/*
 ============================================================================
 Name        : ringBufferList.h
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#ifndef DATASTRUCTURES
#define DATASTRUCTURES

#include <stdbool.h>

#include "ringBuffer.h"

struct ringBufferListNode;
struct ringBufferList;

/* Allocate new list and reset it's values */
struct ringBufferList* newRingBufferList();

/* Add a new node which contains the relevant data to the specified list */
void addNode(struct ringBufferList* rbl, struct ringBuffer* data);

/* Remove the first node in the list and return it */
struct ringBufferListNode* removeHead(struct ringBufferList* rbl);

/* Remove from the list the first occurrence of a node containing 'data', if not found - return NULL */
struct ringBuffer* removeNode(struct ringBufferList* rbl,
                              const struct ringBuffer* data);

/* Frees all resources allocated to the list */
void freeRingBufferList(struct ringBufferList* rbl);

/* Returns the first node in a list (if the list is empty, it'll return NULL) */
struct ringBufferListNode* getHead(const struct ringBufferList* rbl);

/* Returns the next node in a list (if the list is empty, it'll return NULL) */
struct ringBufferListNode* getNext(const struct ringBufferListNode* node);

/* Returns the 'ringBuffer' pointer of the node */
struct ringBuffer* getRingBuffer(const struct ringBufferListNode* node);

#endif /* DATASTRUCTURES */
