/*
 ============================================================================
 Name        : ringBufferTest.c
 Author      : Barak Sason Rofman
 Copyright   : TODO: update
 Description : This module contains tests for the linked list submodule
 ============================================================================
 */

#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "ringBufferTest.h"
#include "../../unit/unitTests.h"
#include "../../../core/common/ringBufferList/ringBuffer/ringBuffer.h"
#include "../../testsCommon.h"

static int writeInFormat(char* buf, void* data);
static int writeSeqNoOverwrite();
static int writeWrapNoOverwriteCopySeq();
static int writeWrapNoOverwriteCopyWrap();
static int wrapOverwrite();
static int writeAfterWrapOverwrite();
static int sqeOverwrite();

/* API method - Description located at .h file */
int runRingBufferTests() {
	if ((UT_STATUS_FAILURE == writeSeqNoOverwrite())
	        || (UT_STATUS_FAILURE == writeWrapNoOverwriteCopySeq())
	        || (UT_STATUS_FAILURE == writeWrapNoOverwriteCopyWrap())
	        || (UT_STATUS_FAILURE == wrapOverwrite())
	        || (UT_STATUS_FAILURE == writeAfterWrapOverwrite())
	        || (UT_STATUS_FAILURE == sqeOverwrite())) {
		return UT_STATUS_FAILURE;
	}
	return UT_STATUS_SUCCESS;
}

static int writeInFormat(char* buf, void* data) {
	return sprintf(buf, "%s", (char*) data);
}

/* Write sequentially to file, without overwrite and read data written
 * to verify */
static int writeSeqNoOverwrite() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int res;

	fileName = "writeSewNoOverwrite.txt";
	msg = "This message is 30 chars long!";
	msgLen = strlen(msg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	drainBufferToFile(rb, testFile);

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		res = UT_STATUS_SUCCESS;
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}

/* Write sequentially and then in a wrap-around manner to file, without overwrite and
 * read data written to verify */
static int writeWrapNoOverwriteCopySeq() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* shortMsg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int shortMsgLen;
	int res;

	fileName = "writeWrapNoOverwriteCopySeq.txt";
	msg = "This message is 30 chars long!";
	shortMsg = "Short message";
	msgLen = strlen(msg);
	shortMsgLen = strlen(shortMsg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* Drain buffer to avoid potential overwrite */
	drainBufferToFile(rb, testFile);

	/* This write will be sequential (short message) */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, shortMsg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}
	drainBufferToFile(rb, testFile);

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		byetsRead = read(testFile, tmpBuf, shortMsgLen);

		if (0 == strncmp(shortMsg, tmpBuf, byetsRead)) {
			res = UT_STATUS_SUCCESS;
		} else {
			PRINT_FAILURE;
			res = UT_STATUS_FAILURE;
		}
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}

/* Write sequentially and then in a wrap-around manner to file, without overwrite and
 * read data written to verify */
static int writeWrapNoOverwriteCopyWrap() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int res;

	fileName = "writeWrapNoOverwriteCopyWrap.txt";
	msg = "This message is 30 chars long!";
	msgLen = strlen(msg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* Drain buffer to avoid potential overwrite */
	drainBufferToFile(rb, testFile);

	/* This write will be wrap-around */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}
	drainBufferToFile(rb, testFile);

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		byetsRead = read(testFile, tmpBuf, msgLen);

		if (0 == strncmp(msg, tmpBuf, byetsRead)) {
			res = UT_STATUS_SUCCESS;
		} else {
			PRINT_FAILURE;
			res = UT_STATUS_FAILURE;
		}
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}

/* Write to buffer sequentially and then in wrap-around manner (cause potential wrap-around
 * overwrite) and check handling, drain buffer to file and verify data */
static int wrapOverwrite() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int res;

	fileName = "wrapOverwrite.txt";
	msg = "This message is 30 chars long!";
	msgLen = strlen(msg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* This write will be wrap-around - buffer wasn't drained, potential wrap-around
	 * overwrite happens and write fails */
	if (RB_STATUS_FAILURE != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}
	drainBufferToFile(rb, testFile);

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		byetsRead = read(testFile, tmpBuf, msgLen);

		if (0 == strncmp(msg, tmpBuf, byetsRead)) {
			res = UT_STATUS_SUCCESS;
		} else {
			PRINT_FAILURE;
			res = UT_STATUS_FAILURE;
		}
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}

/* Write to buffer sequentially and then in wrap-around manner (cause potential wrap-around
 * overwrite) and check handling, drain buffer to file , perform another write and
 * verify data */
static int writeAfterWrapOverwrite() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int res;

	fileName = "writeAfterWrapOverwrite.txt";
	msg = "This message is 30 chars long!";
	msgLen = strlen(msg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* This write will be wrap-around - buffer wasn't drained, potential wrap-around
	 * overwrite happens and write fails */
	if (RB_STATUS_FAILURE != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* Drain buffer to avoid potential overwrite */
	drainBufferToFile(rb, testFile);

	/* This write will be wrap-around - verify writing success after previous failure
	 * and buffer drain */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		byetsRead = read(testFile, tmpBuf, msgLen);

		if (0 == strncmp(msg, tmpBuf, byetsRead)) {
			byetsRead = read(testFile, tmpBuf, msgLen);
			if (0 == strncmp(msg, tmpBuf, byetsRead)) {
				res = UT_STATUS_SUCCESS;
			} else {
				PRINT_FAILURE;
				res = UT_STATUS_FAILURE;
			}
		} else {
			PRINT_FAILURE;
			res = UT_STATUS_FAILURE;
		}
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}

/* Write to buffer sequentially and then in wrap-around manner (cause potential wrap-around
 * overwrite) and check handling, drain buffer to file , perform another write and then
 * another write (cause potential sequential overwrite) and verify data */
static int sqeOverwrite() {
	struct ringBuffer* rb = NULL;
	int buffSize;
	int testFile;
	char* fileName = NULL;
	int safetyLen;
	char* msg;
	char* tmpBuf = NULL;
	int byetsRead;
	int msgLen;
	int res;

	fileName = "sqeOverwrite.txt";
	msg = "This message is 30 chars long!";
	msgLen = strlen(msg);
	buffSize = 50;
	safetyLen = msgLen;
	tmpBuf = malloc(buffSize);

	remove(fileName);

	rb = newRingBuffer(buffSize, safetyLen);
	testFile = open(fileName, O_RDWR | O_CREAT);

	/* This write will be sequential */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* This write will be wrap-around - buffer wasn't drained, potential overwrite happens
	 * and write fails */
	if (RB_STATUS_FAILURE != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* Drain buffer to avoid potential overwrite */
	drainBufferToFile(rb, testFile);

	/* This write will be wrap-around - buffer wasn't drained, potential overwrite happens
	 * and write fails */
	if (RB_STATUS_SUCCESS != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	/* This write will be sequential - buffer wasn't drained, potential sequential
	 * overwrite happens and write fails */
	if (RB_STATUS_FAILURE != writeToRingBuffer(rb, msg, writeInFormat)) {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
		goto Exit;
	}

	close(testFile);
	testFile = open(fileName, O_RDWR);

	byetsRead = read(testFile, tmpBuf, msgLen);

	if (0 == strncmp(msg, tmpBuf, byetsRead)) {
		byetsRead = read(testFile, tmpBuf, msgLen);

		if (0 == strncmp(msg, tmpBuf, byetsRead)) {
			byetsRead = read(testFile, tmpBuf, msgLen);
			if (0 == strncmp(msg, tmpBuf, byetsRead)) {
				res = UT_STATUS_SUCCESS;
			} else {
				PRINT_FAILURE;
				res = UT_STATUS_FAILURE;
			}
		} else {
			PRINT_FAILURE;
			res = UT_STATUS_FAILURE;
		}
	} else {
		PRINT_FAILURE;
		res = UT_STATUS_FAILURE;
	}

	Exit: remove(fileName);
	free(tmpBuf);
	deleteRingBuffer(rb);

	return res;
}
