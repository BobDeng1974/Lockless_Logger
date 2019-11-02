/*
 ============================================================================
 Name        : nodeTest.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module contains tests for the node submodule
 ============================================================================
 */

#include <stdlib.h>

#include "nodeTest.h"
#include "../../unit/unitTests.h"
#include "../../../core/common/ringBufferList/linkedList/node/linkedListNode.h"
#include "../../testsCommon.h"

static int createNodeAndCheckData();
static int createTwoNodeAndCheckData();

/* API method - Description located at .h file */
int runNodeTests() {
	if ((UT_STATUS_FAILURE == createNodeAndCheckData())
	        || (UT_STATUS_FAILURE == createTwoNodeAndCheckData())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

/* Create a node with data and verify getNext() and getData() APIs on
 * that node */
static int createNodeAndCheckData() {
	int data;
	struct LinkedListNode* node;

	data = 42;

	node = newLinkedListNode(&data);

	if (NULL != getNext(node) || (data != *((int*) getData(node)))) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(node);

	return UT_STATUS_SUCCESS;
}

/* Create 2 nodes with data and verify getNext() and getData() APIs on
 * those nodes, in respect for each other */
static int createTwoNodeAndCheckData() {
	int data1;
	int data2;
	struct LinkedListNode* node1;
	struct LinkedListNode* node2;

	data1 = 42;
	data2 = -42;

	node1 = newLinkedListNode(&data1);
	node2 = newLinkedListNode(&data2);

	setNext(node1, node2);

	if (node2 != getNext(node1) || (data1 != *((int*) getData(node1)))
	        || (data2 != *((int*) (getData(getNext(node1)))))) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	free(node1);
	free(node2);

	return UT_STATUS_SUCCESS;
}
