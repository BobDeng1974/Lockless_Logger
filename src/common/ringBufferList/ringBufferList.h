/*
 ============================================================================
 Name        : ringBufferList.h
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#ifndef RINGBUFFERLIST_H
#define RINGBUFFERLIST_H

#include <stdbool.h>

#include "linkedList/linkedList.h"

/* Returns true if the the node 'node' contains data 'data' or false otherwise */
bool isContains(const struct LinkedListNode* node, const void* data);

/* Returns the ringBuffer pointer from the node 'node' */
struct ringBuffer* getRingBuffer(struct LinkedListNode* node);

/* Frees all nodes and ring buffers allocated to the list */
void deepDeleteRingBufferList(struct LinkedList* al);

/* Frees all nodes (but not ring buffers) allocated to the list */
void shallowDeleteRingBufferList(struct LinkedList* al);

#endif /* RINGBUFFERLIST_H */
