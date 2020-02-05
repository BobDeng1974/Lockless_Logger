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
#include <stdatomic.h>

#include "messageData.h"
#include "messageQueue.h"

typedef struct MessageQueue {
	/** The position which data was last read from */
	atomic_int lastRead;
	/** The position which data was last written to */
	atomic_int lastWrite;
	/** The size of the buffer */
	int size;
	/** Whether this buffer was dynamically allocated */
	bool isDynamicallyAllocated;
	/** Whether this buffer has been taken by a worker thread */
	atomic_bool isTaken;
	/** Whether this buffer should be freed */
	atomic_bool isDecomossioned;
	/** Whether this buffer is currently being used by a worker thread */
	atomic_bool isBeingUsed;
	/** Pointer to the internal buffer */
	MessageData* messagesData;
} MessageQueue;

static void initMessageQueue(MessageQueue* mq, const int size,
                             const int maxArgsLen,
                             const bool isDynamicallyAllocated);
static inline int getNextPos(int curPos, const int queueSize);
static void prepareMessageQueue(MessageQueue* mq, const int size,
                                const int maxArgsLen);

/* API method - Description located at .h file */
MessageQueue* newMessageQueue(const int size, const int maxArgsLen,
                              const bool isDynamicallyAllocated) {
	MessageQueue* mq;

	//TODO: think if malloc failures need to be handled
	mq = malloc(sizeof(*mq));
	if (NULL != mq) {
		initMessageQueue(mq, size, maxArgsLen, isDynamicallyAllocated);
	}

	return mq;
}

/**
 * Initializes MessageQueue
 * @param mq MessageQueue struct to initialize
 * @param size The of the queue
 * @param maxArgsLen Maximum length of message arguments
 * @param isDynamicallyAllocated Whether or not to enable dynamic buffers allocation
 */
static void initMessageQueue(MessageQueue* mq, const int size,
                             const int maxArgsLen,
                             const bool isDynamicallyAllocated) {
	mq->isDynamicallyAllocated = isDynamicallyAllocated;
	__atomic_store_n(&mq->isDecomossioned, false, __ATOMIC_SEQ_CST);
	__atomic_store_n(&mq->isBeingUsed, false, __ATOMIC_SEQ_CST);
	prepareMessageQueue(mq, size, maxArgsLen);
}

/**
 * Prepare MessageQueue dynamic parameters
 * @param mq The MessageQueue to prepare
 * @param size The size of the MessageData internal buffer
 * @param maxArgsLen Maximum length of message arguments
 */
static void prepareMessageQueue(MessageQueue* mq, const int size,
                                const int maxArgsLen) {
	int i;

	mq->size = size;

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
int addMessage(MessageQueue* mq, const int loggingLevel, char* file,
               const char* func, const int line, va_list* args, const char* msg,
               const int logMethod, const int maxArgsLen) {
	int lastRead;
	int lastWrite;
	int nextLastWrite;

	/* Atomic load lastRead, as it's written by a different thread */
	__atomic_load(&mq->lastRead, &lastRead, __ATOMIC_SEQ_CST);
	lastWrite = mq->lastWrite;
	nextLastWrite = getNextPos(lastWrite, mq->size);

	if (nextLastWrite != lastRead) {
		setMsgValues(&mq->messagesData[lastWrite], loggingLevel, file, func,
		             line, args, msg, logMethod, maxArgsLen);

		/* Atomic store lastWrite, as it's read by a different thread */
		__atomic_store_n(&mq->lastWrite, nextLastWrite, __ATOMIC_SEQ_CST);

		return MQ_STATUS_SUCCESS;
	}

	return MQ_STATUS_FAILURE;
}

/* API method - Description located at .h file */
void drainMessages(MessageQueue* mq, FILE* logFile, const int maxMsgLen,
                   const void (*writeMethod)()) {
	int lastRead;
	int lastWrite;
	int nextLastRead;

	/* Atomic load lastWrite, as it's read by a different thread */
	__atomic_load(&mq->lastWrite, &lastWrite, __ATOMIC_SEQ_CST);
	lastRead = mq->lastRead;
	nextLastRead = getNextPos(lastRead, mq->size);

	if (nextLastRead != lastWrite) {
		int prevNextLastRead;
		do {
			MessageData* md;

			md = &mq->messagesData[nextLastRead];
			writeMethod(md, logFile);

			prevNextLastRead = nextLastRead;
			nextLastRead =
			        (nextLastRead + 1) >= mq->size ? 0 : (nextLastRead + 1);
		} while (nextLastRead != lastWrite);

		/* Atomic store lastRead, as it's read by a different thread */
		__atomic_store_n(&mq->lastRead, prevNextLastRead, __ATOMIC_SEQ_CST);
	}
}

/* API method - Description located at .h file */
void directWriteToFile(const int loggingLevel, char* file, const char* func,
                       const int line, va_list* args, char* msg, FILE* logFile,
                       const int maxMsgLen, const int maxArgsLen,
                       const int logMethod, const void (*writeMethod)()) {
	MessageData md;
	char argsBuf[maxArgsLen];

	md.argsBuf = argsBuf;
	setMsgValues(&md, loggingLevel, file, func, line, args, msg, logMethod,
	             maxArgsLen);

	writeMethod(&md, logFile);
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

/**
 * Return the next position in a queue
 * @param curPos Current position
 * @param queueSize Queue size
 * @return The next position in a queue
 */
static inline int getNextPos(int curPos, const int queueSize) {
	return ++curPos >= queueSize ? 0 : curPos;
}

/* API method - Description located at .h file */
void inline setIsDynamicallyAllocated(MessageQueue* mq, bool state) {
	__atomic_store_n(&mq->isDynamicallyAllocated, state, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
bool inline getIsDynamicallyAllocated(MessageQueue* mq) {
	return mq->isDynamicallyAllocated;
}

/* API method - Description located at .h file */
void inline decommisionBuffer(MessageQueue* mq) {
	__atomic_store_n(&mq->isDecomossioned, true, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
bool inline isDecommisionedBuffer(MessageQueue* mq) {
	bool isDecomossionedLoc;

	__atomic_load(&mq->isDecomossioned, &isDecomossionedLoc, __ATOMIC_SEQ_CST);

	return isDecomossionedLoc;
}

/* API method - Description located at .h file */
void inline setIsBeingUsed(MessageQueue* mq, bool state) {
	__atomic_store_n(&mq->isBeingUsed, state, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
bool inline getIsPrivateBufferBeingUsed(MessageQueue* mq) {
	bool isBeingUsed;

	__atomic_load(&mq->isBeingUsed, &isBeingUsed, __ATOMIC_SEQ_CST);

	return isBeingUsed;
}

/* API method - Description located at .h file */
void changeBufferSize(MessageQueue* mq, const int newSize, const int maxArgsLen) {
	int i;

	for (i = 0; i < mq->size; ++i) {
		MessageData* md = (&mq->messagesData[i]);
		free(md->argsBuf);
	}

	free(mq->messagesData);
	prepareMessageQueue(mq, newSize, maxArgsLen);

}

/* API method - Description located at .h file */
void inline setIsTaken(MessageQueue* mq, bool state) {
	__atomic_store_n(&mq->isTaken, state, __ATOMIC_SEQ_CST);
}

/* API method - Description located at .h file */
bool inline getIsPrivateBufferTaken(MessageQueue* mq) {
	bool isTaken;

	__atomic_load(&mq->isTaken, &isTaken, __ATOMIC_SEQ_CST);

	return isTaken;
}
