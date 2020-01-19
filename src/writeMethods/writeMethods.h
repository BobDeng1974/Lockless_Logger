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
 * @file writeMethods.h
 * @author Barak Sason Rofman
 * @brief This module provides different writing methods which can be used by the application to
 * write to a file in a certain manner.
 * Additional methods may be added to this file and passed to the logger in order to change the
 * way messages are written.
 * @license Apache License, Version 2.0
 * @copywrite (C) [2019] [Barak Sason Rofman]
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

/**
 * Initialize a mutex that may be used in direct write mode to ensure message
 * consistency, if required
 */
void initDirectWriteLock();

/**
 * Destroys the direct write mutex
 */
void destroyDirectWriteLock();

#endif /* WRITEMETHODS_H */
