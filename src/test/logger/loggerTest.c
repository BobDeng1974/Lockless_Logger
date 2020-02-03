/****************************************************************************
 * Copyright (C) [2019] [Barak Sason Rofman]								*
 *																			*
 * Licensed under the Apache License, Version 2.0 (the "License");			*
 * you may not use this file except in compliance with the License.			*
 * You may obtain a copy of the License at:									*
 *																			*
 * http://www.apache.org/licenses/LICENSE-2.0								*
 *																			*
 * Unless required by applicable law or agreed to in writing, software		*
 * distributed under the License is distributed on an "AS IS" BASIS,		*
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.	*
 * See the License for the specific language governing permissions and		*
 * limitations under the License.											*
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdarg.h>
#include <errno.h>

#include "../../core/api/logger.h"
#include "../../writeMethods/writeMethods.h"

#define ITERATIONS 25000
#define NUM_THRDS 200
#define BUF_SIZE 78

#define MAX_MSG_LEN 512
#define ARGS_BUF_SIZE 128

#define BUFFSIZE 25000
#define SHAREDBUFFSIZE 10000

char chars[] = "0123456789abcdefghijklmnopqrstuvwqxy";
char** data;

static void createRandomData(char** data, int charsLen);
static void* threadMethod();

int main(void) {
	remove("logFile.txt");

	if (LOG_STATUS_SUCCESS
	        == initLogger(10, BUFFSIZE, SHAREDBUFFSIZE, LOG_LEVEL_TRACE,
	        MAX_MSG_LEN,
	                      ARGS_BUF_SIZE, true, asciiWrite)) {
		int i;
		int charsLen;
		pthread_t threads[NUM_THRDS];
		struct timeval tv1, tv2;

		charsLen = strlen(chars);
		data = malloc(NUM_THRDS * sizeof(*data));
		createRandomData(data, charsLen);

		gettimeofday(&tv1, NULL);

		for (i = 0; i < NUM_THRDS; ++i) {
			pthread_create(&threads[i], NULL, threadMethod, data[i]);
		}

//			sleep(1);
//			setLoggingLevel(LOG_LEVEL_NONE);

		for (i = 0; i < NUM_THRDS; ++i) {
			pthread_join(threads[i], NULL);
		}

		terminateLogger();
		free(data);
		gettimeofday(&tv2, NULL);

		printf("Direct writes = %llu\n", cnt);
		printf("Total time = %f seconds\n",
		       (double) (tv2.tv_usec - tv1.tv_usec) / 1000000
		               + (double) (tv2.tv_sec - tv1.tv_sec));

		return LOG_STATUS_SUCCESS;
	}

	return LOG_STATUS_FAILURE;
}

static void createRandomData(char** data, int charsLen) {
	int i;

	for (i = 0; i < NUM_THRDS; ++i) {
		int j;

		data[i] = malloc(BUF_SIZE);
		for (j = 0; j < BUF_SIZE; ++j) {
			data[i][j] = chars[rand() % charsLen];
		}
	}
}

static void* threadMethod(void* data) {

	for (int i = 0; i < ITERATIONS; ++i) {
		registerThread();
		LOG_MSG(LOG_LEVEL_EMERG, "A message with arguments: %s", (char* )data);
		unregisterThread();
	}

	return NULL;
}
