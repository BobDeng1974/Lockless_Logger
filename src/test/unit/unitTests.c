/*
 * unitTests.c
 *
 *  Created on: Nov 1, 2019
 *      Author: root
 */

#include "unitTests.h"

int runUnitTests() {
	if (UT_STATUS_SUCCESS != runNodeTests()) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}
