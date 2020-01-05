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

/**
 * @file messageQueue.c
 * @author Barak Sason Rofman
 * @brief This module provides a MessageQueue implementation
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include "messageData.h"
#include "messageQueue.h"

typedef struct MessageQueue {
	/** Whether or not this ring buffer is being used by a thread */
	bool isInUse;
	/** The position which data was last read from */
	atomic_int lastRead;
	/** The position which data was last written to */
	atomic_int lastWrite;
	/** The size of the buffer */
	int size;
	/** Max length of additional arguments to log message */
	int maxArgsLen;
	/** Pointer to the internal buffer */
	MessageData* messagesData;
} MessageQueue;

static void initMessageQueue(MessageQueue* mq, const int size, const int maxArgsLen);

/* API method - Description located at .h file */
MessageQueue* newMessageInfo(const int size, const int maxArgsLen) {
	MessageQueue* mq;

	//TODO: think if malloc failures need to be handled
	mq = malloc(sizeof(*mq));
	if (NULL != mq) {
		initMessageQueue(mq, size, maxArgsLen);
	}

	return mq;
}

/**
 * Initializes MessageQueue
 * @param mq MessageQueue struct to initialize
 * @param size The of the queue
 * @param maxArgsLen Maximum length of message arguments
 */
static void initMessageQueue(MessageQueue* mq, const int size, const int maxArgsLen) {
	int i;

	mq->size = size;
	mq->maxArgsLen = maxArgsLen;

	//TODO: think if malloc failures need to be handled
	mq->messagesData = malloc(size * sizeof(*mq->messagesData));

	for (i = 0; i < size; ++i) {
		MessageData* md;

		md = &mq->messagesData[i];
		md->argsBuf = malloc(maxArgsLen);
	}

	mq->lastRead = 0;
	mq->lastWrite = 1; // Advance to 1, as an empty buffer is defined by having a difference of 1 between
	                   // lastWrite and lastRead
}

/* API method - Description located at .h file */
int addMessage(MessageQueue* mq, const int loggingLevel, char* file, const char* func,
               const int line, va_list* args, const char* msg, int logMethod) {
	int lastRead;
	int lastWrite;
	int nextLastWrite;

	/* Atomic load lastRead, as it's written by a different thread */
	__atomic_load(&mq->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = mq->lastWrite;
	nextLastWrite = (lastWrite + 1) >= mq->size ? 0 : (lastWrite + 1);

	if (nextLastWrite != lastRead) {
		setMsgValues(&mq->messagesData[lastWrite], loggingLevel, file, func, line, args, msg,
		             logMethod, mq->maxArgsLen);

		/* Atomic store lastWrite, as it's read by a different thread */
		__atomic_store_n(&mq->lastWrite, nextLastWrite, __ATOMIC_SEQ_CST);

		return MQ_STATUS_SUCCESS;
	}

	return MQ_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void drainMessages(MessageQueue* mq, FILE* logFile, const int maxMsgLen,
                   const int (*formatMethod)()) {
	int lastRead;
	int lastWrite;
	int nextLastRead;

	/* Atomic load lastWrite, as it's read by a different thread */
	__atomic_load(&mq->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);
	lastRead = mq->lastRead;
	nextLastRead = (lastRead + 1) >= mq->size ? 0 : (lastRead + 1);

	if (nextLastRead != lastWrite) {
		int prevNextLastRead;
		do {
			MessageData* md;
			int msgLen;
			char buf[maxMsgLen];

			md = &mq->messagesData[nextLastRead];
			msgLen = formatMethod(buf, md, maxMsgLen);
			fwrite(buf, 1, msgLen, logFile);

//			formatMethod(buf, md, maxMsgLen);

			prevNextLastRead = nextLastRead;
			nextLastRead = (nextLastRead + 1) >= mq->size ? 0 : (nextLastRead + 1);
		} while (nextLastRead != lastWrite);

		/* Atomic store lastRead, as it's read by a different thread */
		__atomic_store_n(&mq->lastRead, prevNextLastRead, __ATOMIC_SEQ_CST);
	}
}

/* API method - Description located at .h file */
void directWriteToFile(const int loggingLevel, char* file, const char* func, const int line,
                       va_list* args, char* msg, FILE* logFile, const int maxMsgLen,
                       const int maxArgsLen, const int logMethod, const int (*formatMethod)()) {
	MessageData md;
	int msgLen;
	char argsBuf[maxArgsLen];
	char buf[maxMsgLen];

	md.argsBuf = argsBuf;
	setMsgValues(&md, loggingLevel, file, func, line, args, msg, logMethod, maxArgsLen);

	msgLen = formatMethod(buf, &md, maxMsgLen);
	fwrite(buf, 1, msgLen, logFile);

//	formatMethod(buf, &md, maxMsgLen);
}

/* API method - Description located at .h file */
void messageDataQueueDestroy(MessageQueue* mq) {
	int i;

	for (i = 0; i < mq->size; ++i) {
		MessageData* md = (&mq->messagesData[i]);
		free(md->argsBuf);
	}

	free(mq->messagesData);
	free(mq);
}
