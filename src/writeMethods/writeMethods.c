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
 * @file writeMethods.c
 * @author Barak Sason Rofman
 * @brief This module provides different writing methods which can be used by the application to
 * write to a file in a certain manner.
 * Additional methods may be added to this file and passed to the logger in order to change the
 * way messages are written.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
 */

#include <string.h>
#include <pthread.h>

#include "writeMethods.h"
#include "../core/api/logger.h"

static pthread_mutex_t directWriteLock;

/* API method - Description located at .h file */
void initDirectWriteLock() {
	pthread_mutex_init(&directWriteLock, NULL);
}

static const char logLevelsIds[] = { ' ', /* NONE */
                                     'M', /* EMERGENCY */
                                     'A', /* ALERT */
                                     'C', /* CRITICAL */
                                     'E', /* ERROR */
                                     'W', /* WARNING */
                                     'N', /* NOTICE */
                                     'I', /* INFO */
                                     'D', /* DEBUG */
                                     'T', /* TRACE */};

static const char* logMethods[] = { "pb", "sb", "dw" };

/* API method - Description located at .h file */
void asciiWrite(const MessageData* md, FILE* logFile) {
	int maxMsgLen = getMaxMsgLen();
	int msgLen;
	char buf[maxMsgLen];

	msgLen =
	        snprintf(
	                buf,
	                maxMsgLen,
	                "[mid: %x:%.5x] [ll: %c] [lm: %s] [lwp: %.5ld] [loc: %s:%s:%d] [msg: %s]\n",
	                (unsigned int) md->tv.tv_sec, (unsigned int) md->tv.tv_usec,
	                logLevelsIds[md->logLevel], logMethods[md->logMethod],
	                md->tid, md->file, md->func, md->line, md->argsBuf);

	fwrite(buf, 1, msgLen, logFile);
}

/* API method - Description located at .h file */
void binaryWrite(const MessageData* md, FILE* logFile) {
	int fileNameLen;
	int methodNameLen;
	int argsBufLen;

	fileNameLen = strlen(md->file);
	methodNameLen = strlen(md->func);
	argsBufLen = strlen(md->argsBuf);

	/* Locking is required to ensure message consistency */
	pthread_mutex_lock(&directWriteLock); /* Lock */
	{
		fwrite(&md->tv, sizeof(md->tv), 1, logFile);
		fwrite(&logLevelsIds[md->logLevel], sizeof(logLevelsIds[0]), 1,
		       logFile);
		fwrite(logMethods[md->logMethod], 2, 1, logFile);
		fwrite(&md->tid, sizeof(md->tid), 1, logFile);
		fwrite(&fileNameLen, sizeof(fileNameLen), 1, logFile);
		fwrite(md->file, 1, fileNameLen, logFile);
		fwrite(&methodNameLen, sizeof(methodNameLen), 1, logFile);
		fwrite(md->func, 1, methodNameLen, logFile);
		fwrite(&md->line, 1, sizeof(md->line), logFile);
		fwrite(&argsBufLen, sizeof(argsBufLen), 1, logFile);
		fwrite(md->argsBuf, 1, argsBufLen, logFile);
	}
	pthread_mutex_unlock(&directWriteLock); /* Unlock */
}
