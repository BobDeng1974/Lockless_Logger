/*
 ============================================================================
 Name        : unitTests.h
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module run all unit tests of all submodules
 ============================================================================
 */

#ifndef UNITTESTS_H_
#define UNITTESTS_H_

#include "node/nodeTest.h"
#include "linkedList/linkedListTest.h"

enum unitTestStatusCodes {
	UT_STATUS_FAILURE = -1, UT_STATUS_SUCCESS
};

/* Run all unit tests */
int runUnitTests();

#endif /* UNITTESTS_H_ */
