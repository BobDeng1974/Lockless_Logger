/*
 * writeMethods.c
 *
 *  Created on: Jan 7, 2020
 *      Author: root
 */

#include <string.h>

#include "writeMethods.h"
#include "../core/api/logger.h"

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

	msgLen = snprintf(buf, maxMsgLen,
	                  "[mid: %x:%.5x] [ll: %c] [lm: %s] [lwp: %.5ld] [loc: %s:%s:%d] [msg: %s]\n",
	                  (unsigned int) md->tv.tv_sec, (unsigned int) md->tv.tv_usec,
	                  logLevelsIds[md->logLevel], logMethods[md->logMethod], md->tid, md->file,
	                  md->func, md->line, md->argsBuf);

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

	fwrite(&md->tv, sizeof(md->tv), 1, logFile);
	fwrite(&logLevelsIds[md->logLevel], sizeof(logLevelsIds[0]), 1, logFile);
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
