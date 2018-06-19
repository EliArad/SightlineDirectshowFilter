/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#pragma once
#include "sltypes.h"

/*! Function Prototype for all callback function
 * These functions are used to respond to FIP commands received.
 */
typedef SLStatus (*handlerCallback)(void *context, const u8 *buffer);

u32 SLParsePackets(const u8 *data, u32 len, handlerCallback *callback, u32 firstType, u32 nTypes);
