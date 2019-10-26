/*
 ============================================================================
 Name        : ringBuffer.c
 Author      : Barak Sason Rofman
 Version     : TODO: update
 Copyright   : TODO: update
 Description : TODO: update
 ============================================================================
 */

#include <unistd.h>
#include <string.h>

#include "ringBuffer.h"

//TODO: add additional status codes to describe different errors that occur
enum statusCodes {
	STATUS_FAILURE = -1, STATUS_SUCCESS
};

/* Inlining small methods */
static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen);
static inline bool isWraparoundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd);

static int writeSeq(char* buf, const int lastWrite, void* data,
                    int (*formatMethod)());
static int writeWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                     const int safetyLen, void* data, int (*formatMethod)());

/* API method - Description located at .h file */
bool isNextWriteOverwrite(const int lastRead, const atomic_int lastWrite,
                          const int lenToBufEnd, const int safetyLen) {
	return (isSequentialOverwrite(lastRead, lastWrite, safetyLen)
	        || isWraparoundOverwrite(lastRead, safetyLen, lenToBufEnd));
}

/* Check for sequential data override */
static inline bool isSequentialOverwrite(const int lastRead,
                                         const int lastWrite, const int msgLen) {
	return (lastWrite < lastRead && ((lastWrite + msgLen) >= lastRead));
}

/* Check for wrap-around data override */
static inline bool isWraparoundOverwrite(const int lastRead, const int msgLen,
                                         const int lenToBufEnd) {
	if (msgLen > lenToBufEnd) {
		int bytesRemaining = msgLen - lenToBufEnd;
		return bytesRemaining >= lastRead;
	}

	return false;
}

/* API method - Description located at .h file */
int writeSeqOrWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                   const int safetyLen, void* data, int (*formatMethod)()) {
	int newLastWrite;

	newLastWrite =
	        lenToBufEnd >= safetyLen ?
	                writeSeq(buf, lastWrite, data, formatMethod) :
	                writeWrap(buf, lastWrite, lenToBufEnd, safetyLen, data,
	                          formatMethod);

	return newLastWrite;
}

/* Write sequentially to a designated buffer */
static int writeSeq(char* buf, const int lastWrite, void* data,
                    int (*formatMethod)()) {
	int msgLen;

	msgLen = formatMethod(buf + lastWrite, data);

	return lastWrite + msgLen;
}

/* Space at the end of the buffer is insufficient - write what is possible to
 * the end of the buffer, the rest write at the beginning of the buffer */
static int writeWrap(char* buf, const int lastWrite, const int lenToBufEnd,
                     const int safetyLen, void* data, int (*formatMethod)()) {
	int msgLen;
	char locBuf[safetyLen];
	/* local buffer is used as in the case of wrap-around write, writing is split to 2
	 * portions - write whatever possible at the end of the buffer and the rest write in
	 * the beginning. Since we don't know in advance the real length of the message we can't
	 * assume there is enough space at the end, therefore a temporary, long enough buffer is
	 * required into which data will be written sequentially and then copied back to the
	 * original buffer, either in sequential of wrap-around manner */

	msgLen = formatMethod(locBuf, data);

	if (lenToBufEnd >= msgLen) {
		/* Space at the end of the buffer is sufficient - copy everything at once */
		memcpy(buf + lastWrite, locBuf, msgLen);
		return lastWrite + msgLen;
	} else {
		/* Space at the end of the buffer is insufficient - copy
		 * data written in the end and then data written back at the beginning */
		int bytesRemaining = msgLen - lenToBufEnd;
		memcpy(buf + lastWrite, locBuf, lenToBufEnd);
		memcpy(buf, locBuf + lenToBufEnd, bytesRemaining);
		return bytesRemaining;
	}

}

/* API method - Description located at .h file */
int drainBufferToFile(const int file, const char* buf, const int lastRead,
                      const int lastWrite, const int bufSize) {
	int dataLen;
	int lenToBufEnd;

	if (lastWrite > lastRead) {
		dataLen = lastWrite - lastRead - 1;

		if (dataLen > 0) {
			write(file, buf + lastRead + 1, dataLen);

			return lastWrite - 1;
		}
	} else {
		lenToBufEnd = bufSize - lastRead - 1;
		dataLen = lenToBufEnd + lastWrite - 1;

		if (dataLen > 0) {
			write(file, buf + lastRead + 1, lenToBufEnd);
			write(file, buf, lastWrite);

			return lastWrite - 1;
		}
	}

	return STATUS_FAILURE;
}
