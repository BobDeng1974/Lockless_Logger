/*
 * writeMethods.h
 *
 *  Created on: Jan 7, 2020
 *      Author: root
 */

#ifndef WRITEMETHODS_H
#define WRITEMETHODS_H

#include "../core/logger/messageQueue/messageData.h"

/**
 * Writes a message in ascii format
 * @param md MessageData struct containing message info
 * @param logFile The file to write to
 */
void asciiWrite(const MessageData* md, FILE* logFile);

/**
 * Writes a message in binary format
 * @param md MessageData struct containing message info
 * @param logFile The file to write to
 */
void binaryWrite(const MessageData* md, FILE* logFile);

#endif /* WRITEMETHODS_H */
