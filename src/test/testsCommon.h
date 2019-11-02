/*
 * testsCommon.h
 *
 *  Created on: Nov 1, 2019
 *      Author: root
 */

#ifndef TESTSCOMMON_H_
#define TESTSCOMMON_H_

#include <stdio.h>

#define PRINT_FAILURE fprintf(stderr, "Failure at %s:%s:%d\n", __FILE__,__PRETTY_FUNCTION__,  __LINE__ )

#endif /* TESTSCOMMON_H_ */
