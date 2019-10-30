/*
 ============================================================================
 Name        : loggerTest.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdarg.h>

#include "../api/logger.h"

#define ITERATIONS 10000
#define NUM_THRDS 500
#define BUF_SIZE 75

#define BUFFSIZE 1000000
#define SHAREDBUFFSIZE 10000000

char chars[] = "0123456789abcdefghijklmnopqrstuvwqxy";
char** data;

void createRandomData(char** data, int charsLen);
void* threadMethod();

int main(void) {
	pthread_t threads[NUM_THRDS];
	int i;
	int res;
	int charsLen;
	struct timeval tv1, tv2;

	gettimeofday(&tv1, NULL);

	if (access("logFile.txt", F_OK) != -1) {
		remove("logFile.txt");
	}

	charsLen = strlen(chars);

	res = initLogger(1, BUFFSIZE, SHAREDBUFFSIZE, LOG_LEVEL_TRACE);
	if (LOG_STATUS_SUCCESS == res) {
		data = malloc(NUM_THRDS * sizeof(char*));
		createRandomData(data, charsLen);

		for (i = 0; i < NUM_THRDS; ++i) {
			pthread_create(&threads[i], NULL, threadMethod, data[i]);
		}

		for (i = 0; i < NUM_THRDS; ++i) {
			pthread_join(threads[i], NULL);
		}

		terminateLogger();

		printf("Direct writes = %llu\n", cnt);
		gettimeofday(&tv2, NULL);
		printf("Total time = %f seconds\n",
		       (double) (tv2.tv_usec - tv1.tv_usec) / 1000000
		               + (double) (tv2.tv_sec - tv1.tv_sec));
	}

	return res;
}

void createRandomData(char** data, int charsLen) {
	int i;
	for (i = 0; i < NUM_THRDS; ++i) {
		int j;
		data[i] = malloc(BUF_SIZE);
		for (j = 0; j < BUF_SIZE; ++j) {
			data[i][j] = chars[rand() % charsLen];
		}
	}
}

void* threadMethod(void* data) {
	char* logData = data;

	registerThread();

	for (int i = 0; i < ITERATIONS; ++i) {
		LOG_MSG(LOG_LEVEL_EMERG, "A message with arguments: %s", logData);
		unregisterThread();
	}

	unregisterThread();

	return NULL;
}

