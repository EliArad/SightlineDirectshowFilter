/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#pragma once

#include "sltypes.h"
#include "slfip.h"

void ClosePort(void * handle);

/*! Opens a socket and returns a handle
 */
void * OpenWritePort(const char * ipaddress, int portNumber = SLFIP_TO_BOARD_PORT);
void * OpenListenPort(int portNumber = SLFIP_FROM_BOARD_PORT);

/*! Send bytes
 * @param buffer data to be sent
 * @param dataLen number of bytes to send
 * @return Number of bytes sent
 */
int WriteFIP(void * handle, SLPacketType buffer, s32 dataLen);

/*! Returns a complete SightLine command packets
 * Packet = Header + Length + Type + Data + Checksum
 * @param buffer place where received data is written
 * @param dataLen maximum number of bytes to expect
 * @return number of bytes read, -1 on error
 */
int ReadFIP(void * handle, SLPacketType buffer, u32 dataLen, s32 timeout = -1);
