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

/* Allocates new list using a specific comparison method and reset it's values */
struct LinkedList* newRingBufferList();

/* Returns the ringBuffer pointer from the node 'node' */
struct ringBuffer* getRingBuffer(struct LinkedListNode* node);

/* Frees all nodes and ring buffers allocated to the list */
void deepDeleteRingBufferList(struct LinkedList* al);

/* Frees all nodes (but not ring buffers) allocated to the list */
void shallowDeleteRingBufferList(struct LinkedList* al);

#endif /* RINGBUFFERLIST_H */
