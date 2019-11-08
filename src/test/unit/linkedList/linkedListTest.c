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

static int createListAndCheckData();
static int addNodeToListAndGetHead();
static int removeHeadNodeOnlyOneNode();
static int removeHeadNodeMultipleNodes();
static int removeArbitraryNode();
static int removeHeadEmptyList();
static int removeLastNode();
static int removeHeadNonEmptyList();

/* API method - Description located at .h file */
int runListTests() {
	if ((UT_STATUS_FAILURE == createListAndCheckData())
	        || (UT_STATUS_FAILURE == addNodeToListAndGetHead())
	        || (UT_STATUS_FAILURE == removeHeadNodeOnlyOneNode())
	        || (UT_STATUS_FAILURE == removeHeadNodeMultipleNodes())
	        || (UT_STATUS_FAILURE == removeHeadNonEmptyList())
	        || (UT_STATUS_FAILURE == removeHeadEmptyList())
	        || (UT_STATUS_FAILURE == removeArbitraryNode())
	        || (UT_STATUS_FAILURE == removeLastNode())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

/* Create an empty list and verify that head and tail are NULL */
static int createListAndCheckData() {
	int res;
	struct LinkedList* ll = NULL;

	ll = newLinkedList();

	if (NULL != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (NULL != getTail(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(ll);

	return res;
}

/* Create a list, add a node with data, verify getHead() API */
static int addNodeToListAndGetHead() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);
	addNode(ll, node1);

	node2 = getHead(ll);

	if (node1 != node2) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node1);
	free(ll);

	return res;
}

/* Remove a node in the case that the matching node is head node */
static int removeHeadEmptyList() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node;

	ll = newLinkedList();

	node = removeHead(ll);

	if (NULL != node) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(ll);

	return res;
}

/* Remove a node in the case that the matching node is head node */
static int removeHeadNonEmptyList() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);

	addNode(ll, node1);
	node2 = removeHead(ll);

	if (node1 != node2) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (NULL != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (NULL != getTail(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node1);
	free(ll);

	return res;
}

/* Remove a node in the case that the matching node is head node and the list
 * contains only 1 node */
static int removeHeadNodeOnlyOneNode() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);
	addNode(ll, node1);

	node2 = removeNode(ll, node1);

	if (node1 != node2) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (NULL != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (NULL != getTail(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node1);
	free(ll);

	return res;
}

/* Remove a node in the case that the matching node is head node and the list
 * contains only multiple nodes */
static int removeHeadNodeMultipleNodes() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2 = NULL;
	struct LinkedListNode* node3;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);
	node2 = newLinkedListNode(NULL);
	addNode(ll, node1);
	addNode(ll, node2);

	node3 = removeNode(ll, node1);

	if (node1 != node3) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (node2 != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node2);
	free(node1);
	free(ll);

	return res;
}

/* Remove a node in the case that the matching node is an arbitrary node */
static int removeArbitraryNode() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2 = NULL;
	struct LinkedListNode* node3 = NULL;
	struct LinkedListNode* node4;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);
	node2 = newLinkedListNode(NULL);
	node3 = newLinkedListNode(NULL);
	addNode(ll, node1);
	addNode(ll, node2);
	addNode(ll, node3);

	node4 = removeNode(ll, node2);

	if (node2 != node4) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (node1 != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (node3 != getTail(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node3);
	free(node2);
	free(node1);
	free(ll);

	return res;
}

/* Remove a node in the case that the matching node is the last node */
static int removeLastNode() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2 = NULL;
	struct LinkedListNode* node3 = NULL;
	struct LinkedListNode* node4;

	ll = newLinkedList();
	node1 = newLinkedListNode(NULL);
	node2 = newLinkedListNode(NULL);
	node3 = newLinkedListNode(NULL);
	addNode(ll, node1);
	addNode(ll, node2);
	addNode(ll, node3);

	node4 = removeNode(ll, node3);

	if (node3 != node4) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (node1 != getHead(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	if (node2 != getTail(ll)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(node3);
	free(node2);
	free(node1);
	free(ll);

	return res;
}
