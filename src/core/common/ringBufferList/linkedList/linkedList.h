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
 * @file linkedList.h
 * @author Barak Sason Rofman
 * @brief This module provides a generic linked list implementation and is based on
 * 'linkedListNode' module.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#ifndef LinkedList_H
#define LinkedList_H

#include <stdbool.h>

#include "node/linkedListNode.h"

struct LinkedList;

/**
 * Allocates new LinkedList and reset it's values
 * @return A newly allocated LinkedList
 */
struct LinkedList* newLinkedList();

/**
 * Add a new LinkedListNode 'node' to the LinkedList 'll'
 * @param ll The LinkedList to which add the LinkedListNode 'node'
 * @param node The LinkedListNode to add
 */
void addNode(struct LinkedList* ll, struct LinkedListNode* node);

/**
 * Removes the first LinkedListNode in the LinkedList 'll' and returns it
 * @param ll The LinkedList of which to remove the head
 * @return The LinkedListNode that was previously the head of the LinkedList 'll'
 */
struct LinkedListNode* removeHead(struct LinkedList* ll);

/**
 * Removes the first occurrence of a LinkedListNode 'nodeToRemove' from the LinkedList 'll'.
 * if not match is found, returns NULL
 * @param ll The LinkedList in which to search for LinkedListNode 'nodeToRemove'
 * @param nodeToRemove The LinkedListNode to search and remove
 * @return The first occurrence of a LinkedListNode 'nodeToRemove' from the LinkedList 'll' or NULL
 * in case no match was found
 */
struct LinkedListNode* removeNode(struct LinkedList* ll, struct LinkedListNode* nodeToRemove);

/**
 * Returns (without removing) the first node in a list (if the list is empty,
 * NULL is returned)
 * @param ll The LinkedList from which to retrieve the head LinkedListNode
 * @return The head LinkedListNode of the LinkedList 'll', without removing it
 */
struct LinkedListNode* getHead(const struct LinkedList* ll);

/**
 * Returns (without removing) the last node in a list (if the list is empty,
 * NULL is returned)
 * @param ll The LinkedList from which to retrieve the tail LinkedListNode
 * @return The tail LinkedListNode of the LinkedList 'll', without removing it
 */
struct LinkedListNode* getTail(const struct LinkedList* ll);

#endif /* LinkedList_H */
