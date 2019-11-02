/*
 ============================================================================
 Name        : linkedListTest.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module contains tests for the linked list submodule
 ============================================================================
 */

#include <stdlib.h>
#include <stdbool.h>

#include "linkedListTest.h"
#include "../../unit/unitTests.h"
#include "../../../core/common/ringBufferList/linkedList/linkedList.h"
#include "../../testsCommon.h"

inline static bool compareMethod(const struct LinkedListNode* node,
                                 const void* data);
static int createListAndCheckData();
static int addNodeToListAndCheckData();
static int removeHeadMatchindNode();
static int removeArbitraryMatchindNode();

/* API method - Description located at .h file */
int runListTests() {
	if ((UT_STATUS_FAILURE == createListAndCheckData())
	        || (UT_STATUS_FAILURE == addNodeToListAndCheckData())
	        || (UT_STATUS_FAILURE == removeHeadMatchindNode())
	        || (UT_STATUS_FAILURE == removeArbitraryMatchindNode())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

/* Create an empty list and verify that head is NULL */
static int createListAndCheckData() {
	struct LinkedList* ll;

	ll = newLinkedList(compareMethod);

	if (NULL != getHead(ll)) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(ll);

	return UT_STATUS_SUCCESS;
}

/* Create a list, add a node with data, verify getHead() API,
 * verify removeHead() API */
static int addNodeToListAndCheckData() {
	struct LinkedList* ll;
	struct LinkedListNode* node1;
	struct LinkedListNode* node2;

	ll = newLinkedList(compareMethod);

	node1 = newLinkedListNode(NULL);

	addNode(ll, node1);

	node2 = getHead(ll);

	if (node1 != node2) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	node2 = removeHead(ll);
	if ((node1 != node2) || NULL != getHead(ll)) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(node1);
	free(ll);

	return UT_STATUS_SUCCESS;
}

/* Create a list, add a node with data, verify removeNode() API in the case
 * that the matching node is head node */
static int removeHeadMatchindNode() {
	struct LinkedList* ll;
	struct LinkedListNode* node1;
	struct LinkedListNode* node2;
	int data1;

	ll = newLinkedList(compareMethod);
	data1 = 42;

	node1 = newLinkedListNode(&data1);

	addNode(ll, node1);

	node2 = removeNode(ll, &data1);

	if (node1 != node2) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(node1);
	free(ll);

	return UT_STATUS_SUCCESS;
}

/* Create a list, add a node with data, verify removeNode() API in the case
 * that the matching node is an arbitrary node */
static int removeArbitraryMatchindNode() {
	struct LinkedList* ll;
	struct LinkedListNode* node1;
	struct LinkedListNode* node2;
	struct LinkedListNode* node3;
	int data1;
	int data2;

	ll = newLinkedList(compareMethod);

	data1 = 42;
	node1 = newLinkedListNode(&data1);

	data2 = -42;
	node2 = newLinkedListNode(&data2);

	addNode(ll, node1);
	addNode(ll, node2);

	node3 = removeNode(ll, &data2);

	if (node2 != node3) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(node2);
	free(node1);
	free(ll);

	return UT_STATUS_SUCCESS;
}

/* Returns true if the the node 'node' contains data 'data' or false otherwise */
inline static bool compareMethod(const struct LinkedListNode* node,
                                 const void* data) {
	return (getData(node) == data) ? true : false;
}
