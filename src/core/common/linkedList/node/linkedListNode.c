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
 * @file linkedListNode.c
 * @author Barak Sason Rofman
 * @brief This module provides implementation for node operations in a generic linked list.
 * This module is a sub-module for 'linkedList' implementation.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include "linkedListNode.h"

#include <stdlib.h>


typedef struct LinkedListNode {
	/** LinkedListNode data */
	void* data;
	/** Pointer to next LinkedListNode in the list */
	struct LinkedListNode* next;
} LinkedListNode;

/* API method - Description located at .h file */
LinkedListNode* newLinkedListNode(void* data) {
	//TODO: think if malloc failures need to be handled
	LinkedListNode* node = malloc(sizeof(LinkedListNode));

	setNext(node, NULL);
	node->data = data;

	return node;
}

/* API method - Description located at .h file */
inline struct LinkedListNode* getNext(const LinkedListNode* node) {
	return node->next;
}

/* API method - Description located at .h file */
inline void setNext(LinkedListNode* node, LinkedListNode* nextNode) {
	node->next = nextNode;
}

/* API method - Description located at .h file */
inline void* getData(const LinkedListNode* node) {
	return node->data;
}
