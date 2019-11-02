/*
 * nodeTest.c
 *
 *  Created on: Nov 1, 2019
 *      Author: root
 */

#include <stdlib.h>

#include "nodeTest.h"
#include "../../unit/unitTests.h"
#include "../../../core/common/ringBufferList/linkedList/node/linkedListNode.h"
#include "../../testsCommon.h"

static int createNodeAndCheckData();
static int createTwoNodeAndCheckData();

int runNodeTests() {
	if (UT_STATUS_FAILURE == createNodeAndCheckData()
	        || UT_STATUS_FAILURE == createTwoNodeAndCheckData()) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

static int createNodeAndCheckData() {
	int data;
	struct LinkedListNode* node;

	data = 42;

	node = newLinkedListNode(&data);

	if (NULL != getNext(node) || (data != *((int*) getData(node)))) {
		PRINT_FAILURE;
		return UT_STATUS_FAILURE;
	}

	return UT_STATUS_SUCCESS;
}

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

	return UT_STATUS_SUCCESS;
}
