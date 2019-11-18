/*
 ============================================================================
 Name        : ringBufferListTest.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module contains tests for the ring buffer list submodule
 ============================================================================
 */

#include "ringBufferListTest.h"

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "../../unit/unitTests.h"
#include "../../../core/common/ringBufferList/ringBufferList.h"
#include "../../testsCommon.h"

static int compareRingBufferListNodeData();
static int compareRingBufferListNodeBuffer();
static int deepDeleteRingBufferListTest();
static int shallowDeleteRingBufferListTest();

/* API method - Description located at .h file */
int runRingBufferListTests() {
	if ((UT_STATUS_FAILURE == compareRingBufferListNodeData())
	        || (UT_STATUS_FAILURE == compareRingBufferListNodeBuffer())
	        || (UT_STATUS_FAILURE == deepDeleteRingBufferListTest())
	        || (UT_STATUS_FAILURE == shallowDeleteRingBufferListTest())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

/* Add a node to a ring buffer list, remove it and compare to original */
static int compareRingBufferListNodeData() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2;
	int data1;

	ll = newRingBufferList();
	data1 = 42;
	node1 = newLinkedListNode(&data1);
	addNode(ll, node1);
	node2 = removeNode(ll, node1);

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

/* Add a ring buffer to a node, add the node to a list, retrieve ring
 * buffer and compare to original */
static int compareRingBufferListNodeBuffer() {
	int res;
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node = NULL;
	struct RingBuffer* rb = NULL;

	ll = newRingBufferList();
	rb = newRingBuffer(8, 4);
	node = newLinkedListNode(rb);
	addNode(ll, node);

	if (getRingBuffer(node) != rb) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	res = UT_STATUS_SUCCESS;

	Exit: free(rb);
	free(node);
	free(ll);

	return res;
}

/* Delete a ring buffer list including ring buffer elements */
static int deepDeleteRingBufferListTest() {
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2 = NULL;
	struct RingBuffer* rb1 = NULL;
	struct RingBuffer* rb2 = NULL;

	ll = newRingBufferList();
	rb1 = newRingBuffer(8, 4);
	rb2 = newRingBuffer(8, 4);
	node1 = newLinkedListNode(rb1);
	node2 = newLinkedListNode(rb2);
	addNode(ll, node1);
	addNode(ll, node2);

	deepDeleteRingBufferList(ll);

	return UT_STATUS_SUCCESS;
}

/* Delete a ring buffer list excluding ring buffer elements */
static int shallowDeleteRingBufferListTest() {
	struct LinkedList* ll = NULL;
	struct LinkedListNode* node1 = NULL;
	struct LinkedListNode* node2 = NULL;
	struct RingBuffer* rb1 = NULL;
	struct RingBuffer* rb2 = NULL;

	ll = newRingBufferList();
	rb1 = newRingBuffer(8, 4);
	rb2 = newRingBuffer(8, 4);
	node1 = newLinkedListNode(rb1);
	node2 = newLinkedListNode(rb2);
	addNode(ll, node1);
	addNode(ll, node2);

	shallowDeleteRingBufferList(ll);
	free(rb1);
	free(rb2);

	return UT_STATUS_SUCCESS;
}
