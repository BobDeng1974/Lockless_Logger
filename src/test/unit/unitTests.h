/*
 * unitTests.h
 *
 *  Created on: Nov 1, 2019
 *      Author: root
 */

#ifndef UNITTESTS_H_
#define UNITTESTS_H_

#include "node/nodeTest.h"

enum unitTestStatusCodes {
	UT_STATUS_FAILURE = -1, UT_STATUS_SUCCESS
};

int runUnitTests();

#endif /* UNITTESTS_H_ */
