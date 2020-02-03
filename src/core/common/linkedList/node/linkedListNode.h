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
 * @file linkedListNode.h
 * @author Barak Sason Rofman
 * @brief This module provides implementation for node operations in a generic linked list.
 * This module is a sub-module for 'linkedList' implementation.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef LINKEDLISTNODE_H_
#define LINKEDLISTNODE_H_

struct LinkedListNode;

/**
 * Allocates new LinkedListNode containing 'data'
 * @param data LinkedListNode's data
 * @return A new LinkedListNode containing 'data'
 */
struct LinkedListNode* newLinkedListNode(void* data);

/**
 *	Get the next LinkedListNode pointed by the LinkedListNode 'node'
 * @param node A LinkedListNode in a linked list
 * @return The next LinkedListNode in a list (if the list is empty, NULL is returned)
 */
struct LinkedListNode* getNext(const struct LinkedListNode* node);

/**
 * Sets the next node of LinkedListNode 'node' to 'nextNode'
 * @param node The LinkedListNode to be changed
 * @param nextNode The LinkedListNode which 'node' will point to as the next LinkedListNode
 */
void setNext(struct LinkedListNode* node, struct LinkedListNode* nextNode);

/**
 * Get the data stored by the LinkedListNode 'node'
 * @param node A Node with some data
 * @return The data stored by the LinkedListNode 'node'
 */
void* getData(const struct LinkedListNode* node);

#endif /* LINKEDLISTNODE_H_ */
