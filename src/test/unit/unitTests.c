/*
 ============================================================================
 Name        : unitTests.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module run all unit tests of all submodules
 ============================================================================
 */

#include "unitTests.h"
#include "node/nodeTest.h"
#include "linkedList/linkedListTest.h"
#include "ringBuffer/ringBufferTest.h"
#include "ringBufferList/ringBufferListTest.h"

/* API method - Description located at .h file */
int runUnitTests() {
	if ((UT_STATUS_SUCCESS != runNodeTests())
	        || (UT_STATUS_SUCCESS != runListTests())
	        || (UT_STATUS_SUCCESS != runRingBufferTests())
	        || (UT_STATUS_SUCCESS != runRingBufferListTests())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}
