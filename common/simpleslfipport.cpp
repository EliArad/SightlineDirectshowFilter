/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#include "simpleslfipport.h"
#include "slfip.h"

u32 SLParsePackets(const u8 *data, u32 len, handlerCallback *callback, u32 firstType, u32 nTypes)
{
  SLStatus rv;

  if(len<4)
    return 0;
  u32 i = 0;
  u32 markFirstUnknownByte = 0;

  while(i<len-4){
    markFirstUnknownByte = i;
    // Find the header signature, with enough room for length and checksum bytes at least
    for(;i<len-4;i++){
      if(data[i]==HeaderByte1 && data[i+1]==HeaderByte2) break;
    }

    if(i<len-4){
      u32 additionalBytes = data[i+SLFIP_OFFSET_LENGTH];
      // Check for invalid length
      if(!(len <= MAX_SLFIP_PACKET)) { // len is the length of the entire packet including header, and length bytes
        i+=2;
      } else {
        if(additionalBytes+i+2 < len && additionalBytes>0){
          // valid checksum?
          u8 ck = SLComputeFIPChecksum(data+i+SLFIP_OFFSET_TYPE, additionalBytes-1);
          if(ck == data[i+2+additionalBytes]){
            u8 type = data[i+SLFIP_OFFSET_TYPE];
            if(type-firstType<nTypes){
              handlerCallback cb;
              //if ((type-firstType) != 8) SLTrace("\t%s\n", SLCommandString(type-firstType)); //skip tracking
              u8 cbIdx = type-firstType;
              cb = callback[cbIdx];
              if(cb) {
                rv = cb(0, data + i );
              } 
            } 
            i+= 3 + additionalBytes;
          } else {
            // Invalid checksum, scan for another header
            i+=2;
          }
        } else {
          // All done, but need more data...
          return len-i;
        }
      }
    }
  }
  return len-i;
}
