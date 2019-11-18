/****************************************************************************
 * Copyright (C) [2019] [Barak Sason Rofman]								*
 *																			*
 * Licensed under the Apache License, Version 2.0 (the "License");			*
 * you may not use this file except in compliance with the License.			*
 * You may obtain a copy of the License at:									*
 *																			*
 * http://www.apache.org/licenses/LICENSE-2.0								*
 *																			*
 * Unless required by applicable law or agreed to in writing, software		*
 * distributed under the License is distributed on an "AS IS" BASIS,		*
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.	*
 * See the License for the specific language governing permissions and		*
 * limitations under the License.											*
 ****************************************************************************/

/**
 * @file ringBufferList.h
 * @author Barak Sason Rofman
 * @brief This module provides implementation of a ring buffer list.
 * This module is based on the submodules - 'linkedList' and 'ringBuffer'.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef RINGBUFFERLIST_H
#define RINGBUFFERLIST_H

#include "linkedList/linkedList.h"
#include "ringBuffer/ringBuffer.h"

/**
 * Allocates new list using a specific comparison method and reset it's values
 * @return The newly allocated LinkedList
 */
struct LinkedList* newRingBufferList();

/**
 * Returns the RingBuffer pointer from the LinkedListNode 'node'
 * @param node The LinkedListNode to retrieve the pointer from
 * @return A pointer to the RingBuffer struct of the LinkedListNode
 */
struct RingBuffer* getRingBuffer(struct LinkedListNode* node);

/**
 * Frees all LinkedListNodes and RingBuffers allocated to the list
 * @param ll The LinkedList to deallocate
 */
void deepDeleteRingBufferList(struct LinkedList* ll);

/**
 * Frees all LinkedListNodes (but not RingBuffers) allocated to the list
 * @param ll The LinkedList to deallocate
 */
void shallowDeleteRingBufferList(struct LinkedList* ll);

#endif /* RINGBUFFERLIST_H */
