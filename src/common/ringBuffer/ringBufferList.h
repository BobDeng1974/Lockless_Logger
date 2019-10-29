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

/* Allocate new node containing ring buffer 'rb' */
struct ringBufferListNode* newringBufferNode(struct ringBuffer* rb);

/* Add a new node 'node' to a ring buffer list 'rbl' */
void addRingBufferNode(struct ringBufferList* rbl, struct ringBufferListNode* node);

/* Removes the first node in the list and return it */
struct ringBufferListNode* removeHead(struct ringBufferList* rbl);

/* Remove the first occurrence of a node containing 'rb' from the list 'rbl', if not found - return NULL */
struct ringBufferListNode* removeNode(struct ringBufferList* rbl,
                                      const struct ringBuffer* rb);

/* Frees all nodes and ring buffers allocated to the list */
void deepDeleteRingBufferList(struct ringBufferList* rbl);

/* Frees all nodes (but not ring buffers) allocated to the list */
void shallowDeleteRingBufferList(struct ringBufferList* rbl);

/* Returns the first node in a list (if the list is empty, it'll return NULL) */
struct ringBufferListNode* getHead(const struct ringBufferList* rbl);

/* Returns the next node in a list (if the list is empty, it'll return NULL) */
struct ringBufferListNode* getNext(const struct ringBufferListNode* node);

/* Returns the 'ringBuffer' pointer of the node */
struct ringBuffer* getRingBuffer(const struct ringBufferListNode* node);

#endif /* DATASTRUCTURES */
