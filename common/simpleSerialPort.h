/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#pragma once

#include "sltypes.h"
#include "slfip.h"

//Opens a COM port and returns a handle  e.g: COM4, 57600, 8, 1, NONE
void * OpenSerialPort(const char * portNumberStr);

// closes the open port:
void CloseSerialPort(void * handle);

// Send bytes
//param buffer data to be sent
//param dataLen number of bytes to send
//return Number of bytes sent, -1 on error
int WriteFIP(void * handle, SLPacketType buffer, s32 dataLen);

// Returns a complete SightLine command packets
//acket = Header + Length + Type + Data + Checksum
//param buffer place where received data is written
//param dataLen maximum number of bytes to expect
//return number of bytes read, -1 on error
s32 ReadFIP(void * handle, SLPacketType data, u32 dataLen, s32 timeout = -1);
