/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#include "slfip.h"
// This File is released to customers.  Please do not add includes.

#include <stddef.h>
#include <string.h>

#ifdef linux
#pragma GCC diagnostic ignored "-Wstrict-aliasing" // dereferencing type-punned pointer will break strict-aliasing rules
#endif

///////////////////////////////////////////////////////////////////////////////
static u16 toU16(const u8 *ptr)
{
  u16 a;
  a = ptr[0] | (ptr[1]<<8);
  return a;
}

///////////////////////////////////////////////////////////////////////////////
static s16 toS16(const u8 *ptr)
{
  u16 a;
  a = ptr[0] | (ptr[1]<<8);
  return (s16)a;
}

///////////////////////////////////////////////////////////////////////////////
static u32 toU32(const u8 *ptr)
{
  u32 a;
  a = ptr[0] | (ptr[1]<<8) | (ptr[2]<<16) | (ptr[3]<<24);
  return a;
}

///////////////////////////////////////////////////////////////////////////////
static s32 toS32(const u8 *ptr)
{
  u32 a;
  a = ptr[0] | (ptr[1]<<8) | (ptr[2]<<16) | (ptr[3]<<24);
  return (s32)a;
}

///////////////////////////////////////////////////////////////////////////////
static u64 toU64(const u8 *ptr)
{
  u32 a,b;

  a = ptr[0] | (ptr[1]<<8) | (ptr[2]<<16) | (ptr[3]<<24);
  b = ptr[4] | (ptr[5]<<8) | (ptr[6]<<16) | (ptr[7]<<24);
  return (u64)a + (((u64)b)<<32);
}


///////////////////////////////////////////////////////////////////////////////
static u8 write4(u8 *buffer, u8 byteCount, u32 val)
{
  buffer[byteCount++] = val&0xFF;
  buffer[byteCount++] = (val>>8)&0xFF;
  buffer[byteCount++] = (val>>16)&0xFF;
  buffer[byteCount++] = (val>>24)&0xFF;
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static const u8 crc8Table[ ] =
{
    0,  94, 188, 226,  97,  63, 221, 131, 194, 156, 126,  32, 163, 253,  31,  65,
  157, 195,  33, 127, 252, 162,  64,  30,  95,   1, 227, 189,  62,  96, 130, 220,
   35, 125, 159, 193,  66,  28, 254, 160, 225, 191,  93,   3, 128, 222,  60,  98,
  190, 224,   2,  92, 223, 129,  99,  61, 124,  34, 192, 158,  29,  67, 161, 255,
   70,  24, 250, 164,  39, 121, 155, 197, 132, 218,  56, 102, 229, 187,  89,   7,
  219, 133, 103,  57, 186, 228,   6,  88,  25,  71, 165, 251, 120,  38, 196, 154,
  101,  59, 217, 135,   4,  90, 184, 230, 167, 249,  27,  69, 198, 152, 122,  36,
  248, 166,  68,  26, 153, 199,  37, 123,  58, 100, 134, 216,  91,   5, 231, 185,
  140, 210,  48, 110, 237, 179,  81,  15,  78,  16, 242, 172,  47, 113, 147, 205,
   17,  79, 173, 243, 112,  46, 204, 146, 211, 141, 111,  49, 178, 236,  14,  80,
  175, 241,  19,  77, 206, 144, 114,  44, 109,  51, 209, 143,  12,  82, 176, 238,
   50, 108, 142, 208,  83,  13, 239, 177, 240, 174,  76,  18, 145, 207,  45, 115,
  202, 148, 118,  40, 171, 245,  23,  73,   8,  86, 180, 234, 105,  55, 213, 139,
   87,   9, 235, 181,  54, 104, 138, 212, 149, 203,  41, 119, 244, 170,  72,  22,
  233, 183,  85,  11, 136, 214,  52, 106,  43, 117, 151, 201,  74,  20, 246, 168,
  116,  42, 200, 150,  21,  75, 169, 247, 182, 232,  10,  84, 215, 137, 107,  53
} ;

///////////////////////////////////////////////////////////////////////////////
u8 SLComputeFIPChecksum(const u8 *data, u32 len)
{
  u32 i;
  u8 crc = 1;
  for(i=0;i<len;i++){
    crc = crc8Table[ crc ^ data[i] ];
  }
  return crc;
}

///////////////////////////////////////////////////////////////////////////////
const char * FipNames[] = {
  "GetVersionNumber",                       // 0x00
  "ResetAllParameters",                     // 0x01
  "SetStabilizationParameters",             // 0x02
  "GetStabilizationParameters",             // 0x03
  "ResetStabilizationState",                // 0x04
  "ModifyTracking",                         // 0x05
  "SetOverlayMode",                         // 0x06
  "GetOverlayMode",                         // 0x07
  "StartTracking",                          // 0x08
  "StopTracking",                           // 0x09
  "NudgeTrackingCoordinate",                // 0x0A
  "SetCoordinateReportingMode",             // 0x0B
  "SetTrackingParameters",                  // 0x0C
  "GetTrackingParameters",                  // 0x0D
  "SetRegistrationParameters",              // 0x0E
  "GetRegistrationParameters",              // 0x0F
  "SetVideoParameters",                     // 0x10
  "GetVideoParameters",                     // 0x11
  "SetStabilizationBias",                   // 0x12
  "SetMetadataValues",                      // 0x13
  "SetMetadataStaticValues",                // 0x14
  "SetMetadataFrameValues",                 // 0x15
  "SetDisplayParameters",                   // 0x16
  "ModifyTrackIndex",                       // 0x17
  "SetADCParameters",                       // 0x18
  "GetADCParameters",                       // 0x19
  "SetEthernetVideoParameters",             // 0x1A
  "GetEthernetVideoParameters",             // 0x1B
  "SetNetworkParameters",                   // 0x1C
  "GetNetworkParameters",                   // 0x1D
  "SetSDRecordingParameters",               // 0x1E
  "SetVideoMode",                           // 0x1F
  "GetVideoMode",                           // 0x20
  "SetVideoEnhancementParameters",          // 0x21
  "GetVideoEnhancementParameters",          // 0x22
  "SetH264Parameters",                      // 0x23
  "GetH264Parameters",                      // 0x24
  "SaveParameters",                         // 0x25
  "SetRGB565Conversion",                    // 0x26
  "CurrentPrimaryTrackIndex",               // 0x27
  "GetParameters",                          // 0x28
  "SetEthernetDisplayParameters",           // 0x29
  "SetDisplayAdjustments",                  // 0x2A
  "SendStitchParams",                       // 0x2B
  "GetStitchParams",                        // 0x2C
  "SetDetectionParameters",                 // 0x2D
  "GetDetectionParameters",                 // 0x2E
  "SendBlendParams",                        // 0x2F
  "GetBlendParams",                         // 0x30
  "GetImageSize",                           // 0x31
  "DesignateSelectedTrackPrimary",          // 0x32
  "ShiftSelectedTrack",                     // 0x33
  "GetPrimaryTrackIndex",                   // 0x34
  "SetNuc",	            	                  // 0x35
  "GetNuc",							                    // 0x36
  "SetAcqParams",                           // 0x37
  "GetAcqParams",                           // 0x38
  "GetEthernetDisplayParameters",           // 0x39
  "GetDisplayParameters",                   // 0x3A
  "DrawObject",                             // 0x3B
  "StopSelectedTrack",                      // 0x3C
  "CommandPassThrough",                     // 0x3D
  "ConfigurePort",                          // 0x3E
  "GetPortConfig",                          // 0x3F
  "VersionNumber",                          // 0x40
  "CurrentStabilizationMode",               // 0x41
  "CurrentOverlayMode",                     // 0x42
  "TrackingPosition",                       // 0x43
  "CurrentTrackingParameters",              // 0x44
  "CurrentRegistrationParameters",          // 0x45
  "CurrentVideoParameters",                 // 0x46
  "CurrentADCParameters",                   // 0x47
  "CurrentEthernetVideoParameters",         // 0x48
  "CurrentNetworkParameters",               // 0x49
  "CurrentVideoEnhancementParameters",      // 0x4A
  "CurrentVideoModeParameters",             // 0x4B
  "CurrentStitchParameters",                // 0x4C
  "CurrentBlendParameters",                 // 0x4D
  "CurrentImageSize",                       // 0x4E
  "CurrentAcqParams",                       // 0x4F
  "Unused",                                 // 0x50
  "TrackingPositions",                      // 0x51
  "CurrentEthernetDisplayParameters",       // 0x52
  "CurrentPortConfiguration",               // 0x53
  "CurrentDetectionParameters",             // 0x54
  "Unknown_85",                             // 0x55
  "CurrentH264Parameters",                  // 0x56
  "CurrentDisplayParameters",               // 0x57
  "CurrentSDCardRecordingStatus",           // 0x58
  "CurrentSDCardDirectoryInfo",             // 0x59
  "SendTraceStr",                           // 0x5A
  "SetCommandCamera",                       // 0x5B
  "SendDisplayAngle",                       // 0x5C
  "CurrentSnapShot",                        // 0x5D
  "SetSnapShot",                            // 0x5E
  "GetSnapShot",                            // 0x5F
  "DoSnapShot",                             // 0x60
  "SetKlvData",                             // 0x61
  "SetMetadataRate",                        // 0x62
  "SetSystemType",                          // 0x63
  "SetTelemetryDestination",                // 0x64
  "CurrentSystemType",                      // 0x65
  "NetworkList",                            // 0x66
  "CurrentNetworkList",                     // 0x67
  "CurrentOverlayObjectsIds",               // 0x68
  "SetParameterBlock",                      // 0x69
  "ParameterBlock",                         // 0x6A
  "CurrentOverlayObjectParams",             // 0x6B
  "SetLensMode",                            // 0x6C
  "CurrentLensStatus",                      // 0x6D
  "SetLensParams",                          // 0x6E
  "CurrentLensParams",                      // 0x6F
  "SetDigCamParams",                        // 0x70
  "CurrentDigCamParams",                    // 0x71
  "SetUserPalette",                         // 0x72
  "CurrentUserPalette",                     // 0x73
  "SetMultipleAlignment",                   // 0x74
  "CurrentMultipleAlignment",               // 0x75
  "SetAdvancedDetectionParameters",         // 0x76
  "CurrentAdvancedDetectionParameters",     // 0x77
  "TrackingBoxPixelStats",                  // 0x78
  "DirectoryStatisticsReply",               // 0x79
  "CurrentStabilizationBias",               // 0x7A
  "SetAdvancedCaptureParameters",           // 0x7B
  "SetDetectionRegionOfInterestParams",     // 0x7C
  "CurrentDetectionRegionOfInterestParams", // 0x7D
  "Unknown_126",                            // 0x7E
  "UserWarning",                            // 0x7F
  "SystemStatus",                           // 0x80 
  "LandingAid",                             // 0x81
  "HtCamChange",                            // 0x82
  "LandingPosition",                        // 0x83
  "Unknown_132",                            // 0x84
  "SetGeneric",                             // 0x85
  "UserWarningMessage",                     // 0x86
  "SystemStatusMessage",                    // 0x87 
  "DetailedTimingMessage",                  // 0x88
  "SetAppendedMetadata",                    // 0x89
  "SetFrameIndex",                          // 0x8A
  "CurrentMetadataValues",                  // 0x8B,
  "CurrentMetadataFrameValues",             // 0x8C,
  "CurrentMetadataRate",                    // 0x8D,
  "CurrentConfiguration",                   // 0x8E,
  "ExternalProgram",                        // 0x8F,
  "StreamingControl",                       // 0x90,
  "DigiVidParseParams",                     // 0x91,
  "SetSystemValue",                         // 0x92,
  "CurrentSystemValue",                     // 0x93,
  "I2CCommand"                              // 0x94,
  };

///////////////////////////////////////////////////////////////////////////////
const char * SLCommandString( u8 enumVal )
{
  int sizeofarray = sizeof(FipNames)/sizeof(*FipNames);
  if(enumVal>=0 && enumVal<=sizeofarray)
    return FipNames[enumVal];
  else
    return "Unknown";
}

///////////////////////////////////////////////////////////////////////////////
int addAuxTelem(SLPacketType buffer, s32* byteCount, const u16 flags, const u32 frameId, const u64 timeStamp)
{
  if(flags & SL_COORD_REPORT_AUX_DATA){
    int countBefore = *byteCount;

    u32 timeStampl = timeStamp & 0xFFFFFFFF;
    u32 timeStamph = timeStamp >> 32;
    buffer[(*byteCount)++] = timeStampl & 0xFF;
    buffer[(*byteCount)++] = (timeStampl & 0xFF00) >> 8;
    buffer[(*byteCount)++] = (timeStampl & 0xFF0000) >> 16;
    buffer[(*byteCount)++] = (timeStampl & 0xFF000000) >> 24;
    buffer[(*byteCount)++] = timeStamph & 0xFF;
    buffer[(*byteCount)++] = (timeStamph & 0xFF00) >> 8;
    buffer[(*byteCount)++] = (timeStamph & 0xFF0000) >> 16;
    buffer[(*byteCount)++] = (timeStamph & 0xFF000000) >> 24;

    buffer[(*byteCount)++] = (u8)((frameId & 0x000000FF));  
    buffer[(*byteCount)++] = (u8)((frameId & 0x0000FF00) >> 8);  
    buffer[(*byteCount)++] = (u8)((frameId & 0x00FF0000) >> 16); 
    buffer[(*byteCount)++] = (u8)((frameId & 0xFF000000) >> 24); 

    // take care of the length we added:
    return (*byteCount - countBefore); 
  }
  return 0; // we didn't add anything.
}


int adjustLen(const u16 flags)
{
  if(flags & SL_COORD_REPORT_AUX_DATA){
    return 12;
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
// A request command usually only consists of a type and checksum.
///////////////////////////////////////////////////////////////////////////////
static s32 simplePack(SLPacketType buffer, u8 type)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = type;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetParameters(SLPacketType buffer, u8 id)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetParameters;
  buffer[byteCount++] = id;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetParameters(SLPacketType buffer, u8 id, u8 arg)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4;
  buffer[byteCount++] = GetParameters;
  buffer[byteCount++] = id;
  buffer[byteCount++] = arg;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetParameters(SLPacketType buffer, u8 id, u16 arg)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 5;
  buffer[byteCount++] = GetParameters;
  buffer[byteCount++] = id;
  buffer[byteCount++] = (u8)(arg & 0x00FF);
  buffer[byteCount++] = (u8)(arg>>8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetParametersMetadata(SLPacketType buffer, u8 id, u8 type, u16 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = GetParameters;
  buffer[byteCount++] = id;
  buffer[byteCount++] = type;
  buffer[byteCount++] = (u8)(dispId & 0x00FF);
  buffer[byteCount++] = (u8)(dispId>>8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetParameters(SLPacketType buffer, u8 id, const u8 *arg, u32 argLen)
{
  if (argLen > MAX_SLFIP_PAYLOAD)
    return -1;
  if (arg == 0 && argLen > 0)
    return -1;
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3 + argLen;
  buffer[byteCount++] = GetParameters;
  buffer[byteCount++] = id;
  if (arg) {
    memcpy(&buffer[byteCount], arg, argLen);
    byteCount += argLen;
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVersionNumber(SLPacketType buffer)
{
  return SLFIPGetParameters(buffer, GetVersionNumber);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetHardwareID(SLPacketType buffer)
{
  return SLFIPGetParameters(buffer, GetHardwareID);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPResetAllParameters(SLPacketType buffer, u8 resetType)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = ResetAllParameters;
  buffer[byteCount++] = resetType;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPResetAllParametersExtended(SLPacketType buffer, u8 resetType, u64 swID)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11;
  buffer[byteCount++] = ResetAllParameters;
  buffer[byteCount++] = resetType;
  int i;
  for(i=0;i<8;i++){
    buffer[byteCount++] = (u8)(swID>>(i*8));
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetStabilizationParameters(SLPacketType buffer, u8 mode, u8 rate, u8 limit, u8 angleLimit)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = SetStabilizationParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = rate;
  buffer[byteCount++] = limit;
  buffer[byteCount++] = angleLimit;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with Camera index
s32 SLFIPSetStabilizationParameters(SLPacketType buffer, u8 mode, u8 rate, u8 limit, u8 angleLimit, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7;
  buffer[byteCount++] = SetStabilizationParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = rate;
  buffer[byteCount++] = limit;
  buffer[byteCount++] = angleLimit;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with Camera index and stabLimit
s32 SLFIPSetStabilizationParameters(SLPacketType buffer, u8 mode, u8 rate, u8 disLimit, u8 angleLimit, u8 cameraIdx, u8 stabLimit)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8;
  buffer[byteCount++] = SetStabilizationParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = rate;
  buffer[byteCount++] = disLimit;
  buffer[byteCount++] = angleLimit;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = stabLimit;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 packSystemType(SLPacketType buffer, u8 type, u16 systemType)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4;
  buffer[byteCount++] = type;
  buffer[byteCount++] = (u8)(systemType & 0x00FF);        // LSB
  buffer[byteCount++] = (u8)((systemType & 0xFF00) >> 8);  // MSB
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetSystemType(SLPacketType buffer, u16 systemType)
{
  return packSystemType(buffer, SetSystemType, systemType);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetPacketDestination(SLPacketType buffer, SL_SETEL_MODE mode, u8 cameraID, u32 ipAddr, u16 udpDestinationPort, u8 flags)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 12;
  buffer[byteCount++] = SetTelemetryDestination;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = cameraID;
  buffer[byteCount++] = (u8)((ipAddr & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((ipAddr & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((ipAddr & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((ipAddr & 0x000000FF));
  buffer[byteCount++] = (u8)((udpDestinationPort & 0x000000FF));
  buffer[byteCount++] = (u8)((udpDestinationPort & 0x0000FF00)>> 8);
  buffer[byteCount++] = flags;
  buffer[byteCount++] = 0; //reserved
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

// Deprecated, use SLFIPSetPacketDestination
s32 SLFIPSetTelemetryDestination(SLPacketType buffer, SL_SETEL_MODE mode, u8 cameraID, u32 ipAddr, u16 udpDestinationPort, u8 flags) {
  return SLFIPSetPacketDestination(buffer, mode, cameraID, ipAddr, udpDestinationPort, flags);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetStabilizationParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetStabilizationParameters);
}

///////////////////////////////////////////////////////////////////////////////\
// Overloaded function with camera index
s32 SLFIPGetStabilizationParameters(SLPacketType buffer, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetStabilizationParameters;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPResetStabilizationMode(SLPacketType buffer)
{
  return simplePack(buffer, ResetStabilizationMode);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setOverlayMode(SLPacketType buffer, u8 type, u8 primaryReticle, u8 secondaryReticle, u16 graphics, u8 mtiColor, u8 mtiSelectableColor, u8 camIdx=0xfe)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = (camIdx == 0xfe) ? 8 : 9;
  buffer[byteCount++] = type;
  buffer[byteCount++] = primaryReticle;
  buffer[byteCount++] = secondaryReticle;
  buffer[byteCount++] = (u8)(graphics & 0x00FF);         // LSB
  buffer[byteCount++] = (u8)((graphics & 0xFF00) >> 8);  // MSB
  buffer[byteCount++] = mtiColor;
  buffer[byteCount++] = mtiSelectableColor;
  if (camIdx != 0xfe)
    buffer[byteCount++] = camIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetOverlayMode(SLPacketType buffer, u8 primaryReticle, u8 secondaryReticle, u16 graphics, u8 mtiColor, u8 mtiSelectableColor)
{
  return setOverlayMode(buffer, SetOverlayMode, primaryReticle, secondaryReticle, graphics, mtiColor, mtiSelectableColor);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetOverlayMode3000(SLPacketType buffer, u8 primaryReticle, u8 secondaryReticle, u16 graphics, u8 mtiColor, u8 mtiSelectableColor, u8 camIdx)
{
  return setOverlayMode(buffer, SetOverlayMode, primaryReticle, secondaryReticle, graphics, mtiColor, mtiSelectableColor, camIdx);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetOverlayMode(SLPacketType buffer)
{
  return simplePack(buffer, GetOverlayMode);
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetOverlayMode3000(SLPacketType buffer, u8 camIdx)
{
  return SLFIPGetParameters(buffer, SetOverlayMode, camIdx);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPModifyTracking(SLPacketType buffer, u16 col, u16 row, u8 flags )
{
  return SLFIPModifyTracking(buffer, col, row, flags, 0, 0 );
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPModifyTracking(SLPacketType buffer, u16 col, u16 row, u8 flags, u8 width, u8 height )
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 9;
  buffer[byteCount++] = ModifyTracking;
  buffer[byteCount++] = (u8)((col & 0x00FF));
  buffer[byteCount++] = (u8)((col & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((row & 0x00FF));
  buffer[byteCount++] = (u8)((row & 0xFF00) >> 8);
  buffer[byteCount++] = flags;
  buffer[byteCount++] = width;
  buffer[byteCount++] = height;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPModifyTracking(SLPacketType buffer, u16 col, u16 row, u8 flags, u8 width, u8 height, u8 camIdx )
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 10;
  buffer[byteCount++] = ModifyTracking;
  buffer[byteCount++] = (u8)((col & 0x00FF));
  buffer[byteCount++] = (u8)((col & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((row & 0x00FF));
  buffer[byteCount++] = (u8)((row & 0xFF00) >> 8);
  buffer[byteCount++] = flags;
  buffer[byteCount++] = width;
  buffer[byteCount++] = height;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPStartTracking(SLPacketType buffer, u16 col, u16 row, u8 flags)
{
  return SLFIPStartTracking(buffer, col, row, flags, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPStartTracking(SLPacketType buffer, u16 col, u16 row, u8 flags, u8 width, u8 height)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7;
  buffer[byteCount++] = StartTracking;
  buffer[byteCount++] = (u8)((col & 0x00FF));
  buffer[byteCount++] = (u8)((col & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((row & 0x00FF));
  buffer[byteCount++] = (u8)((row & 0xFF00) >> 8);
  buffer[byteCount++] = flags;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPStopTracking(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 5;
  buffer[byteCount++] = StopTracking;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPNudgeTrackingCoordinate(SLPacketType buffer, s8 offsetCol, s8 offsetRow, u8 rotate)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 5;
  buffer[byteCount++] = NudgeTrackingCoordinate;
  buffer[byteCount++] = offsetCol;
  buffer[byteCount++] = offsetRow;
  buffer[byteCount++] = rotate;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetCoordinateReportingPeriod(SLPacketType buffer, u8 framePeriod, u16 flags)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 5;
  buffer[byteCount++] = SetCoordinateReportingMode;
  buffer[byteCount++] = framePeriod;
  buffer[byteCount++] = (u8)(flags & 0x00FF);
  buffer[byteCount++] = (u8)((flags&0xFF00)>>8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetCoordinateReportingPeriod(SLPacketType buffer, u8 framePeriod, u16 flags, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = SetCoordinateReportingMode;
  buffer[byteCount++] = framePeriod;
  buffer[byteCount++] = (u8)(flags & 0x00FF);
  buffer[byteCount++] = (u8)((flags&0xFF00)>>8);
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetTrackingParameters(SLPacketType buffer, u8 objectSize, u8 mode, 
                                   u8 noiseMode, u8 maxMisses, u8 reserved1, 
                                   u16 nearVal, u8 acqAssist, u8 intellAssist, u8 objectHeight)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 9;
  buffer[byteCount++] = SetTrackingParameters;
  buffer[byteCount++] = objectSize;
  buffer[byteCount++] = mode | (noiseMode<<4) | ( acqAssist << 5 ) | ( intellAssist << 6 );
  buffer[byteCount++] = 0;
  buffer[byteCount++] = maxMisses;
  buffer[byteCount++] = (u8)(nearVal & 0x00FF);
  buffer[byteCount++] = (u8)((nearVal & 0xFF00) >> 8);
  buffer[byteCount++] = objectHeight > 0 ? objectHeight : objectSize;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function that includes camera index
s32 SLFIPSetTrackingParameters(SLPacketType buffer, u8 objectSize, u8 mode,
  u8 noiseMode, u8 maxMisses, u8 reserved1,
  u16 nearVal, u8 acqAssist, u8 intellAssist, u8 objectHeight, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 10;
  buffer[byteCount++] = SetTrackingParameters;
  buffer[byteCount++] = objectSize;
  buffer[byteCount++] = mode | (noiseMode << 4) | (acqAssist << 5) | (intellAssist << 6);
  buffer[byteCount++] = 0;
  buffer[byteCount++] = maxMisses;
  buffer[byteCount++] = (u8)(nearVal & 0x00FF);
  buffer[byteCount++] = (u8)((nearVal & 0xFF00) >> 8);
  buffer[byteCount++] = objectHeight > 0 ? objectHeight : objectSize;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetTrackingParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetTrackingParameters);
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function that includes camera index
s32 SLFIPGetTrackingParameters(SLPacketType buffer, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetTrackingParameters;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetRegistrationParameters(SLPacketType buffer, u16 maxTranslation, u8 maxRotation,
                                   u8 zoomRange, u8 lft, u8 rgt, u8 top, u8 bot)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 10;
  buffer[byteCount++] = SetRegistrationParameters;
  buffer[byteCount++] = (u8)(maxTranslation & 0x00FF);
  buffer[byteCount++] = (u8)((maxTranslation & 0xFF00) >> 8);
  buffer[byteCount++] = maxRotation;
  buffer[byteCount++] = zoomRange;
  buffer[byteCount++] = lft;
  buffer[byteCount++] = rgt;
  buffer[byteCount++] = top;
  buffer[byteCount++] = bot;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index 
s32 SLFIPSetRegistrationParameters(SLPacketType buffer, u16 maxTranslation, u8 maxRotation,
  u8 zoomRange, u8 lft, u8 rgt, u8 top, u8 bot, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11;
  buffer[byteCount++] = SetRegistrationParameters;
  buffer[byteCount++] = (u8)(maxTranslation & 0x00FF);
  buffer[byteCount++] = (u8)((maxTranslation & 0xFF00) >> 8);
  buffer[byteCount++] = maxRotation;
  buffer[byteCount++] = zoomRange;
  buffer[byteCount++] = lft;
  buffer[byteCount++] = rgt;
  buffer[byteCount++] = top;
  buffer[byteCount++] = bot;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetRegistrationParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetRegistrationParameters);
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPGetRegistrationParameters(SLPacketType buffer, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetRegistrationParameters;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoParameters(SLPacketType buffer, u8 autoChop, u8 chopTop, u8 chopBot, u8 chopLft, u8 chopRgt, 
                            u8 deinterlace, u8 autoReset)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  // New way - chop limited to 0-255 and pass autoChop, no fieldSelect
  buffer[byteCount++] = 9;
  buffer[byteCount++] = SetVideoParameters;
  buffer[byteCount++] = autoChop;
  buffer[byteCount++] = chopTop;
  buffer[byteCount++] = chopBot;
  buffer[byteCount++] = chopLft;
  buffer[byteCount++] = chopRgt;
  buffer[byteCount++] = deinterlace;
  buffer[byteCount++] = autoReset;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoParameters(SLPacketType buffer, u8 autoChop, u8 chopTop, u8 chopBot, u8 chopLft, u8 chopRgt, 
                            u8 deinterlace, u8 autoReset, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  // New way - chop limited to 0-255 and pass autoChop, no fieldSelect
  buffer[byteCount++] = 10;
  buffer[byteCount++] = SetVideoParameters;
  buffer[byteCount++] = autoChop;
  buffer[byteCount++] = chopTop;
  buffer[byteCount++] = chopBot;
  buffer[byteCount++] = chopLft;
  buffer[byteCount++] = chopRgt;
  buffer[byteCount++] = deinterlace;
  buffer[byteCount++] = autoReset;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVideoParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetVideoParameters);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVideoParameters(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;  
  buffer[byteCount++] = GetVideoParameters;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetStabilizationBias(SLPacketType buffer, s8 biasCol, s8 biasRow, bool autoBias, u8 updateRate)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = SetStabilizationBias;
  buffer[byteCount++] = biasCol;
  buffer[byteCount++] = biasRow;
	buffer[byteCount++] = autoBias ? 1 : 0;
  buffer[byteCount++] = updateRate;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPSetStabilizationBias(SLPacketType buffer, s8 biasCol, s8 biasRow, bool autoBias, u8 updateRate, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7;
  buffer[byteCount++] = SetStabilizationBias;
  buffer[byteCount++] = biasCol;
  buffer[byteCount++] = biasRow;
  buffer[byteCount++] = autoBias ? 1 : 0;
  buffer[byteCount++] = updateRate;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetStabilizationBias3000(SLPacketType buffer, s8 biasCol, s8 biasRow, bool autoBias, u8 updateRate, u8 camIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7;
  buffer[byteCount++] = SetStabilizationBias;
  buffer[byteCount++] = biasCol;
  buffer[byteCount++] = biasRow;
	buffer[byteCount++] = autoBias ? 1 : 0;
  buffer[byteCount++] = updateRate;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetADCParameters(SLPacketType buffer, u8 brightness, u8 contrast, u8 saturation, s8 hue, u8 luma1, u8 luma2, u8 luma3, u8 chroma1, u8 chroma2, u8 mode)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 12;
  buffer[byteCount++] = SetADCParameters;
  buffer[byteCount++] = brightness;
  buffer[byteCount++] = contrast;
  buffer[byteCount++] = saturation;
  buffer[byteCount++] = hue;
  buffer[byteCount++] = luma1;
  buffer[byteCount++] = luma2;
  buffer[byteCount++] = luma3;
  buffer[byteCount++] = chroma1;
  buffer[byteCount++] = chroma2;
  buffer[byteCount++] = mode;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetADCParameters(SLPacketType buffer, u8 brightness, u8 contrast, u8 saturation, s8 hue, u8 luma1, u8 luma2, u8 luma3, u8 chroma1, u8 chroma2, u8 mode, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 12;
  buffer[byteCount++] = SetADCParameters;
  buffer[byteCount++] = brightness;
  buffer[byteCount++] = contrast;
  buffer[byteCount++] = saturation;
  buffer[byteCount++] = hue;
  buffer[byteCount++] = luma1;
  buffer[byteCount++] = luma2;
  buffer[byteCount++] = luma3;
  buffer[byteCount++] = chroma1;
  buffer[byteCount++] = chroma2;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetADCParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetADCParameters); // return simplePack(buffer, SetADCParameters);
}

///////////////////////////////////////////////////////////////////////////////
// Set the autogain parameters
s32 SLFIPSetDigCamParameters(SLPacketType buffer, u8 camIdx,  u8 mode, u16 AGHoldmax, u16 AGHoldmin, 
                                 u8 rowROIPct, u8 colROIPct, u8 highROIPct, u8 wideROIPct)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 12;
  buffer[byteCount++] = SetDigCamParams;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = (u8)(AGHoldmax & 0x00FF);
  buffer[byteCount++] = (u8)((AGHoldmax & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(AGHoldmin & 0x00FF);
  buffer[byteCount++] = (u8)((AGHoldmin & 0xFF00) >> 8);
  buffer[byteCount++] = rowROIPct;
  buffer[byteCount++] = colROIPct;
  buffer[byteCount++] = highROIPct;
  buffer[byteCount++] = wideROIPct;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// request that we return the current autogain params
// this is for backward compatibility - the user should just call GetParameters() directly
s32 SLFIPGetDigCamParameters(SLPacketType buffer)
{
  return SLFIPGetParameters(buffer, SetDigCamParams); 
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetUserFalseColorPalette(FIPEX_PACKET *fpx, const u8* YValues, const u8* UValues, const u8* VValues, u8 type)
{
  u8 *buffer = &fpx->bytes[0];
  // note: we must assume that there are 256 values in each of the color component vectors.
  int table_size = 256;
  // note: this is a MAX_SLFIPEX_PACKET because we need a larger one hence bytecount is u16 not u8.
  u32 byteCount = SLFIPAddHeader(&fpx->bytes[0]); 
  byteCount += SlFipSetLen(&fpx->bytes[byteCount], table_size*3+2, true); // +1:type,+1:cksum.
  buffer[byteCount++] = type;
  for( int i = 0; i<table_size; i++) {
    buffer[byteCount++] = YValues[i];
    buffer[byteCount++] = UValues[i];
    buffer[byteCount++] = VValues[i];
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+2], table_size*3+1); 
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setDisplayParameters(SLPacketType buffer, u8 type, u16 rotationDegrees,
  u16 rotationLimit, u8 decayRate, u8 falseColor,
  u8 zoom, u8 zoomToTrack, s16 panCol, s16 tiltRow)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 13; // without camera index len == 13
  buffer[byteCount++] = type;
  buffer[byteCount++] = (u8)(rotationDegrees & 0x00FF);
  buffer[byteCount++] = (u8)((rotationDegrees & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(rotationLimit & 0x00FF);
  buffer[byteCount++] = (u8)((rotationLimit & 0xFF00) >> 8);
  buffer[byteCount++] = decayRate;
  buffer[byteCount++] = (falseColor & 0x7f) | (zoomToTrack ? 0x80 : 0);
  buffer[byteCount++] = zoom;
  buffer[byteCount++] = (u8)(panCol & 0x00FF);
  buffer[byteCount++] = (u8)((panCol & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(tiltRow & 0x00FF);
  buffer[byteCount++] = (u8)((tiltRow & 0xFF00) >> 8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setDisplayParameters(SLPacketType buffer, u8 type, u16 rotationDegrees, 
                                   u16 rotationLimit, u8 decayRate, u8 falseColor,
                                   u8 zoom, u8 zoomToTrack, s16 panCol, s16 tiltRow, u8 idx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 14;  // Was 9 before adding panTilt
  buffer[byteCount++] = type;
  buffer[byteCount++] = (u8)(rotationDegrees & 0x00FF);
  buffer[byteCount++] = (u8)((rotationDegrees & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(rotationLimit & 0x00FF);
  buffer[byteCount++] = (u8)((rotationLimit & 0xFF00) >> 8);
  buffer[byteCount++] = decayRate;
  buffer[byteCount++] = (falseColor&0x7f) | (zoomToTrack?0x80:0);
  buffer[byteCount++] = zoom;
  buffer[byteCount++] = (u8)(panCol & 0x00FF);
  buffer[byteCount++] = (u8)((panCol & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(tiltRow & 0x00FF);
  buffer[byteCount++] = (u8)((tiltRow & 0xFF00) >> 8);
  buffer[byteCount++] = idx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDisplayParameters(SLPacketType buffer, u16 rotationDegrees,
  u16 rotationLimit, u8 decayRate, u8 falseColor,
  u8 zoom, u8 zoomToTrack, s16 panCol, s16 tiltRow)
{
  return setDisplayParameters(buffer, SetDisplayParameters, rotationDegrees, rotationLimit, decayRate,
    falseColor, zoom, zoomToTrack, panCol, tiltRow);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDisplayParameters(SLPacketType buffer, u16 rotationDegrees, 
                                u16 rotationLimit, u8 decayRate, u8 falseColor,
                                u8 zoom, u8 zoomToTrack, s16 panCol, s16 tiltRow, u8 idx)
{
  return setDisplayParameters(buffer, SetDisplayParameters, rotationDegrees, rotationLimit, decayRate,
                                 falseColor, zoom, zoomToTrack, panCol, tiltRow, idx);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetDisplayParameters(SLPacketType buffer, u8 idx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;  
  buffer[byteCount++] = GetDisplayParameters;
  buffer[byteCount++] = idx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDisplayAngle(SLPacketType buffer, u8 idx, u16 rotationDegrees, u16 rotationLimit, u8 decayRate)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8;
  buffer[byteCount++] = SetDisplayAngle;
  buffer[byteCount++] = idx;
  buffer[byteCount++] = (u8)(rotationDegrees & 0x00FF);
  buffer[byteCount++] = (u8)((rotationDegrees & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(rotationLimit & 0x00FF);
  buffer[byteCount++] = (u8)((rotationLimit & 0xFF00) >> 8);
  buffer[byteCount++] = decayRate;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setMultipleAlignment(SLPacketType buffer, u8 type, SLFIPAlignData* alignments, u8 length)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 28; // 5*5 + 3
  buffer[byteCount++] = type;
  buffer[byteCount++] = length;
  for( int i = 0; i< length; i++){
    buffer[byteCount++] = alignments[i].vertical;
    buffer[byteCount++] = alignments[i].horizontal;
    buffer[byteCount++] = alignments[i].rotate;
    buffer[byteCount++] = alignments[i].zoom;
    buffer[byteCount++] = alignments[i].hzoom;
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMultipleAlignment(SLPacketType buffer, SLFIPAlignData* alignments, u8 length)
{
   return setMultipleAlignment(buffer, SetMultipleAlignment, alignments, length);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setAdvancedDetectionParameters(SLPacketType buffer, u8 type, SLFIPAdvancedDetectionParams params)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 40; // length of packet TBD
  buffer[byteCount++] = type;

  buffer[byteCount++] = (u8)(params.MinVel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinVel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxVel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxVel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxAccel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxAccel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MinWide & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinWide & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxWide & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxWide & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MinHigh & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinHigh & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxHigh & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxHigh & 0xFF00) >> 8);

  buffer[byteCount++] = params.hideOverlapTrks;
  buffer[byteCount++] = params.NFramesBack;
  buffer[byteCount++] = params.MergeRadius;
  buffer[byteCount++] = params.MergeDirTol;

  // Four Reserved Bytes
  buffer[byteCount++] = params.UseRegistration;
  buffer[byteCount++] = params.DetUpdateRate;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  
  buffer[byteCount++] = (u8)(params.BgTimeConst & 0x00FF);
  buffer[byteCount++] = (u8)((params.BgTimeConst & 0xFF00) >> 8);
  
  buffer[byteCount++] = params.BgEdgePenalty6;
  
  buffer[byteCount++] = params.BgResetConf;
  buffer[byteCount++] = params.BgResetOff;
  buffer[byteCount++] = params.BgResetAng;
  buffer[byteCount++] = params.BgResetFrames;
  buffer[byteCount++] = params.BgWarpConf;
  buffer[byteCount++] = params.BgWarpOff;
  buffer[byteCount++] = params.BgWarpAng;
  buffer[byteCount++] = params.BgWarpFrames;

  buffer[byteCount++] = params.MaxTrackFrames;
  buffer[byteCount++] = params.DebugFiltering;
  buffer[byteCount++] = params.Downsample;
  buffer[byteCount++] = params.MaxTelemTrks;
  buffer[byteCount++] = params.MaxKlvTrks;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount; 
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
static s32 setAdvancedDetectionParameters(SLPacketType buffer, u8 type, SLFIPAdvancedDetectionParams params, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 41; // length of packet TBD
  buffer[byteCount++] = type;

  buffer[byteCount++] = (u8)(params.MinVel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinVel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxVel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxVel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxAccel8 & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxAccel8 & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MinWide & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinWide & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxWide & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxWide & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MinHigh & 0x00FF);
  buffer[byteCount++] = (u8)((params.MinHigh & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.MaxHigh & 0x00FF);
  buffer[byteCount++] = (u8)((params.MaxHigh & 0xFF00) >> 8);

  buffer[byteCount++] = params.hideOverlapTrks;
  buffer[byteCount++] = params.NFramesBack;
  buffer[byteCount++] = params.MergeRadius;
  buffer[byteCount++] = params.MergeDirTol;

  // Four Reserved Bytes
  buffer[byteCount++] = params.UseRegistration;
  buffer[byteCount++] = params.DetUpdateRate;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;

  buffer[byteCount++] = (u8)(params.BgTimeConst & 0x00FF);
  buffer[byteCount++] = (u8)((params.BgTimeConst & 0xFF00) >> 8);

  buffer[byteCount++] = params.BgEdgePenalty6;

  buffer[byteCount++] = params.BgResetConf;
  buffer[byteCount++] = params.BgResetOff;
  buffer[byteCount++] = params.BgResetAng;
  buffer[byteCount++] = params.BgResetFrames;
  buffer[byteCount++] = params.BgWarpConf;
  buffer[byteCount++] = params.BgWarpOff;
  buffer[byteCount++] = params.BgWarpAng;
  buffer[byteCount++] = params.BgWarpFrames;

  buffer[byteCount++] = params.MaxTrackFrames;
  buffer[byteCount++] = params.DebugFiltering;
  buffer[byteCount++] = params.Downsample;
  buffer[byteCount++] = params.MaxTelemTrks;
  buffer[byteCount++] = params.MaxKlvTrks;
  buffer[byteCount++] = cameraIdx;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetAdvancedMoTDetParameters(SLPacketType buffer, SLFIPAdvancedMoTDetParams params)
{
  return setAdvancedDetectionParameters(buffer, SetAdvancedDetectionParameters, params);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetAdvancedDetectionParameters(SLPacketType buffer, SLFIPAdvancedDetectionParams params)
{
  return setAdvancedDetectionParameters(buffer, SetAdvancedDetectionParameters, params);
}

///////////////////////////////////////////////////////////////////////////////
// overloaded function with camera index
s32 SLFIPSetAdvancedDetectionParameters(SLPacketType buffer, SLFIPAdvancedDetectionParams params, u8 cameraIdx)
{
  return setAdvancedDetectionParameters(buffer, SetAdvancedDetectionParameters, params, cameraIdx);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setDetectionRegionOfInterestParameters(SLPacketType buffer, u8 type, SLFIPDetectionRoiParams params)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 19;
  buffer[byteCount++] = type;
  buffer[byteCount++] = params.flags;

  buffer[byteCount++] = (u8)(params.searchRowUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchRowUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchColUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchColUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchHeight & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchHeight & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchWidth & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchWidth & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectRowUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectRowUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectColUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectColUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectHeight & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectHeight & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectWidth & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectWidth & 0xFF00) >> 8);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
static s32 setDetectionRegionOfInterestParameters(SLPacketType buffer, u8 type, SLFIPDetectionRoiParams params, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 20;
  buffer[byteCount++] = type;
  buffer[byteCount++] = params.flags;

  buffer[byteCount++] = (u8)(params.searchRowUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchRowUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchColUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchColUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchHeight & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchHeight & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.searchWidth & 0x00FF);
  buffer[byteCount++] = (u8)((params.searchWidth & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectRowUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectRowUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectColUl & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectColUl & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectHeight & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectHeight & 0xFF00) >> 8);

  buffer[byteCount++] = (u8)(params.detectWidth & 0x00FF);
  buffer[byteCount++] = (u8)((params.detectWidth & 0xFF00) >> 8);

  buffer[byteCount++] = cameraIdx;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMotDetRegionOfInterestParameters(SLPacketType buffer, SLFIPMotDetRoiParams params)
{
  return setDetectionRegionOfInterestParameters(buffer, SetDetectionRegionOfInterestParameters, params);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDetectionRegionOfInterestParameters(SLPacketType buffer, SLFIPDetectionRoiParams params)
{
  return setDetectionRegionOfInterestParameters(buffer, SetDetectionRegionOfInterestParameters, params);
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPSetDetectionRegionOfInterestParameters(SLPacketType buffer, SLFIPDetectionRoiParams params, u8 cameraIdx)
{
  return setDetectionRegionOfInterestParameters(buffer, SetDetectionRegionOfInterestParameters, params, cameraIdx);
}

///////////////////////////////////////////////////////////////////////////////
// Deprecated support
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetAVideoParameters(SLPacketType buffer, u8 autoChop, u8 chopTop, u8 chopBot, u8 chopLft, u8 chopRgt, 
                             u8 deinterlace, u8 autoReset) {
  return SLFIPSetVideoParameters(buffer, autoChop, chopTop, chopBot, chopLft, chopRgt, deinterlace, autoReset);
}
s32 SLFIPGetAVideoParameters(SLPacketType buffer) {
  return SLFIPGetVideoParameters(buffer);
}
s32 SLFIPSetDisplayRotation(SLPacketType buffer, u16 rotationDegrees, u16 rotationLimit, u8 decayRate, u8 falseColor, u8 displayZoom, u8 zoomToTrack, s16 panCol, s16 tiltRow)
{
  return SLFIPSetDisplayParameters(buffer, rotationDegrees, rotationLimit, decayRate, falseColor, displayZoom, zoomToTrack, panCol, tiltRow);
}
s32 SLFIPSetDisplayRotation(SLPacketType buffer, u16 rotationDegrees, u16 rotationLimit, u8 decayRate, u8 falseColor, u8 displayZoom, u8 zoomToTrack, s16 panCol, s16 tiltRow, u8 idx)
{
  return SLFIPSetDisplayParameters(buffer, rotationDegrees, rotationLimit, decayRate, falseColor, displayZoom, zoomToTrack, panCol, tiltRow, idx);
}
s32 SLFIPGetDisplayRotation(SLPacketType buffer, u8 idx)
{
  return SLFIPGetDisplayParameters(buffer, idx);
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoEnhancementParameters(SLPacketType buffer, u8 enhancementMode, u8 sharpening, u8 param1, u8 param2, u8 param3)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = SetVideoEnhancementParameters;
  buffer[byteCount++] = ((sharpening & 0x0F) << 4) | (enhancementMode & 0x0F);
  buffer[byteCount++] = param1;
  buffer[byteCount++] = param2;
  buffer[byteCount++] = param3;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoEnhancementParameters(SLPacketType buffer, u8 enhancementMode, u8 sharpening, u8 param1, u8 param2, u8 param3, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7;
  buffer[byteCount++] = SetVideoEnhancementParameters;
  buffer[byteCount++] = ((sharpening & 0x0F) << 4) | (enhancementMode & 0x0F);
  buffer[byteCount++] = param1;
  buffer[byteCount++] = param2;
  buffer[byteCount++] = param3;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVideoEnhancementParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetVideoEnhancementParameters);
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVideoEnhancementParameters(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;  
  buffer[byteCount++] = GetVideoEnhancementParameters;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPCommandPassthrough(SLPacketType buffer, SLPortID destPort, u8 * data, u8 numBytes)
{
  if(numBytes > (MAX_SLFIP_PAYLOAD - 3)) return -1; // 3 = type + destPort + checksum
  
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = numBytes + 3;  // type + destPort + payload + checksum
  buffer[byteCount++] = CommandPassThrough;
  buffer[byteCount++] = destPort;
  for(int i = 0; i < numBytes; ++i)
    buffer[byteCount++] = data[i];
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 configSerialPassthrough(SLPacketType buffer, u8 type, SLPortID destPort,
                                     SLBaudRate baud, u8 dataBits, u8 stopBits, SLParity parity,
                                     u8 maxPacket, u8 maxDelay, SLProtocolType protocol,
                                     u16 inputUdpPort, u32 udpDestinationAddr, u16 udpDestinationPort,
                                     u16 udpAttNavPort) 
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 20;
  buffer[byteCount++] = type;
  buffer[byteCount++] = destPort;
  buffer[byteCount++] = baud;
  buffer[byteCount++] = dataBits;
  buffer[byteCount++] = stopBits;
  buffer[byteCount++] = parity;
  buffer[byteCount++] = maxPacket;
  buffer[byteCount++] = maxDelay;
  buffer[byteCount++] = protocol;
  buffer[byteCount++] = (u8)((inputUdpPort & 0x000000FF));
  buffer[byteCount++] = (u8)((inputUdpPort & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((udpDestinationAddr & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((udpDestinationAddr & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((udpDestinationAddr & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((udpDestinationAddr & 0x000000FF));
  buffer[byteCount++] = (u8)((udpDestinationPort & 0x000000FF));
  buffer[byteCount++] = (u8)((udpDestinationPort & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((udpAttNavPort & 0x000000FF));
  buffer[byteCount++] = (u8)((udpAttNavPort & 0x0000FF00)>> 8);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPConfigSerialPassthrough(SLPacketType buffer, SLPortID destPort, 
                                 SLBaudRate baud, u8 dataBits, u8 stopBits, SLParity parity,
                                 u8 maxPacket, u8 maxDelay, SLProtocolType protocol,
                                 u16 inputUdpPort, u32 udpDestinationAddr, u16 udpDestinationPort, u16 udpAttNavPort) 
{
  return configSerialPassthrough(buffer, ConfigureCommunicationsPort, destPort, baud, dataBits, stopBits, parity, maxPacket, maxDelay, protocol,
                                   inputUdpPort, udpDestinationAddr, udpDestinationPort, udpAttNavPort);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetSerialPassthrough(SLPacketType buffer, SLPortID destPort)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetPortConfig;
  buffer[byteCount++] = destPort;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// TODO: customer never likely to gen a response; move to slfip_resp.c?
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPVersionNumber(SLPacketType buffer, u8 swMajor, u8 swMinor, u8 hwVersion,
                       u8 degreesF, u64 hwID, u32 appBits, u8 swRelease, SL_BOARD_TYPE boardType,u16 otherVersion)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 17;
  buffer[byteCount++] = VersionNumber;
  buffer[byteCount++] = swMajor;
  buffer[byteCount++] = swMinor;
  buffer[byteCount++] = hwVersion;
  buffer[byteCount++] = degreesF;
  for(u8 i=0; i<3; i++) {
    buffer[byteCount++] = ((u8*)&hwID)[i];
  }
  for(u8 i=0; i<4; i++) {
    buffer[byteCount++] = ((u8*)&appBits)[i];
  }
  //SLDebugAssert(boardType >= 0 && boardType < SL_BOARD_TYPE_Last); // Make sure it is a byte value.
  buffer[byteCount++] = (u8)boardType;
  buffer[byteCount++] = swRelease;
  buffer[byteCount++] = (u8)(otherVersion & 0x00FF); // used for FPGA version
  buffer[byteCount++] = (u8)((otherVersion & 0xFF00) >> 8);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPHardwareID(SLPacketType buffer, u64 hwID)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 10;
  buffer[byteCount++] = GetHardwareID;
  for(u8 i=0; i<8; i++) {
    buffer[byteCount++] = ((u8*)&hwID)[i];
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPTrackingPosition(SLPacketType buffer, 
                          s16 trackingRow, s16 trackingCol, u8 trackingConfidence, 
                          f32 sceneRow, f32 sceneCol, f32 sceneAngle, f32 sceneScale, u8 sceneConfidence,
                          s16 offsetRow, s16 offsetCol, u16 rotation, u8 idx, u8 reserved,
                          u16 flags, u32 frameId, u64 timeStamp)
{
  s32 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 26;
  buffer[byteCount++] = TrackingPosition;
  buffer[byteCount++] = (u8)(trackingCol & 0x00FF);
  buffer[byteCount++] = (u8)((trackingCol & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(trackingRow & 0x00FF);
  buffer[byteCount++] = (u8)((trackingRow & 0xFF00) >> 8);
 
  s32 sceneCol8 = (s32)SLROUNDZERO(sceneCol*256);
  s32 sceneRow8 = (s32)SLROUNDZERO(sceneRow*256);
  buffer[byteCount++] = (u8)((sceneCol8 & 0x00FF00) >> 8);
  buffer[byteCount++] = (u8)((sceneCol8 & 0xFF0000) >> 16);
  buffer[byteCount++] = (u8)((sceneRow8 & 0x00FF00) >> 8);
  buffer[byteCount++] = (u8)((sceneRow8 & 0xFF0000) >> 16);

  buffer[byteCount++] = (u8)(offsetCol & 0x00FF);
  buffer[byteCount++] = (u8)((offsetCol & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(offsetRow & 0x00FF);
  buffer[byteCount++] = (u8)((offsetRow & 0xFF00) >> 8);

  buffer[byteCount++] = trackingConfidence;
  buffer[byteCount++] = sceneConfidence;

  buffer[byteCount++] = (u8)(rotation & 0x00FF);
  buffer[byteCount++] = (u8)((rotation & 0xFF00) >> 8);

  buffer[byteCount++] = idx;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = (u8)((sceneCol8 & 0x0000FF));
  buffer[byteCount++] = (u8)((sceneRow8 & 0x0000FF));

  s16 sceneAngle7 = (s16)SLCLIPS16(SLROUNDZERO(sceneAngle*128));
  buffer[byteCount++] = (u8)(sceneAngle7 & 0x00FF);
  buffer[byteCount++] = (u8)((sceneAngle7 & 0xFF00) >> 8);

  s16 sceneScale8 = (s16)SLCLIPS16(SLROUNDZERO(sceneScale*256));
  buffer[byteCount++] = (u8)(sceneScale8 & 0x00FF);
  buffer[byteCount++] = (u8)((sceneScale8 & 0xFF00) >> 8);

  // auxiliary telemetry may impact packet length:
  buffer[SLFIP_OFFSET_LENGTH]+=addAuxTelem(buffer, &byteCount, flags, frameId, timeStamp);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackTrackingPosition(SLTelemetryData *trackingPosition, const SLPacketType buffer, u64 *timeStamp, u32 *frameIdx )
{
  s32 extraLenBytes = 0;
  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);

  byteCount = 4;

  trackingPosition->trackCol = toS16(&buffer[byteCount]); byteCount+=2;
  trackingPosition->trackRow = toS16(&buffer[byteCount]); byteCount+=2;

  s16 sceneCol = toS16(&buffer[byteCount]); byteCount+=2;
  s16 sceneRow = toS16(&buffer[byteCount]); byteCount+=2;

  trackingPosition->displayCol = toS16(&buffer[byteCount]); byteCount+=2;
  trackingPosition->displayRow = toS16(&buffer[byteCount]); byteCount+=2;

  trackingPosition->trackingConfidence  = buffer[byteCount++];
  trackingPosition->sceneConfidence     = buffer[byteCount++];

  trackingPosition->displayAngle7 = toU16(&buffer[byteCount]); byteCount+=2;
  // WARN: don't divide by 128 here as it will get shifted elsewhere.

  trackingPosition->idx = buffer[byteCount++];
  trackingPosition->reserved = buffer[byteCount++];
  
  u8 sceneColFrac8 = buffer[byteCount++];
  u8 sceneRowFrac8 = buffer[byteCount++];

  trackingPosition->sceneCol = (f32)sceneCol + (sceneColFrac8/256.0f);
  trackingPosition->sceneRow = (f32)sceneRow + (sceneRowFrac8/256.0f);

  trackingPosition->sceneAngle7 = toS16(&buffer[byteCount]); byteCount+=2;
  trackingPosition->sceneScale8 = toU16(&buffer[byteCount]); byteCount+=2;

  if (len >= 38){ // auxiliary telemetry added.
    if( timeStamp )
      *timeStamp = toU64(&buffer[byteCount]); byteCount += 8;
    if( frameIdx )
      *frameIdx = toU32(&buffer[byteCount]); byteCount += 4;
  }
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackTrackingPositions(u8 * cameraIndex, void *trks, u8 maxTracks, const SLPacketType buffer, u64 *timeStamp, u32 *frameIdx )
{
  s32 extraLenBytes = 0;

  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);
  byteCount += 1+extraLenBytes;

  if(buffer[byteCount++] != TrackingPositions)
    return -1;

  *cameraIndex = buffer[byteCount++];
  u8 expectedTracks = buffer[byteCount++];
  u8 mtiPayload = 0;
  if(expectedTracks&0x80) { //high bit set indicates mti payload
    mtiPayload = 1;
    expectedTracks = (expectedTracks&(~0x80));
  }

  if(expectedTracks>maxTracks) return -1;

  SLFIPTrackRes *tracks = (SLFIPTrackRes *)trks;

  s32 numTracks = 0;
  for(u8 idx=0; (idx < expectedTracks) && (byteCount < len); idx++, numTracks++) {
    tracks[idx].idx = buffer[byteCount++];
    tracks[idx].col = toS16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].row = toS16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].wide = toU16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].high = toU16(&buffer[byteCount]); byteCount += 2;

    tracks[idx].velCol8 = toS16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].velRow8 = toS16(&buffer[byteCount]); byteCount += 2;
    
    tracks[idx].confidence = buffer[byteCount++];
    
    tracks[idx].flags    = buffer[byteCount]&0x83;
    tracks[idx].mti      = mtiPayload;

    tracks[idx].resultState = (buffer[byteCount]>>2)&0x3;
    tracks[idx].frame       = 7; // >=7 here makes it draw as a regular track box

    byteCount++;
  }

  if( len-byteCount >= 10 ){
    if( timeStamp )
      *timeStamp = toU64(&buffer[byteCount]); byteCount += 8;
    if( frameIdx )
      *frameIdx = toU32(&buffer[byteCount]); byteCount += 4;
  }
  return numTracks;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPTrackingPositions( SLPacketType buffer, void *trks, u8 numTracks, u8 cameraIndex, u8 mti,
                            u16 flags, u32 frameId, u64 timeStamp)
{
  s32 byteCount = SLFIPAddHeader(&buffer[0]);
  u32 len = 15*numTracks + 4;
  len+=adjustLen(flags);
  s32 lenBytes = SlFipSetLen(&buffer[byteCount], len, true);
  byteCount += lenBytes;
  buffer[byteCount++] = TrackingPositions;

  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = numTracks|(mti?0x80:0x00); //set high bit of numTracks to indicate that this is an mti payload

  SLFIPTrackRes *tracks = (SLFIPTrackRes *)trks;  // outbound

  for(s32 idx=0; idx<numTracks; idx++) {
    buffer[byteCount++] = (u8)tracks[idx].idx;
    s16 col = (s16)SLROUNDZERO(tracks[idx].col);
    s16 row = (s16)SLROUNDZERO(tracks[idx].row);
    buffer[byteCount++] = (u8)( col&0xFF);
    buffer[byteCount++] = (u8)((col&0xFF00)>>8);
    buffer[byteCount++] = (u8)( row&0xFF);
    buffer[byteCount++] = (u8)((row&0xFF00)>>8);

    buffer[byteCount++] = (u8)(tracks[idx].wide&0xFF);
    buffer[byteCount++] = (u8)((tracks[idx].wide&0xFF00)>>8);
    buffer[byteCount++] = (u8)(tracks[idx].high&0xFF);
    buffer[byteCount++] = (u8)((tracks[idx].high&0xFF00)>>8);

    buffer[byteCount++] = (u8)(tracks[idx].velCol8&0xFF);
    buffer[byteCount++] = (u8)((tracks[idx].velCol8&0xFF00)>>8);
    buffer[byteCount++] = (u8)(tracks[idx].velRow8&0xFF);
    buffer[byteCount++] = (u8)((tracks[idx].velRow8&0xFF00)>>8);

    buffer[byteCount++] = tracks[idx].confidence;

    buffer[byteCount] = tracks[idx].flags;
    buffer[byteCount] |= ((tracks[idx].resultState&0x3) << 2);

    byteCount++;
  }

  addAuxTelem(buffer, &byteCount, flags, frameId, timeStamp);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+lenBytes], len-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// pixel stats over all current track boxes
s32 SLFIPTrackingPixelStats(SLPacketType buffer, void *trks, u8 numTracks, u8 cameraIndex, u16 flags, u32 frameId, u64 timeStamp)
{
  s32 byteCount = SLFIPAddHeader(&buffer[0]);
  u32 len = 7*numTracks + 4;
  len+=adjustLen(flags);
  s32 lenBytes = SlFipSetLen(&buffer[byteCount], len, true);
  byteCount += lenBytes;

  buffer[byteCount++] = TrackingBoxPixelStats;

  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = numTracks; //set high bit of numTracks to indicate that this is an mti payload

  SLFIPSLTrackPixStatsPt *tracksStat = (SLFIPSLTrackPixStatsPt *)trks;  // outbound

  for(s32 idx=0; idx<numTracks; idx++) {
    buffer[byteCount++] = (u8)tracksStat[idx].idx;
    buffer[byteCount++] = (u8)(tracksStat[idx].mean&0xFF);
    buffer[byteCount++] = (u8)((tracksStat[idx].mean&0xFF00)>>8);
    buffer[byteCount++] = (u8)(tracksStat[idx].max&0xFF);
    buffer[byteCount++] = (u8)((tracksStat[idx].max&0xFF00)>>8);
    buffer[byteCount++] = (u8)(tracksStat[idx].min&0xFF);
    buffer[byteCount++] = (u8)((tracksStat[idx].min&0xFF00)>>8);
  }

  // auxiliary telemetry may impact packet length:
  addAuxTelem(buffer, &byteCount, flags, frameId, timeStamp);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+lenBytes], len-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackTrackingPixelStats(u8 * cameraIndex, void *trks, u8 maxTracks, const SLPacketType buffer, u64* timeStamp, u32* frameIdx)
{
  s32 extraLenBytes = 0;

  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);
  byteCount += 1+extraLenBytes;

  if(buffer[byteCount++] != TrackingBoxPixelStats)
    return -1;

  *cameraIndex = buffer[byteCount++];
  u8 expectedTracks = buffer[byteCount++];

  if(expectedTracks> maxTracks) return -1;

  SLFIPSLTrackPixStatsPt *tracks = (SLFIPSLTrackPixStatsPt *)trks;

  s32 numTracks = 0;
  for(u8 idx=0; (idx < expectedTracks) && (byteCount < len); idx++, numTracks++) {
    tracks[idx].idx = buffer[byteCount++];
    tracks[idx].mean = toU16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].max = toU16(&buffer[byteCount]); byteCount += 2;
    tracks[idx].min = toU16(&buffer[byteCount]); byteCount += 2;
  }

  if( len-byteCount >= 10 ){
    if( timeStamp )
      *timeStamp = toU64(&buffer[byteCount]); byteCount += 8;
    if( frameIdx )
      *frameIdx = toU32(&buffer[byteCount]); byteCount += 4;
  }

  return numTracks;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPFocusStats(SLPacketType buffer, f32 metric, u64 timeStamp, u8 cameraIndex, u16 flags, u32 frameIndex)
{
  s32 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 19;
  buffer[byteCount++] = FocusStats;
  buffer[byteCount++] = cameraIndex;
  s32 m8 = SLCLIPS32(SLROUNDZERO(metric*256.0f));
  buffer[byteCount++] = (u8)((m8&0x000000FF));
  buffer[byteCount++] = (u8)((m8&0x0000FF00)>>8);
  buffer[byteCount++] = (u8)((m8&0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((m8&0xFF000000)>>24);
  u32 timeStampl = timeStamp&0xFFFFFFFF;
  u32 timeStamph = timeStamp>>32;
  buffer[byteCount++] = timeStampl&0xFF;
  buffer[byteCount++] = (timeStampl&0xFF00)>>8;
  buffer[byteCount++] = (timeStampl&0xFF0000)>>16;
  buffer[byteCount++] = (timeStampl&0xFF000000)>>24;
  buffer[byteCount++] = timeStamph&0xFF;
  buffer[byteCount++] = (timeStamph&0xFF00)>>8;
  buffer[byteCount++] = (timeStamph&0xFF0000)>>16;
  buffer[byteCount++] = (timeStamph&0xFF000000)>>24;

  // since this telemetry packet already had timestamp prior to 2.23, we elected to just always add the frame index.
  byteCount = write4(buffer, byteCount, frameIndex);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackFocusStats(u8* cameraIndex, f32 *metric, u64 *timeStamp, const SLPacketType buffer, u32 *frameIdx )
{
  s32 extraLenBytes = 0;

  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);
  byteCount += 1+extraLenBytes;

  if(buffer[byteCount++] != FocusStats)
    return -1;

  if( len >= 15 )
    *cameraIndex = buffer[byteCount++];
  
  *metric    = (f32)toS32(&buffer[byteCount])/256.0f; byteCount+=4;
  *timeStamp = toU64(&buffer[byteCount]); byteCount+=8;

  if( len>= 19 ){
    if( frameIdx )
      *frameIdx = toU32(&buffer[byteCount]); byteCount+=4;
  }
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetImageSize(SLPacketType buffer)
{
  return simplePack(buffer, GetImageSize);
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPGetImageSize(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetImageSize;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPParameterBlock(FIPEX_PACKET *fpx, const char * data, u16 len)
{
  u32 byteCount = SLFIPAddHeader(&fpx->bytes[0]);
  s32 lenBytes = SlFipSetLen(&fpx->bytes[byteCount], 1+len+1, true); // 1:type,1:cksum.
  byteCount += lenBytes;
  fpx->bytes[byteCount++] = ParameterBlock;
  memcpy(&fpx->bytes[byteCount], data, len);
  byteCount += len;
  fpx->bytes[byteCount++] = SLComputeFIPChecksum(&fpx->bytes[SLFIP_OFFSET_LENGTH+lenBytes], len+1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setNetworkParameters(SLPacketType buffer, u8 type, u8 mode,
                                     u32 ipAddr, u32 subnet, u32 gateway,
                                     u16 c2replyPort, u16 telemetryReplyPort, u8 modes, u8 index, u16 c2listenPort, u16 c2listenPort2)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 25;
  buffer[byteCount++] = type;
  
  buffer[byteCount++] = mode;

  buffer[byteCount++] = (u8)((ipAddr & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((ipAddr & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((ipAddr & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((ipAddr & 0x000000FF));

  buffer[byteCount++] = (u8)((subnet & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((subnet & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((subnet & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((subnet & 0x000000FF));

  buffer[byteCount++] = (u8)((gateway & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((gateway & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((gateway & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((gateway & 0x000000FF));

  buffer[byteCount++] = (u8)((c2replyPort & 0xFF00)>> 8);
  buffer[byteCount++] = (u8)((c2replyPort & 0x00FF));
  buffer[byteCount++] = (u8)((telemetryReplyPort & 0xFF00)>> 8);
  buffer[byteCount++] = (u8)((telemetryReplyPort & 0x00FF));
  buffer[byteCount++] = modes;
  buffer[byteCount++] = index;

  buffer[byteCount++] = (u8)((c2listenPort & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((c2listenPort & 0x00FF));
  buffer[byteCount++] = (u8)((c2listenPort2 & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((c2listenPort2 & 0x00FF));

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetNetworkParameters(SLPacketType buffer, u8 mode, u32 ipAddr, u32 subnet, u32 gateway, u16 c2replyPort, u16 telemetryReplyPort, u8 modes, u8 index, u16 c2listenPort, u16 c2listenPort2)
{
  return setNetworkParameters(buffer, SetNetworkParameters, mode, ipAddr, subnet, gateway, c2replyPort, telemetryReplyPort, modes, index, c2listenPort, c2listenPort2);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetNetworkParameters(SLPacketType buffer, u8 index)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;

  buffer[byteCount++] = GetNetworkParameters;
  buffer[byteCount++] = index;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSaveParameters(SLPacketType buffer)
{
  return simplePack(buffer, SaveParameters);
}

///////////////////////////////////////////////////////////////////////////////
static s32 slNetworkDisplayParams(SLPacketType buffer, u8 type, u8 protocol, u32 ipAddr, u16 port, s32 displayId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 9;
  buffer[byteCount++] = type;
  
  buffer[byteCount++] = protocol;

  // convert to IN_ADDR order
  buffer[byteCount++] = (u8)((ipAddr & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((ipAddr & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((ipAddr & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((ipAddr & 0x000000FF));

  buffer[byteCount++] = (u8)((port & 0x000000FF));
  buffer[byteCount++] = (u8)((port & 0x0000FF00)>> 8);
  
  if (displayId != 0) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)displayId;
    buffer[byteCount++] = (u8)(displayId >> 8);
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetEthernetDisplay(SLPacketType buffer, SLVideoProtocol protocol, u32 ipAddr, u16 port, s32 displayId)
{
  return slNetworkDisplayParams(buffer, SetEthernetDisplayParameters, protocol, ipAddr, port, displayId);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setEthernetVideoParameters(SLPacketType buffer, u8 type, u8 quality, u8 foveal, u8 frameStep, u8 frameSize, s32 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6;
  buffer[byteCount++] = type;
  buffer[byteCount++] = quality;
  buffer[byteCount++] = foveal;
  buffer[byteCount++] = frameStep;
  buffer[byteCount++] = frameSize;
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetEthernetVideoParameters(SLPacketType buffer, u8 quality, u8 foveal, u8 frameStep, u8 frameSize, s32 dispId)
{
  return setEthernetVideoParameters(buffer, SetEthernetVideoParameters, quality, foveal, frameStep, frameSize, dispId);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetEthernetVideoParameters(SLPacketType buffer)
{
  return simplePack(buffer, GetEthernetVideoParameters);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setVideoMode( SLPacketType buffer, u8 type, 
                             u8 numCamera, u8 numDisplay, u8 displayMode, u8 displayDest, 
                             u8 camIdx0, u8 camIdx1, u8 camIdx2, u8 camIdx3, u8 pipScale, u8 pipQuadrant, u8 disIdx0, 
                             u8 disIdx1, u8 disIdx2, u8 disIdx3)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 16;
  buffer[byteCount++] = type;
  
  buffer[byteCount++] = numCamera;
  buffer[byteCount++] = numDisplay;
  buffer[byteCount++] = displayMode;
  buffer[byteCount++] = displayDest;

  buffer[byteCount++] = camIdx0;
  buffer[byteCount++] = camIdx1;
  buffer[byteCount++] = camIdx2;
  buffer[byteCount++] = camIdx3;

  buffer[byteCount++] = pipScale;
  buffer[byteCount++] = pipQuadrant;

	buffer[byteCount++] = disIdx0;
	buffer[byteCount++] = disIdx1;
	buffer[byteCount++] = disIdx2;
	buffer[byteCount++] = disIdx3;
  
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoMode( SLPacketType buffer, u8 numCamera, u8 numDisplay, u8 displayMode, u8 displayDest, 
                       u8 camIdx0, u8 camIdx1, u8 camIdx2, u8 camIdx3, u8 pipScale, u8 pipQuadrant, u8 disIdx0,
                       u8 disIdx1, u8 disIdx2, u8 disIdx3)
{
  return setVideoMode( buffer, SetVideoMode, 
                            numCamera, numDisplay, displayMode, displayDest, 
                            camIdx0, camIdx1, camIdx2, camIdx3, pipScale, pipQuadrant, disIdx0,
                            disIdx1, disIdx2, disIdx3);      
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetVideoMode( SLPacketType buffer )
{
  return simplePack(buffer, GetVideoMode);
}

///////////////////////////////////////////////////////////////////////////////
static s32 setVideoMode3000( SLPacketType buffer, u8 type, 
                             u8 numCamera, u8 displayMode, u8 displayDest, u8 camIdx0, u8 pipScale, u8 pipQuadrant, 
                             u8 disIdx0, u8 disIdx1, u8 disIdx2, u8 disIdx3,
                             const u8 *camDisp, u8 hdmiResolution, u8 hdsdiResolution, u8 analogResolution)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  const u8 firstByteCount = byteCount;

  buffer[byteCount++] = 24;
  buffer[byteCount++] = type;

  buffer[byteCount++] = numCamera;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = displayMode;
  buffer[byteCount++] = displayDest;

  buffer[byteCount++] = camIdx0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;

  buffer[byteCount++] = pipScale;
  buffer[byteCount++] = pipQuadrant;

  buffer[byteCount++] = disIdx0;
  buffer[byteCount++] = disIdx1;
  buffer[byteCount++] = disIdx2;
  buffer[byteCount++] = disIdx3;

  if (camDisp) {
    //SLDebugAssert(camDispObsolete == 0);
    buffer[byteCount++] = camDisp[0]; // Analog display input video source.
    buffer[byteCount++] = camDisp[1]; // HDMI input source.
    buffer[byteCount++] = camDisp[2]; // Net0 input source.
    buffer[byteCount++] = camDisp[3]; // Net1 input source.
    buffer[byteCount++] = camDisp[4]; // HD-SDI input source
  }


  buffer[byteCount++] = hdmiResolution;
  buffer[byteCount++] = hdsdiResolution;
  buffer[byteCount++] = analogResolution;
  
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetVideoMode3000(SLPacketType buffer, u8 numCamera, u8 displayMode, u8 displayDest, 
                       u8 camIdx0, u8 pipScale, u8 pipQuadrant, u8 disIdx0, u8 disIdx1, u8 disIdx2, u8 disIdx3,
                       const u8 *camDisp, u8 hdmiResolution, u8 hdsdiResolution, u8 analogResolution)
{
  return setVideoMode3000(buffer, SetVideoMode, numCamera, displayMode, displayDest, 
    camIdx0, pipScale, pipQuadrant, disIdx0, disIdx1, disIdx2, disIdx3, camDisp, hdmiResolution, hdsdiResolution, analogResolution);
}

///////////////////////////////////////////////////////////////////////////////
// Obsolete: before 2.21.1. -- can be removed once 3000 2.21.0 or prior is gone from the field.
s32 SLFIPSetVideoMode_2_21_0(SLPacketType buffer, u8 numCamera, u8 displayMode, u8 displayDest, 
                       u8 camIdx0, u8 pipScale, u8 pipQuadrant, u8 disIdx0, u8 disIdx1, u8 disIdx2, u8 disIdx3,
                       const u16 *camDispOld)
{
  return setVideoMode3000(buffer, SetVideoMode, numCamera, displayMode, displayDest, 
    camIdx0, pipScale, pipQuadrant, disIdx0, disIdx1, disIdx2, disIdx3, 0, 0, 0, 0);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetCommandCamera( SLPacketType buffer, u8 camIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = SetCommandCamera;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendStitchParams(SLPacketType buffer, u8 cameraIndex, u8 up, u8 right, u8 down, u8 left,
                          u8 rotation, u8 zoom, bool reset)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 10;
  buffer[byteCount++] = SendStitchParams;
  
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = up;
  buffer[byteCount++] = right;
  buffer[byteCount++] = down;
  buffer[byteCount++] = left;
  buffer[byteCount++] = rotation;
  buffer[byteCount++] = zoom;

	buffer[byteCount++] = reset ? 1 : 0;
  
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetStitchParams(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = GetStitchParams;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMotDetParams(SLPacketType buffer, u8 mode, int debugCode, u8 threshold, u8 manualThreshold, u8 manualWatchFrames, u8 suspScore, u8 frameStep, u8 modeUpperByte, u16 minTemperature, u16 maxTemperature)
{
  return SLFIPSetDetectionParams(buffer, mode, debugCode, threshold, manualThreshold, manualWatchFrames, suspScore, frameStep, modeUpperByte, minTemperature, maxTemperature);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetMotDetParams(SLPacketType buffer)
{
  return SLFIPGetDetectionParams(buffer);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDetectionParams(SLPacketType buffer, u8 mode, int debugCode, u8 threshold, u8 manualThreshold, u8 manualWatchFrames, u8 suspScore, u8 frameStep, u8 modeUpperByte, u16 minTemperature, u16 maxTemperature)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 14;
  buffer[byteCount++] = SetDetectionParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = debugCode;
	buffer[byteCount++] = threshold;
  buffer[byteCount++] = manualThreshold;
  buffer[byteCount++] = manualWatchFrames;
  buffer[byteCount++] = suspScore;
  buffer[byteCount++] = frameStep;
  buffer[byteCount++] = modeUpperByte;
  buffer[byteCount++] = (u8)(minTemperature&0xFF);
  buffer[byteCount++] = (u8)((minTemperature&0xFF00)>>8);
  buffer[byteCount++] = (u8)(maxTemperature&0xFF);
  buffer[byteCount++] = (u8)((maxTemperature&0xFF00)>>8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPSetDetectionParams(SLPacketType buffer, u8 mode, int debugCode, u8 threshold, u8 manualThreshold, u8 manualWatchFrames, u8 suspScore, u8 frameStep, u8 modeUpperByte, u16 minTemperature, u16 maxTemperature, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 15;
  buffer[byteCount++] = SetDetectionParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = debugCode;
  buffer[byteCount++] = threshold;
  buffer[byteCount++] = manualThreshold;
  buffer[byteCount++] = manualWatchFrames;
  buffer[byteCount++] = suspScore;
  buffer[byteCount++] = frameStep;
  buffer[byteCount++] = modeUpperByte;
  buffer[byteCount++] = (u8)(minTemperature & 0xFF);
  buffer[byteCount++] = (u8)((minTemperature & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(maxTemperature & 0xFF);
  buffer[byteCount++] = (u8)((maxTemperature & 0xFF00) >> 8);
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetDetectionParams(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = GetDetectionParameters;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPGetDetectionParams(SLPacketType buffer, u8 cameraIdx)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetDetectionParameters;
  buffer[byteCount++] = cameraIdx;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetBlendParams(SLPacketType buffer, u8 mode, bool absOff, s8 vertical, s8 horizontal,
                        u8 rotation, u8 zoom, u8 delay, u8 amt, u8 hue, u8 grey, bool reset, u8 warpIdx,
                        u8 fixedIdx, u8 usePresetAlign, u8 presetAlignIndex, u8 zoomMul, u8 hzoom)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 18;
  buffer[byteCount++] = SendBlendParams;
  
  buffer[byteCount++] = (absOff ? 1 : 0) | ((zoomMul&0x7)<<1); // NOTE:  Only allowing up to 7 multiplier for now
  buffer[byteCount++] = vertical;
  buffer[byteCount++] = horizontal;
  buffer[byteCount++] = rotation;
  buffer[byteCount++] = zoom;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = amt;
  buffer[byteCount++] = hue;
  buffer[byteCount++] = grey;
  buffer[byteCount++] = reset ? 255 : 0;
  buffer[byteCount++] = delay;
  buffer[byteCount++] = warpIdx;
  buffer[byteCount++] = fixedIdx;
  buffer[byteCount++] = usePresetAlign;
  buffer[byteCount++] = presetAlignIndex;
  buffer[byteCount++] = hzoom;
  
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetBlendParams(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = GetBlendParams;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPShiftSelectedTarget(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = ShiftSelectedTrack;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPShiftSelectedTarget(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = ShiftSelectedTrack;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetPrimaryTrackIndex(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = GetPrimaryTrackIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPDesignateSelectedTrackPrimary(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = DesignateSelectedTrackPrimary;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;

}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPDesignateSelectedTrackPrimary(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = DesignateSelectedTrackPrimary;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;

}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPStopSelectedTrack(SLPacketType buffer)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 2;
  buffer[byteCount++] = StopSelectedTrack;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Overloaded function with camera index
s32 SLFIPStopSelectedTrack(SLPacketType buffer, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = StopSelectedTrack;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPModifyTargetIndex(SLPacketType buffer, u8 index, u8 flags)
{
	u8 byteCount = SLFIPAddHeader(&buffer[0]);
	buffer[byteCount++] = 4;
	buffer[byteCount++] = ModifyTrackIndex;
	buffer[byteCount++] = index;
	buffer[byteCount++] = flags;
	buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
	return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPMotDetParams(SLPacketType buffer, u8 mode, u8 threshold, u8 manualThreshold, u8 manualWatchFrames, u8 suspScore, u8 debug, u8 frameStep, u8 modeUpperByte, u16 minTemperature, u16 maxTemperature)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 14;
  buffer[byteCount++] = CurrentMovingTargetDetectionParameters;
  buffer[byteCount++] = mode;
  buffer[byteCount++] = debug;
  buffer[byteCount++] = threshold;
  buffer[byteCount++] = manualThreshold;
  buffer[byteCount++] = manualWatchFrames;
  buffer[byteCount++] = suspScore;
  buffer[byteCount++] = frameStep;
  buffer[byteCount++] = modeUpperByte;
  buffer[byteCount++] = (u8)(minTemperature&0xFF);
  buffer[byteCount++] = (u8)((minTemperature&0xFF00)>>8);
  buffer[byteCount++] = (u8)(maxTemperature&0xFF);
  buffer[byteCount++] = (u8)((maxTemperature&0xFF00)>>8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setAcqParams( SLPacketType buffer, u8 type, u8 cameraIndex,
                             u8 cameraType, u16 high, u16 wide, u8 bitdepth,
                             u16 vertFrontPorch, u16 horzFrontPorch, u16 flags, u8 frameStep,
                             u16 validRow, u16 validCol, u16 validHigh, u16 validWide, const char *optArgs,
                             u16 bigHigh, u16 bigWide, u16 bigVertFrontPorch, u16 bigHorzFrontPorch)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]), lenField = byteCount;
  u32 len = optArgs ? SLMIN(strlen(optArgs), 255): 0;
  buffer[byteCount++] = 25 + 8 + len + 1; // +1 for len
  buffer[byteCount++] = type;

  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = cameraType;
  buffer[byteCount++] = (u8)((high & 0x00FF));
  buffer[byteCount++] = (u8)((high & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((wide & 0x00FF));
  buffer[byteCount++] = (u8)((wide & 0xFF00) >> 8);
  buffer[byteCount++] = bitdepth;
  buffer[byteCount++] = (u8)((vertFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((vertFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((horzFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((horzFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((flags & 0x00FF));
  buffer[byteCount++] = (u8)((flags & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(frameStep);
  buffer[byteCount++] = 0; // reserved
  buffer[byteCount++] = (u8)((validRow & 0x00FF));
  buffer[byteCount++] = (u8)((validRow & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((validCol & 0x00FF));
  buffer[byteCount++] = (u8)((validCol & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((validHigh & 0x00FF));
  buffer[byteCount++] = (u8)((validHigh & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((validWide & 0x00FF));
  buffer[byteCount++] = (u8)((validWide & 0xFF00) >> 8);

  
       
  buffer[byteCount++] = (u8)len;
  if (len > 0) {
    memcpy(&buffer[byteCount], optArgs, len);
    byteCount += len;
  }

  buffer[byteCount++] = (u8)((bigHigh & 0x00FF));
  buffer[byteCount++] = (u8)((bigHigh & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigWide & 0x00FF));
  buffer[byteCount++] = (u8)((bigWide & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigVertFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((bigVertFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigHorzFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((bigHorzFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 getAcqParams( SLPacketType buffer, u8 type, u8 numCamera, u8 cameraIndex,
                             u8 cameraType, u16 high, u16 wide, u8 bitdepth, u16 vertFrontPorch, u16 horzFrontPorch, u16 flags, u8 frameStep,
                             u16 validRow, u16 validCol, u16 validHigh, u16 validWide, const char *optArgs,
                             u16 bigHigh, u16 bigWide, u16 bigVertFrontPorch, u16 bigHorzFrontPorch)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  u32 len = optArgs ? SLMIN(strlen(optArgs), 255): 0;
  buffer[byteCount++] = 26 + len + 1; // +1 for len byte
  buffer[byteCount++] = type;

  buffer[byteCount++] = numCamera;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = cameraType;
  buffer[byteCount++] = (u8)((high & 0x00FF));
  buffer[byteCount++] = (u8)((high & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((wide & 0x00FF));
  buffer[byteCount++] = (u8)((wide & 0xFF00) >> 8);
  buffer[byteCount++] = bitdepth;
  buffer[byteCount++] = (u8)((vertFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((vertFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((horzFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((horzFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((flags & 0x00FF));
  buffer[byteCount++] = (u8)((flags & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(frameStep);
  buffer[byteCount++] = 0; // reserved
  buffer[byteCount++] = (u8)((validRow & 0x00FF));
  buffer[byteCount++] = (u8)((validRow & 0xFF00)>>8);
  buffer[byteCount++] = (u8)((validCol & 0x00FF));
  buffer[byteCount++] = (u8)((validCol & 0xFF00)>>8);
  buffer[byteCount++] = (u8)((validHigh & 0x00FF));
  buffer[byteCount++] = (u8)((validHigh & 0xFF00)>>8);
  buffer[byteCount++] = (u8)((validWide & 0x00FF));
  buffer[byteCount++] = (u8)((validWide & 0xFF00)>>8);

  buffer[byteCount++] = (u8)len;
  if (len > 0) {
    memcpy(&buffer[byteCount], optArgs, len);
    byteCount += len;
  }

  buffer[byteCount++] = (u8)((bigHigh & 0x00FF));
  buffer[byteCount++] = (u8)((bigHigh & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigWide & 0x00FF));
  buffer[byteCount++] = (u8)((bigWide & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigVertFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((bigVertFrontPorch & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((bigHorzFrontPorch & 0x00FF));
  buffer[byteCount++] = (u8)((bigHorzFrontPorch & 0xFF00) >> 8);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetAcqParams( SLPacketType buffer, u8 cameraIndex, u8 cameraType, 
                        u16 high, u16 wide, u8 bitdepth, u16 vertFrontPorch, u16 horzFrontPorch, u16 flags, u8 frameStep,
                        u16 validRow, u16 validCol, u16 validHigh, u16 validWide, const char *optArgs, u16 bigHigh, u16 bigWide, u16 bitVertFrontPorch, u16 bitHorzFrontPorch)
{
  return setAcqParams( buffer, SetAcqParams, cameraIndex, cameraType, high, wide, bitdepth,vertFrontPorch,horzFrontPorch,flags,frameStep, validRow, validCol,
    validHigh, validWide, optArgs, bigHigh, bigWide, bitVertFrontPorch, bitHorzFrontPorch);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetAcqParams( SLPacketType buffer, u8 cameraIndex )
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = GetAcqParams;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setAdvancedCaptureParams( SLPacketType buffer, u8 type, u16 horzControl, s16 xStart, s16 yStart)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8;
  buffer[byteCount++] = type;

  buffer[byteCount++] = (u8)((horzControl & 0x00FF));
  buffer[byteCount++] = (u8)((horzControl & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((xStart & 0x00FF));
  buffer[byteCount++] = (u8)((xStart & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((yStart & 0x00FF));
  buffer[byteCount++] = (u8)((yStart & 0xFF00) >> 8);
 
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetAdvancedCaptureParams( SLPacketType buffer, u16 horzControl, s16 xStart, s16 yStart )
{
  return setAdvancedCaptureParams( buffer, SetAdvancedCaptureParams, horzControl, xStart, yStart);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetSDCardRecordingParameters(SLPacketType buffer, u8 recordMode, u8 clearFlash, u8 getStatus, u8 getDirectory,
                                      u16 markFrame, u8 recordType, u8 netDispIndex, u8 lblLen, const char *lbl)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11+lblLen;//9+lblLen;
  buffer[byteCount++] = SetSDRecordingParameters;
  buffer[byteCount++] = recordMode;
  buffer[byteCount++] = clearFlash;
  buffer[byteCount++] = getStatus;
  buffer[byteCount++] = getDirectory;
  buffer[byteCount++] = markFrame&0x00FF;
  buffer[byteCount++] = (markFrame&0xFF00)>>8;
  buffer[byteCount++] = recordType;
  buffer[byteCount++] = netDispIndex;
  buffer[byteCount++] = lblLen;
  for(s32 i=0; i<lblLen; i++) 
    buffer[byteCount++] = lbl[i];
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSDCardDirectoryContents(SLPacketType buffer, const char **files, s16 nFiles, s16 startIndex, u16 *endIndex)
{
  u8 fileLens[1024];
  for(s32 i=0; i<nFiles; i++)
    fileLens[i] = strlen(files[i]);

  u8 totalLen = 8;    //5 bytes for other data
  s32 eIdx = startIndex-1;
  while((eIdx+1)<nFiles && (u16)(totalLen+fileLens[(eIdx+1)]+1)<MAX_SLFIP_PAYLOAD) {  //add as many files as possible while length is less than 256
    eIdx++;
    totalLen += fileLens[eIdx]+1; //1 byte for storing file length
  }
  *endIndex = eIdx;

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = totalLen;
  buffer[byteCount++] = CurrentSDCardDirectoryInfo;
  buffer[byteCount++] = nFiles&0xFF;
  buffer[byteCount++] = (nFiles&0xFF00)>>8;
  buffer[byteCount++] = (startIndex&0xFF);
  buffer[byteCount++] = (startIndex&0xFF00)>>8;
  buffer[byteCount++] = eIdx&0xFF;
  buffer[byteCount++] = (eIdx&0xFF00)>>8;

  for(s32 idx=startIndex; idx<=eIdx; idx++)
    buffer[byteCount++] = fileLens[idx];

  for(s32 i=startIndex; i<=eIdx; i++) {
    for(s32 ii=0; ii<fileLens[i]; ii++)
      buffer[byteCount++] = files[i][ii];
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPExSDCardDirectoryContents(SLPacketType buffer, const char **files, s16 nFiles, s16 startIndex, u16 *endIndex, u32 maxBufLen)
{
  u8 fileLens[1024];
  for(s32 i=0; i<nFiles; i++)
    fileLens[i] = strlen(files[i]);

  u32 totalLen = 6;    //5 bytes for other data
  s32 eIdx = startIndex-1;
  while((eIdx+1)<nFiles && (u16)(totalLen+fileLens[(eIdx+1)]+6+1)<maxBufLen) {  //add as many files as possible while length is less than maxBufLen
    eIdx++;
    totalLen += fileLens[eIdx]+1; //1 byte for storing string length
  }
  *endIndex = eIdx;

  u32 byteCount = SLFIPAddHeader(&buffer[0]);
  s32 lenBytes = SlFipSetLen(&buffer[byteCount], 1+totalLen+1, true); // 1:type,1:cksum.
  byteCount += lenBytes;
  buffer[byteCount++] = CurrentSDCardDirectoryInfo;
  buffer[byteCount++] = nFiles&0xFF;
  buffer[byteCount++] = (nFiles&0xFF00)>>8;
  buffer[byteCount++] = (startIndex&0xFF);
  buffer[byteCount++] = (startIndex&0xFF00)>>8;
  buffer[byteCount++] = eIdx&0xFF;
  buffer[byteCount++] = (eIdx&0xFF00)>>8;

  for(s32 idx=startIndex; idx<=eIdx; idx++)
    buffer[byteCount++] = fileLens[idx];

  for(s32 i=startIndex; i<=eIdx; i++) {
    for(s32 ii=0; ii<fileLens[i]; ii++)
      buffer[byteCount++] = files[i][ii];
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+lenBytes], totalLen+1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendTraceString(SLPacketType buffer, const u8 *str, u8 len)
{
  u16 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = len+2;
  buffer[byteCount++] = SendTraceStr;
  for(s32 i=0; i<len; i++)
    buffer[byteCount++] = str[i];

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSDCardRecordingStatus(SLPacketType buffer, u8 state, u32 length, u32 space)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 0x0B;
  buffer[byteCount++] = CurrentSDCardRecordingStatus;
  buffer[byteCount++] = state;
  buffer[byteCount++] = length&0xFF;
  buffer[byteCount++] = (length&0xFF00)>>8;
  buffer[byteCount++] = (length&0xFF0000)>>16;
  buffer[byteCount++] = (length&0xFF000000)>>24;
  buffer[byteCount++] = space&0xFF;
  buffer[byteCount++] = (space&0xFF00)>>8;
  buffer[byteCount++] = (space&0xFF0000)>>16;
  buffer[byteCount++] = (space&0xFF000000)>>24;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPDirectoryStatisticsReply(SLPacketType buffer, u32 totalSize, u32 spaceUsed)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 0x0A;
  buffer[byteCount++] = DirectoryStatisticsReply;
  
  buffer[byteCount++] = totalSize&0xFF;
  buffer[byteCount++] = (totalSize&0xFF00)>>8;
  buffer[byteCount++] = (totalSize&0xFF0000)>>16;
  buffer[byteCount++] = (totalSize&0xFF000000)>>24;
  
  buffer[byteCount++] = (spaceUsed & 0xFF);
  buffer[byteCount++] = (spaceUsed & 0xFF00) >> 8;
  buffer[byteCount++] = (spaceUsed & 0xFF0000) >> 16;
  buffer[byteCount++] = (spaceUsed & 0xFF000000) >> 24;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetRGB565Conversion(SLPacketType buffer, u16 chromaRot, u8 redScale, u8 greenScale, u8 blueScale)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 0x07;
  buffer[byteCount++] = SetRGB565Conversion;
  buffer[byteCount++] = chromaRot&0xFF;
  buffer[byteCount++] = chromaRot>>8;
  buffer[byteCount++] = redScale;
  buffer[byteCount++] = greenScale;
  buffer[byteCount++] = blueScale;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataValues(SLPacketType buffer, 
                          u16 enables, u64 utcTime, u16 heading, s16 pitch, s16 roll,
                          s32 lat, s32 lon, u16 alt, u16 hfov, u16 vfov,
                          u32 az, s32 el, u32 sensorRoll, s32 dispId)
{
  u32 utcTimel = utcTime&0xFFFFFFFF;
  u32 utcTimeh = utcTime>>32;

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 44;
  buffer[byteCount++] = SetMetadataValues;
  buffer[byteCount++] = enables&0xFF;
  buffer[byteCount++] = (enables&0xFF00)>>8;
  buffer[byteCount++] = utcTimel&0xFF;
  buffer[byteCount++] = (utcTimel&0xFF00)>>8;
  buffer[byteCount++] = (utcTimel&0xFF0000)>>16;
  buffer[byteCount++] = (utcTimel&0xFF000000)>>24;
  buffer[byteCount++] = utcTimeh&0xFF;
  buffer[byteCount++] = (utcTimeh&0xFF00)>>8;
  buffer[byteCount++] = (utcTimeh&0xFF0000)>>16;
  buffer[byteCount++] = (utcTimeh&0xFF000000)>>24;
  buffer[byteCount++] = heading&0xFF;
  buffer[byteCount++] = (heading&0xFF00)>>8;
  buffer[byteCount++] = pitch&0xFF;
  buffer[byteCount++] = (pitch&0xFF00)>>8;
  buffer[byteCount++] = roll&0xFF;
  buffer[byteCount++] = (roll&0xFF00)>>8;
  buffer[byteCount++] = lat&0xFF;
  buffer[byteCount++] = (lat&0xFF00)>>8;
  buffer[byteCount++] = (lat&0xFF0000)>>16;
  buffer[byteCount++] = (lat&0xFF000000)>>24;
  buffer[byteCount++] = lon&0xFF;
  buffer[byteCount++] = (lon&0xFF00)>>8;
  buffer[byteCount++] = (lon&0xFF0000)>>16;
  buffer[byteCount++] = (lon&0xFF000000)>>24;
  buffer[byteCount++] = alt&0xFF;
  buffer[byteCount++] = (alt&0xFF00)>>8;
  buffer[byteCount++] = hfov&0xFF;
  buffer[byteCount++] = (hfov&0xFF00)>>8;
  buffer[byteCount++] = vfov&0xFF;
  buffer[byteCount++] = (vfov&0xFF00)>>8;
  buffer[byteCount++] = az&0xFF;
  buffer[byteCount++] = (az&0xFF00)>>8;
  buffer[byteCount++] = (az&0xFF0000)>>16;
  buffer[byteCount++] = (az&0xFF000000)>>24;
  buffer[byteCount++] = el&0xFF;
  buffer[byteCount++] = (el&0xFF00)>>8;
  buffer[byteCount++] = (el&0xFF0000)>>16;
  buffer[byteCount++] = (el&0xFF000000)>>24;
  buffer[byteCount++] = sensorRoll&0xFF;
  buffer[byteCount++] = (sensorRoll&0xFF00)>>8;
  buffer[byteCount++] = (sensorRoll&0xFF0000)>>16;
  buffer[byteCount++] = (sensorRoll&0xFF000000)>>24;
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataStaticValues(SLPacketType buffer, 
                                u8 type, u8 len, char *str, s32 dispId)
{
  int i;
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4+len;
  buffer[byteCount++] = MetadataStaticValues;
  buffer[byteCount++] = type;
  buffer[byteCount++] = len;
  for(i=0;i<len;i++)
    buffer[byteCount+i] = str[i];
  byteCount += len;
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setMFV(SLPacketType buffer, 
                  s32 frameCenterLat, s32 frameCenterLon, u16 frameCenterEl, u16 frameWidth, u32 slantRange)
{
  s32 byteCount = 0;
  buffer[byteCount++] = frameCenterLat&0xFF;
  buffer[byteCount++] = (frameCenterLat&0xFF00)>>8;
  buffer[byteCount++] = (frameCenterLat&0xFF0000)>>16;
  buffer[byteCount++] = (frameCenterLat&0xFF000000)>>24;
  buffer[byteCount++] = frameCenterLon&0xFF;
  buffer[byteCount++] = (frameCenterLon&0xFF00)>>8;
  buffer[byteCount++] = (frameCenterLon&0xFF0000)>>16;
  buffer[byteCount++] = (frameCenterLon&0xFF000000)>>24;
  buffer[byteCount++] = frameCenterEl&0xFF;
  buffer[byteCount++] = (frameCenterEl&0xFF00)>>8;
  buffer[byteCount++] = frameWidth&0xFF;
  buffer[byteCount++] = (frameWidth&0xFF00)>>8;
  buffer[byteCount++] = slantRange&0xFF;
  buffer[byteCount++] = (slantRange&0xFF00)>>8;
  buffer[byteCount++] = (slantRange&0xFF0000)>>16;
  buffer[byteCount++] = (slantRange&0xFF000000)>>24;

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setMFV2(SLPacketType buffer, 
                   s32 targetLat, s32 targetLon, u16 targetEl, u8 targetHeight, u8 targetWidth)
{
  s32 byteCount = 0;
  buffer[byteCount++] = targetLat&0xFF;
  buffer[byteCount++] = (targetLat&0xFF00)>>8;
  buffer[byteCount++] = (targetLat&0xFF0000)>>16;
  buffer[byteCount++] = (targetLat&0xFF000000)>>24;
  buffer[byteCount++] = targetLon&0xFF;
  buffer[byteCount++] = (targetLon&0xFF00)>>8;
  buffer[byteCount++] = (targetLon&0xFF0000)>>16;
  buffer[byteCount++] = (targetLon&0xFF000000)>>24;
  buffer[byteCount++] = targetEl&0xFF;
  buffer[byteCount++] = (targetEl&0xFF00)>>8;
  buffer[byteCount++] = targetHeight;
  buffer[byteCount++] = targetWidth;

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setMFV3(SLPacketType buffer, 
                    s16 offsetCornerLat1, s16 offsetCornerLon1, s16 offsetCornerLat2, s16 offsetCornerLon2,
                    s16 offsetCornerLat3, s16 offsetCornerLon3, s16 offsetCornerLat4, s16 offsetCornerLon4)
{
  s32 byteCount = 0;
  buffer[byteCount++] = offsetCornerLat1&0xFF;
  buffer[byteCount++] = (offsetCornerLat1&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLon1&0xFF;
  buffer[byteCount++] = (offsetCornerLon1&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLat2&0xFF;
  buffer[byteCount++] = (offsetCornerLat2&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLon2&0xFF;
  buffer[byteCount++] = (offsetCornerLon2&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLat3&0xFF;
  buffer[byteCount++] = (offsetCornerLat3&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLon3&0xFF;
  buffer[byteCount++] = (offsetCornerLon3&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLat4&0xFF;
  buffer[byteCount++] = (offsetCornerLat4&0xFF00)>>8;
  buffer[byteCount++] = offsetCornerLon4&0xFF;
  buffer[byteCount++] = (offsetCornerLon4&0xFF00)>>8;

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataFrameValues(SLPacketType buffer, 
                                u16 enables, s32 frameCenterLat, s32 frameCenterLon, u16 frameCenterEl, u16 frameWidth, u32 slantRange)
{

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 20;
  buffer[byteCount++] = SetMetadataFrameValues;
  buffer[byteCount++] = enables&0xFF;
  buffer[byteCount++] = (enables&0xFF00)>>8;
  byteCount += setMFV(&buffer[byteCount], frameCenterLat, frameCenterLon, frameCenterEl, frameWidth, slantRange);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataFrameValues(SLPacketType buffer, 
                          u16 enables, s32 frameCenterLat, s32 frameCenterLon, u16 frameCenterEl, u16 frameWidth, u32 slantRange,
                          u8 userSuppliedFlags)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 21;
  buffer[byteCount++] = SetMetadataFrameValues;
  buffer[byteCount++] = enables&0xFF;
  buffer[byteCount++] = (enables&0xFF00)>>8;
  byteCount += setMFV(&buffer[byteCount], frameCenterLat, frameCenterLon, frameCenterEl, frameWidth, slantRange);
  buffer[byteCount++] = userSuppliedFlags;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataFrameValues(SLPacketType buffer, 
                          u16 enables, s32 frameCenterLat, s32 frameCenterLon, u16 frameCenterEl, u16 frameWidth, u32 slantRange,
                          u8 userSuppliedFlags, s32 targetLat, s32 targetLon, u16 targetEl, u8 targetHeight, u8 targetWidth)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 33;
  buffer[byteCount++] = SetMetadataFrameValues;
  buffer[byteCount++] = enables&0xFF;
  buffer[byteCount++] = (enables&0xFF00)>>8;
  byteCount += setMFV(&buffer[byteCount], frameCenterLat, frameCenterLon, frameCenterEl, frameWidth, slantRange);
  buffer[byteCount++] = userSuppliedFlags;
  byteCount += setMFV2(&buffer[byteCount], targetLat, targetLon, targetEl, targetHeight, targetWidth);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataFrameValues(SLPacketType buffer, 
                          u16 enables, s32 frameCenterLat, s32 frameCenterLon, u16 frameCenterEl, u16 frameWidth, u32 slantRange,
                          u8 userSuppliedFlags, s32 targetLat, s32 targetLon, u16 targetEl, u8 targetHeight, u8 targetWidth,
                          s16 offsetCornerLat1, s16 offsetCornerLon1, s16 offsetCornerLat2, s16 offsetCornerLon2,
                          s16 offsetCornerLat3, s16 offsetCornerLon3, s16 offsetCornerLat4, s16 offsetCornerLon4, s32 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 49;
  buffer[byteCount++] = SetMetadataFrameValues;
  buffer[byteCount++] = enables&0xFF;
  buffer[byteCount++] = (enables&0xFF00)>>8;
  byteCount += setMFV(&buffer[byteCount], frameCenterLat, frameCenterLon, frameCenterEl, frameWidth, slantRange);
  buffer[byteCount++] = userSuppliedFlags;
  byteCount += setMFV2(&buffer[byteCount], targetLat, targetLon, targetEl, targetHeight, targetWidth);
  byteCount += setMFV3(&buffer[byteCount], offsetCornerLat1, offsetCornerLon1, offsetCornerLat2, offsetCornerLon2,
                       offsetCornerLat3, offsetCornerLon3, offsetCornerLat4, offsetCornerLon4);
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetMetadataRate(SLPacketType buffer, u64 enables, u8 frameStep, s32 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11;
  buffer[byteCount++] = SetMetadataRate;
  buffer[byteCount++] = (u8)(enables&0xFF);
  buffer[byteCount++] = (u8)((enables&0xFF00)>>8);
  buffer[byteCount++] = (u8)((enables&0xFF0000)>>16);
  buffer[byteCount++] = (u8)((enables&0xFF000000)>>24);
  buffer[byteCount++] = (u8)((enables&0xFF00000000ll)>>32);
  buffer[byteCount++] = (u8)((enables&0xFF0000000000ll)>>40);
  buffer[byteCount++] = (u8)((enables&0xFF000000000000ll)>>48);
  buffer[byteCount++] = (u8)((enables&0xFF00000000000000ll)>>56);
  buffer[byteCount++] = frameStep;
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDisplayAdjustments(SLPacketType buffer, s16 ratio8, s16 pan, s16 tilt)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8;
  buffer[byteCount++] = SetDisplayAdjustments;
  buffer[byteCount++] = ratio8&0xFF;
  buffer[byteCount++] = (ratio8&0xFF00)>>8;
  buffer[byteCount++] = pan&0xFF;
  buffer[byteCount++] = (pan&0xFF00)>>8;
  buffer[byteCount++] = tilt&0xFF;
  buffer[byteCount++] = (tilt&0xFF00)>>8;

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetDisplayAdjustments(SLPacketType buffer, s16 ratio8, s16 pan, s16 tilt, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 9;
  buffer[byteCount++] = SetDisplayAdjustments;
  buffer[byteCount++] = ratio8&0xFF;
  buffer[byteCount++] = (ratio8&0xFF00)>>8;
  buffer[byteCount++] = pan&0xFF;
  buffer[byteCount++] = (pan&0xFF00)>>8;
  buffer[byteCount++] = tilt&0xFF;
  buffer[byteCount++] = (tilt&0xFF00)>>8;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// propertyFlags:
//  0x01: 0 = Coordinates are specified in stabilized image space, 1 = Coordiantes are specified in source image space.
//  0x02: 0 = Fix coordinates of object, 1 = Move object with camera motion
//  0x04: 0 = ..., 1=static object (doesn't move at all)
//  0x80: 0 = Origin is the center of the image, 1 = Origin is the upper-left corner of the image.
s32 SLFIPDrawObject(SLPacketType buffer, u8 objId, u8 action, u8 propertyFlags, u8 type, void *params, u8 backgroundColor, u8 foregroundColor)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 15;
  buffer[byteCount++] = DrawObject;
  buffer[byteCount++] = objId;
  buffer[byteCount++] = action;
  buffer[byteCount++] = propertyFlags; 
  buffer[byteCount++] = type;

  u16 a=0, b=0, c=0, d=0;

  SL_CIRCLE_DRAW_PARAMS *cdp=0;
  SL_RECT_DRAW_PARAMS *rdp=0;
  SL_LINE_DRAW_PARAMS *ldp=0;
  SL_TEXT_DRAW_PARAMS *tdp=0;
  SL_TEXTEX_DRAW_PARAMS *txdp=0;
  SL_CUSTOM_DRAW_PARAMS *mdp=0;
  SL_KLV_DRAW_PARAMS *klvp=0;
  if(action) {
    switch((SLFIP_DRAW_OBJECT_TYPE)type) {
      case SLFIP_DO_CIRCLE:
      case SLFIP_DO_CIRCLE_FILL:
        cdp = (SL_CIRCLE_DRAW_PARAMS *)params;
        a = cdp->centerC;
        b = cdp->centerR;
        c = cdp->radius;
        d = 0;
        break;
      case SLFIP_DO_RECT:
      case SLFIP_DO_RECT_FILL:
        rdp = (SL_RECT_DRAW_PARAMS *)params;
        a = rdp->ulC;
        b = rdp->ulR;
        c = rdp->wide;
        d = rdp->high;
        break;
      default:
        //SLDebugAssert(0);
        // fall through...
      case SLFIP_DO_LINE:
        ldp = (SL_LINE_DRAW_PARAMS *)params;
        a = ldp->p1C;
        b = ldp->p1R;
        c = ldp->p2C;
        d = ldp->p2R;
        break;
      case SLFIP_DO_TEXT:
        tdp = (SL_TEXT_DRAW_PARAMS *)params;
        a = tdp->col;
        b = tdp->row;
        c = tdp->len;
        d = 0;
        break;
      case SLFIP_DO_TEXT_EX:
        txdp = (SL_TEXTEX_DRAW_PARAMS *)params;
        a = txdp->col;
        b = txdp->row;
        c = (txdp->xscale5) | ((u32)txdp->yscale5 << 8);
        d = (txdp->fontId & 0xff) | (txdp->fontWidth << 8);
        break;
      case SLFIP_DO_KLV_FIELD:
        klvp = (SL_KLV_DRAW_PARAMS *)params;
        a = klvp->col;
        b = klvp->row;
        c = (klvp->scale5) | ((u32)klvp->fontId << 8);
        d = (klvp->klvField & 0xff) | (u32)(klvp->klvFormat << 8);
        break;
      case SLFIP_DO_CUSTOM_0:
      case SLFIP_DO_CUSTOM_1:
      case SLFIP_DO_CUSTOM_2:
      case SLFIP_DO_CUSTOM_3:
      case SLFIP_DO_CUSTOM_4:
      case SLFIP_DO_CUSTOM_5:
      case SLFIP_DO_CUSTOM_6:
      case SLFIP_DO_CUSTOM_7:
        mdp = (SL_CUSTOM_DRAW_PARAMS *)params;
        a = mdp->col;
        b = mdp->row;
        c = (mdp->xscale5) | (mdp->yscale5 << 8);
        d = mdp->angle5;
        break;  
    }
  }
  buffer[byteCount++] = a&0xFF;
  buffer[byteCount++] = (a>>8);
  buffer[byteCount++] = b&0xFF;
  buffer[byteCount++] = (b>>8);
  buffer[byteCount++] = c&0xFF;
  buffer[byteCount++] = (c>>8);
  buffer[byteCount++] = d&0xFF;
  buffer[byteCount++] = (d>>8);
  buffer[byteCount++] = (backgroundColor&0xF)|((foregroundColor&0xF)<<4);

  if((SLFIP_DRAW_OBJECT_TYPE)type == SLFIP_DO_TEXT) {
    if(tdp && tdp->len<=MAX_TEXT_LEN) {
      memcpy(buffer+byteCount, tdp->text, tdp->len);
      byteCount += tdp->len;
      buffer[SLFIP_OFFSET_LENGTH] = byteCount - SLFIP_OFFSET_LENGTH;
    }
  }
  else if((SLFIP_DRAW_OBJECT_TYPE)type == SLFIP_DO_TEXT_EX) {
    if(txdp && txdp->len<=MAX_TEXT_LEN) {
      memcpy(buffer+byteCount, txdp->text, txdp->len);
      byteCount += txdp->len;
      buffer[SLFIP_OFFSET_LENGTH] = byteCount - SLFIP_OFFSET_LENGTH;
    }
  }
  else if((SLFIP_DRAW_OBJECT_TYPE)type == SLFIP_DO_KLV_FIELD) {
    if(klvp && klvp->len<MAX_TEXT_LEN) {
      memcpy(buffer+byteCount, klvp->text, klvp->len);
      byteCount += klvp->len;
      buffer[SLFIP_OFFSET_LENGTH] = byteCount - SLFIP_OFFSET_LENGTH;
    }
  }

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

u8 createPropertyFlags(u8 sourceCoords, u8 moveWithCamera, u8 staticCoordinates, u8 upperLeftOrigin, u8 save)
{
  // TODO: some of these properties may be exlusive or with other properties
  u8 propertyFlags = 0;  //!< deaults defined by documentation / expected behavior
  propertyFlags  = ( (sourceCoords > 0) ? SL_DRAW_FLAGS_SOURCE_COORDS : 0);
  propertyFlags |= ( (moveWithCamera > 0) ? SL_DRAW_FLAGS_MOVE_WITH_CAMERA : 0);
  propertyFlags |= ( (staticCoordinates > 0) ? SL_DRAW_FLAGS_STATIC : 0);
  propertyFlags |= ( (save > 0) ? SL_DRAW_FLAGS_SAVE : 0);
  propertyFlags |= ( (upperLeftOrigin > 0) ? SL_DRAW_FLAGS_ORIGIN_UPPER_LEFT : 0);
  return propertyFlags;
}
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPDrawObject(SLPacketType buffer, u8 objId, u8 action, u8 sourceCoords, u8 moveWithCamera, u8 staticCoordinates, u8 upperLeftOrigin, u8 type, void *params, u8 backgroundColor, u8 foregroundColor, u8 save)
{
  return SLFIPDrawObject(buffer, objId, action, createPropertyFlags(sourceCoords, moveWithCamera, staticCoordinates, upperLeftOrigin, save), 
    type, params, backgroundColor, foregroundColor);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPMTISetParameters(SLPacketType buffer, u8 debugLog, u8 thresh, u8 threshMode, u8 runningRate8, u8 viewRunningImage, u8 postProcessPeaks, u8 freakoutDetect)
{
  u8 byteCount = SLFIPAddMTIHeader(&buffer[0]);
  buffer[byteCount++] = 9;
  buffer[byteCount++] = SendMTIConfig;
  buffer[byteCount++] = debugLog;
  buffer[byteCount++] = thresh;
  buffer[byteCount++] = threshMode;
  buffer[byteCount++] = runningRate8;
  buffer[byteCount++] = viewRunningImage;
  buffer[byteCount++] = postProcessPeaks;
  buffer[byteCount++] = freakoutDetect;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 setH264Params(SLPacketType buffer, u8 type, SLH264Params *params, s32 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11;
  buffer[byteCount++] = type;
  byteCount = write4(buffer, byteCount, params->targetBitRate);
  buffer[byteCount++] = params->intraFrameInterval;
  buffer[byteCount++] = params->lfDisableIdc;
  buffer[byteCount++] = params->airMbPeriod;
  buffer[byteCount++] = params->sliceRefreshRowNumber;
  buffer[byteCount++] = params->flags;
  if (dispId) {
    buffer[SLFIP_OFFSET_LENGTH] += 2;
    buffer[byteCount++] = (u8)dispId;
    buffer[byteCount++] = (u8)(dispId >> 8);
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetH264Params(SLPacketType buffer, SLH264Params *params, s32 dispId)
{
  return setH264Params(buffer, SetH264Parameters, params, dispId);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPGetH264Params(SLPacketType buffer )
{
  return simplePack(buffer, GetH264Parameters);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetSnapShot(SLPacketType buffer, SLSNAP_MODE mode, SLSnapShotFormat format, SLSnapShotSource source, u8 quality, u8 downSample,
                     u32 ipAddr, u32 port, const char *userName, const char *password)
{
  u8 uLen = userName ? strlen(userName) : 0;
  u8 pLen = password ? strlen(password) : 0;

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 15 + uLen + pLen;
  buffer[byteCount++] = SetSnapShot;

  buffer[byteCount++] = mode; // MicroSD, Network, etc.
  buffer[byteCount++] = format; // JPEG, etc
  buffer[byteCount++] = source;
  buffer[byteCount++] = quality;
  buffer[byteCount++] = downSample;
  buffer[byteCount++] = (u8)((ipAddr & 0xFF000000)>>24);
  buffer[byteCount++] = (u8)((ipAddr & 0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((ipAddr & 0x0000FF00)>> 8);
  buffer[byteCount++] = (u8)((ipAddr & 0x000000FF));
  buffer[byteCount++] = (u8)((port & 0xFF00)>> 8);
  buffer[byteCount++] = (u8)((port & 0x00FF));

  buffer[byteCount++] = uLen;
  for(s32 idx=0; idx < uLen; idx++)
    buffer[byteCount++] = userName[idx];

  buffer[byteCount++] = pLen;
  for(s32 idx=0; idx < pLen; idx++)
    buffer[byteCount++] = password[idx];

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPDoSnapShot(SLPacketType buffer, u8 frameStep, u8 numFrames, const char * fileName, u8 snapAllCamerasMask, u8 resumeCount)
{
  u8 bLen = strlen(fileName);

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7 + bLen;
  buffer[byteCount++] = DoSnapShot;

  buffer[byteCount++] = frameStep;
  buffer[byteCount++] = numFrames;

  buffer[byteCount++] = bLen;
  for(s32 idx=0; idx < bLen; idx++)
    buffer[byteCount++] = fileName[idx];

  // this first one (snapAllCamerasMask) used to be optional (variable length) but that's not possible with another parameter after it.
  buffer[byteCount++] = snapAllCamerasMask;
  buffer[byteCount++] = resumeCount;


  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendZoom(SLPacketType buffer, u8 in)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3;
  buffer[byteCount++] = 0x69;
  buffer[byteCount++] = in;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendShutter(SLPacketType buffer, u8 up, u8 reset)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4;
  buffer[byteCount++] = 0x6A;
  buffer[byteCount++] = up;
  buffer[byteCount++] = reset;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Wrap binary KLV data in FIP packet.
// - Length can be 2 bytes.
// - PKT: 0x61 0xAC Len (Len) Type Rsvd[0] Rsvd[1] Klv[0] ... Klv[end] Chksum
s32 SLFIPEXBinaryKlv(FIPEX_PACKET *fpx, const u8 *klvData, u32 klvLen, s32 dispId)
{
  u32 byteCount = SLFIPAddHeader(&fpx->bytes[0]);
  s32 lenBytes = SlFipSetLen(&fpx->bytes[byteCount], klvLen+1+2+1, true); // 1:type,2:rsvd,1:cksum.
  byteCount += lenBytes;
  fpx->bytes[byteCount++] = SetKlvData; // Type:
  fpx->bytes[byteCount++] = (u8)dispId;
  fpx->bytes[byteCount++] = (u8)(dispId >> 8);
  memcpy(&fpx->bytes[byteCount], klvData, klvLen);
  byteCount += klvLen;
  fpx->bytes[byteCount++] = SLComputeFIPChecksum(&fpx->bytes[SLFIP_OFFSET_LENGTH+lenBytes], klvLen+1+2); // 1:type,2:rsvd
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetLensMode(SLPacketType buffer, SLLensMode lensMode, u8* data, u16 dataLen)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 3+dataLen;
  buffer[byteCount++] = SetLensMode; // command ID
  buffer[byteCount++] = lensMode;
  for(int i=0; i<dataLen;i++)
  {
    buffer[byteCount++] = *data++;
  }
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetLensParams(SLPacketType buffer, u8 lensType, u8 AFMetricRegionSize,
                                   u8 zoomTrackFocus, u8 autofocusMethod, u16 autofocusRateAdjust,
                                   u8 autofocusChangePct, u8 zoomSpeed, u8 focusSpeed, u8 comPortNum)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 12;
  buffer[byteCount++] = SetLensParams; // command ID
  buffer[byteCount++] = lensType;
  buffer[byteCount++] = AFMetricRegionSize;
  buffer[byteCount++] = zoomTrackFocus;
  buffer[byteCount++] = autofocusMethod;
  buffer[byteCount++] = autofocusRateAdjust&0xFF;
  buffer[byteCount++] = (autofocusRateAdjust&0xFF00)>>8;
  buffer[byteCount++] = autofocusChangePct&0xFF;
  buffer[byteCount++] = zoomSpeed;
  buffer[byteCount++] = focusSpeed;
  buffer[byteCount++] = comPortNum;
 
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPAnalyzeRenderSync(SLPacketType buffer, SLSyncType type, u8 idx, u64 ts)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 11;
  buffer[byteCount++] = AnalyzeRenderSync; // command ID
  buffer[byteCount++] = type | (idx<<4);
  buffer[byteCount++] = (ts>>0)&0xFF;
  buffer[byteCount++] = (ts>>8)&0xFF;
  buffer[byteCount++] = (ts>>16)&0xFF;
  buffer[byteCount++] = (ts>>24)&0xFF;
  buffer[byteCount++] = (ts>>32)&0xFF;
  buffer[byteCount++] = (ts>>40)&0xFF;
  buffer[byteCount++] = (ts>>48)&0xFF;
  buffer[byteCount++] = (ts>>56)&0xFF;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
static s32 sendUserWarningMessage(SLPacketType buffer, SLUserWarnings level, const u8 *str, u32 len, bool fipEx)
{
  u32 byteCount = SLFIPAddHeader(&buffer[0]);
  u32 plen = 1+2+len+1; // ID+level+str+cksum
  s32 lenBytes = SlFipSetLen(&buffer[byteCount], plen, fipEx);
  byteCount += lenBytes;
  buffer[byteCount++] = UserWarningMessage; // command ID
  buffer[byteCount++] = (u8)level;
  buffer[byteCount++] = (u8)(level>>8);
  memcpy(buffer+byteCount, str, len);
  byteCount+=len;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+lenBytes], plen-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// string length is truncated to 123 characters including the null terminate char
s32 SLFIPSendUserWarningMessage(SLPacketType buffer, SLUserWarnings level, const char *str)
{
  u32 len = strlen(str) + 1;  // +1 for the null-terminator.
  if (len > MAX_SLFIPEX_BYTEMASK - 5)
    len = MAX_SLFIPEX_BYTEMASK - 5; // Chop the message length to 123 bytes.
  return sendUserWarningMessage(buffer, level, (u8*)str, len, false);
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendDetailedTimingMessageEx(SLPacketType buffer, s16 reserved, const u8 *str, u32 len)
{
  u32 byteCount = SLFIPAddHeader(&buffer[0]);
  u32 plen = 1+2+len+1; // ID+reserved+str+cksum
  bool fipEx = true;
  s32 lenBytes = SlFipSetLen(&buffer[byteCount], plen, fipEx);
  byteCount += lenBytes;
  buffer[byteCount++] = DetailedTimingMessage; // command ID
  buffer[byteCount++] = (u8)reserved;
  buffer[byteCount++] = (u8)(reserved>>8);
  memcpy(buffer+byteCount, str, len);
  byteCount+=len;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_LENGTH+lenBytes], plen-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetUserWarningLevel(SLPacketType buffer, SLUserWarnings warningLevelBits)
{
  //SLDebugAssert((warningLevelBits & ~SL_USER_WARN_ALL) == 0); // Caller should not use any bits not defined for ALL -- can't use SLDebugAssert!
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4;
  buffer[byteCount++] = UserWarningLevel;
  buffer[byteCount++] = (warningLevelBits>>0)&0xFF;
  buffer[byteCount++] = (warningLevelBits>>8)&0xFF;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSendSystemStatusMessage(SLPacketType buffer, u64 errorFlags, s16 temperature, u8 load0, u8 load1, u8 load2, u8 load3)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 20;
  buffer[byteCount++] = SystemStatusMessage;
  int i;
  for(i=0;i<8;i++){
    buffer[byteCount++] = (u8)(errorFlags>>(i*8));
  }
  buffer[byteCount++] = (u8)(temperature & 0x00FF);
  buffer[byteCount++] = (u8)((temperature & 0xFF00) >> 8);
  buffer[byteCount++] = load0;
  buffer[byteCount++] = load1;
  buffer[byteCount++] = load2;
  buffer[byteCount++] = load3;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = 0;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetSystemStatusMode(SLPacketType buffer, u16 systemStatusBits, u32 systemDebugBits)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8;
  buffer[byteCount++] = SystemStatusMode;
  buffer[byteCount++] = (systemStatusBits>>0)&0xFF;
  buffer[byteCount++] = (systemStatusBits>>8)&0xFF;
  buffer[byteCount++] = (systemDebugBits>>0)&0xFF;
  buffer[byteCount++] = (systemDebugBits>>8)&0xFF;
  buffer[byteCount++] = (systemDebugBits>>16)&0xFF;
  buffer[byteCount++] = (systemDebugBits>>24)&0xFF;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPLandingAid(SLPacketType buffer, u8 mode, u16 camHFovDeg8, u32 blackTargetSize16, u32 whiteTargetSize16,
                    u8 matchThresh, u8 keepOutMode, u32 keepOutRadius16,
                    u16 ctrlParam0, u16 ctrlParam1, u16 ctrlParam2, u16 ctrlParam3)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 30;
  buffer[byteCount++] = LandingAid; // command ID
  buffer[byteCount++] = mode;
  buffer[byteCount++] = (camHFovDeg8>>0)&0xFF;
  buffer[byteCount++] = (camHFovDeg8>>8)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>0)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>8)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>16)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>24)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>0)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>8)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>16)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>24)&0xFF;
  buffer[byteCount++] = matchThresh;
  buffer[byteCount++] = 0;//reserved - lStepBase;
  buffer[byteCount++] = 0;//reserved - lStepTall;
  buffer[byteCount++] = keepOutMode;
  buffer[byteCount++] = 0;//reserved - 
  buffer[byteCount++] = (keepOutRadius16>>0)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>8)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>16)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>24)&0xFF;
  buffer[byteCount++] = (ctrlParam0>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam0>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>8)&0xFF;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPLandingAid(SLPacketType buffer, u8 mode, u16 camHFovDeg8, u32 blackTargetSize16, u32 whiteTargetSize16,
                    u8 matchThresh, u8 keepOutMode, u32 keepOutRadius16,
                    u16 ctrlParam0, u16 ctrlParam1, u16 ctrlParam2, u16 ctrlParam3, u8 cameraIndex)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 31;
  buffer[byteCount++] = LandingAid; // command ID
  buffer[byteCount++] = mode;
  buffer[byteCount++] = (camHFovDeg8>>0)&0xFF;
  buffer[byteCount++] = (camHFovDeg8>>8)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>0)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>8)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>16)&0xFF;
  buffer[byteCount++] = (blackTargetSize16>>24)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>0)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>8)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>16)&0xFF;
  buffer[byteCount++] = (whiteTargetSize16>>24)&0xFF;
  buffer[byteCount++] = matchThresh;
  buffer[byteCount++] = 0;//reserved - lStepBase;
  buffer[byteCount++] = 0;//reserved - lStepTall;
  buffer[byteCount++] = keepOutMode;
  buffer[byteCount++] = 0;//reserved - 
  buffer[byteCount++] = (keepOutRadius16>>0)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>8)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>16)&0xFF;
  buffer[byteCount++] = (keepOutRadius16>>24)&0xFF;
  buffer[byteCount++] = (ctrlParam0>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam0>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>8)&0xFF;
  buffer[byteCount++] = cameraIndex;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

#define SETNOT0(res, val) (*res = (val!=0) ? val : *res)
///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackLandingAid(u8 *mode, u16 *camHFovDeg8, u32 *blackTargetSize16, u32 *whiteTargetSize16,
                          u8 *matchThresh, u8 *keepOutMode, u32 *keepOutRadius16,
                          u16 *ctrlParam0, u16 *ctrlParam1, u16 *ctrlParam2, u16 *ctrlParam3, const SLPacketType buffer)
{
  s32 extraLenBytes = 0;
  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);
  byteCount += 1+extraLenBytes;
  u32 type = buffer[byteCount++];
  if(type != LandingAid)
    return -1;

  *mode = buffer[byteCount]; byteCount++;
  SETNOT0(camHFovDeg8,       toU16(&buffer[byteCount])); byteCount+=2;
  SETNOT0(blackTargetSize16, toU32(&buffer[byteCount])); byteCount+=4;
  SETNOT0(whiteTargetSize16, toU32(&buffer[byteCount])); byteCount+=4;
  SETNOT0(matchThresh,       buffer[byteCount]);         byteCount++;
  //reserved SETNOT0(lStepBase,         buffer[byteCount]);
  byteCount++; // Still add for reserved bytes
  //reserved SETNOT0(lStepTall,         buffer[byteCount]);         
  byteCount++; // Still add for reserved bytes
  
  if(len>=30){
    *keepOutMode     = buffer[byteCount]; byteCount++;
    byteCount++; // reserved byte
    *keepOutRadius16 = toU32(&buffer[byteCount]); byteCount+=4;  
    *ctrlParam0      = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam1      = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam2      = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam3      = toU16(&buffer[byteCount]); byteCount+=2;
  }

  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPLandingPosition(SLPacketType buffer, u8 camIdx, s16 col, s16 row, u16 angleDeg7, u32 distance16, u8 confidence, u16 camHFovDeg8, 
                         s16 capWide, s16 capHigh, u16 ctrlParam0, u16 ctrlParam1, u16 ctrlParam2, u16 ctrlParam3, 
                         u8 keepOutState, u8 keepOutConfidence, u16 keepOutSz, u32 keepOutDist16,
                         u16 flags, u32 frameId, u64 timeStamp)
{
  s32 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 36;
  buffer[byteCount++] = LandingPosition;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = (u8)(col & 0x00FF);
  buffer[byteCount++] = (u8)((col & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(row & 0x00FF);
  buffer[byteCount++] = (u8)((row & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(angleDeg7 & 0x00FF);
  buffer[byteCount++] = (u8)((angleDeg7 & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)((distance16&0x000000FF));
  buffer[byteCount++] = (u8)((distance16&0x0000FF00)>>8);
  buffer[byteCount++] = (u8)((distance16&0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((distance16&0xFF000000)>>24);
  buffer[byteCount++] = confidence;
  buffer[byteCount++] = (camHFovDeg8>>0)&0xFF;
  buffer[byteCount++] = (camHFovDeg8>>8)&0xFF;
  buffer[byteCount++] = (u8)(capWide & 0x00FF);
  buffer[byteCount++] = (u8)((capWide & 0xFF00) >> 8);
  buffer[byteCount++] = (u8)(capHigh & 0x00FF);
  buffer[byteCount++] = (u8)((capHigh & 0xFF00) >> 8);
  buffer[byteCount++] = (ctrlParam0>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam0>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam1>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam2>>8)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>0)&0xFF;
  buffer[byteCount++] = (ctrlParam3>>8)&0xFF;
  buffer[byteCount++] = keepOutState;
  buffer[byteCount++] = keepOutConfidence;
  buffer[byteCount++] = (keepOutSz>>0)&0xFF;
  buffer[byteCount++] = (keepOutSz>>8)&0xFF;
  buffer[byteCount++] = (u8)((keepOutDist16&0x000000FF));
  buffer[byteCount++] = (u8)((keepOutDist16&0x0000FF00)>>8);
  buffer[byteCount++] = (u8)((keepOutDist16&0x00FF0000)>>16);
  buffer[byteCount++] = (u8)((keepOutDist16&0xFF000000)>>24);

  buffer[SLFIP_OFFSET_LENGTH]+=addAuxTelem(buffer, &byteCount, flags, frameId, timeStamp);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPUnpackLandingPosition(SLPacketType buffer, u8 *camIdx, s16 *col, s16 *row, u16 *angleDeg7, u32 *distance16, u8 *confidence,
                               u16* camHFovDeg8, s16* capWide, s16* capHigh, u16* ctrlParam0, u16* ctrlParam1, u16* ctrlParam2, u16* ctrlParam3,    
                               u8* keepOutState, u8* keepOutConfidence, u16* keepOutSz, u32* keepOutDist16,
                               u64 *timeStamp, u32 *frameIdx)
{
  s32 extraLenBytes = 0;
  s32 byteCount = SLFIP_OFFSET_LENGTH;
  s32 len = (s32)SlFipGetPktLen(&buffer[byteCount], &extraLenBytes, true);
  byteCount += 1+extraLenBytes;
  if(buffer[byteCount++] != LandingPosition)
    return -1;

  *camIdx     = buffer[byteCount]; byteCount++;
  *col        = toS16(&buffer[byteCount]); byteCount+=2;
  *row        = toS16(&buffer[byteCount]); byteCount+=2;
  *angleDeg7  = toU16(&buffer[byteCount]); byteCount+=2;
  *distance16 = toU32(&buffer[byteCount]); byteCount+=4;
  *confidence = buffer[byteCount]; byteCount++;

  if(len>=36){
    *camHFovDeg8       = toU16(&buffer[byteCount]); byteCount+=2;
    *capWide           = toS16(&buffer[byteCount]); byteCount+=2;
    *capHigh           = toS16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam0        = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam1        = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam2        = toU16(&buffer[byteCount]); byteCount+=2;
    *ctrlParam3        = toU16(&buffer[byteCount]); byteCount+=2;
    *keepOutState      = buffer[byteCount]; byteCount++;
    *keepOutConfidence = buffer[byteCount]; byteCount++;
    *keepOutSz         = toU16(&buffer[byteCount]); byteCount+=2;
    *keepOutDist16     = toU32(&buffer[byteCount]); byteCount+=4;
  }
  if (len >= 48){
    if( timeStamp )
      *timeStamp = toU64(&buffer[byteCount]); byteCount += 8;
    if( frameIdx )
      *frameIdx = toU32(&buffer[byteCount]); byteCount += 4;
  }
return byteCount;
}

// similar to SLFIPEXBinaryKlv, but with a control for frequency in SetMetadataRate. 
// Support for what looks like a future valid tag in MISB.
s32 SLFIPSetAppendedMetadata(SLPacketType buffer, const u8* data, u8 length, u16 dispId)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = length + 5; // 1(type) + 1(checksum) + 1(length) + length + 2(displayID)
  buffer[byteCount++] = AppendedMetadata;
  buffer[byteCount++] = length;
  memcpy(&buffer[byteCount], data, length);
  byteCount += length;
  buffer[byteCount++] = (dispId>>0)&0xFF;
  buffer[byteCount++] = (dispId>>8)&0xFF;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

// allows a user to set a frame index to a frame, for later reference to telemtry packets referring this frame.
// note that this index does not automatically increment - it is up to the user to manage this value.
// the camera index is mandatory.
s32 SLFIPSetFrameIndex(SLPacketType buffer, const u8 camIdx, const u32 index)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7; // u32 is 4 bytes + camIdx + type + checksum
  buffer[byteCount++] = FrameIndex;
  buffer[byteCount++] = camIdx;
  byteCount = write4(buffer, byteCount, index);

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

////////////////////////////////////////////////////////////////////////////
// SLA-3000 only. For users with pixel control over video input, allows to enable parsing of FIP commands on the two lines of 
// the Y buffer of the image past the valid ROI row specified in setAcqParams (0x37).
s32 SLFIPDigiVidParseParams(SLPacketType buffer, const u8 camIdx, const bool enable, const bool startAt0, const bool oneFipPerImage, const bool usePixelValue, const bool startAfterAcquired)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 7; // 5 u8s is 5 bytes + type + checksum
  buffer[byteCount++] = DigitalVideoParserParameters;
  buffer[byteCount++] = camIdx;
  buffer[byteCount++] = enable ? 1 : 0;         // turn parsing on/off without changing any other settings
  u8 flags = 0;
  flags |= startAt0 ? 0x01 : 0;                 // start at the first pixel of the first row, no margins.
  flags |= startAfterAcquired ? 0x02 : 0;       // start at the first line after the qcuisition window, or after the ROI window.
  buffer[byteCount++] = flags;       
  buffer[byteCount++] = oneFipPerImage ? 1 : 0; // if one FIP was parsed - don't look for more in this 2-row buffer.
  buffer[byteCount++] = usePixelValue ? 1 : 0;  // if true: use pixel value per byte in FIP message: 
                                                // FipByte[byteIndex] = pixelVal; this requires perfect accuracy.
                                                // if false: FipByte[byteIndex][bitIndex] = (pixelVal < 128 ? 0 : 1). more lenient.

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;

}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPStreamingControl(SLPacketType buffer, u16 streamingMask)
{
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 4;
  buffer[byteCount++] = StreamingControl;
  buffer[byteCount++] = (u8) streamingMask;
  buffer[byteCount++] = (u8)(streamingMask >> 8);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
s32 SLFIPSetExternalProgram(SLPacketType buffer, const u8 programType, const char* prog1, const char* prog2, const char* prog3)
{
  u8 prog1Len = strlen(prog1);
  u8 prog2Len = strlen(prog2);
  u8 prog3Len = strlen(prog3);

  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6 + prog1Len + prog2Len + prog3Len;
  buffer[byteCount++] = ExternalProgram;
  buffer[byteCount++] = programType;

  buffer[byteCount++] = prog1Len;
  for(s32 idx=0; idx < prog1Len; idx++)
    buffer[byteCount++] = prog1[idx];

  buffer[byteCount++] = prog2Len;
  for(s32 idx=0; idx < prog2Len; idx++)
    buffer[byteCount++] = prog2[idx];

  buffer[byteCount++] = prog3Len;
  for(s32 idx=0; idx < prog3Len; idx++)
    buffer[byteCount++] = prog3[idx];

  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH]-1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
/*! 
 * @param systemValue one of SYSVALUE values to set value.
 * @apram setMode bit ORed field, see SYSVALUE_MODE.
 * @param value points to array of s32.
 * @param numValues 1 .. 4.
*/
s32 SLFIPSetSystemValue(SLPacketType buffer, u32 systemValue, u32 setMode, const s32 *value, u32 numValues)
{
  if (numValues < 1 || numValues > 4 || value == 0)
    return -1;
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 8 + numValues * sizeof(s32) + 2;
  buffer[byteCount++] = (u8)(SetSystemValue);
  buffer[byteCount++] = (u8)(systemValue);
  buffer[byteCount++] = (u8)(systemValue >> 8);
  buffer[byteCount++] = (u8)(systemValue >> 16);
  buffer[byteCount++] = (u8)(systemValue >> 24);
  buffer[byteCount++] = (u8)(setMode);
  buffer[byteCount++] = (u8)(setMode >> 8);
  buffer[byteCount++] = (u8)(setMode >> 16);
  buffer[byteCount++] = (u8)(setMode >> 24);
  memcpy(&buffer[byteCount], value, numValues * sizeof(s32));
  byteCount += numValues * sizeof(s32);
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

///////////////////////////////////////////////////////////////////////////////
// Deprecated -- use SetSystemValue.
s32 SLFIPSetGeneric(SLPacketType buffer, u32 cmdIndex, u16 extraLen, const u8 *extraBuf)
{
  //SLDebugAssert(cmdIndx == 1);
  u8 byteCount = SLFIPAddHeader(&buffer[0]);
  buffer[byteCount++] = 6 + extraLen;
  buffer[byteCount++] = SetGeneric;
  buffer[byteCount++] = (cmdIndex & 0xFF);
  buffer[byteCount++] = (cmdIndex & 0xFF00) >> 8;
  buffer[byteCount++] = (cmdIndex & 0xFF0000) >> 16;
  buffer[byteCount++] = (cmdIndex & 0xFF000000) >> 24;
  memcpy(&buffer[byteCount], extraBuf, extraLen); byteCount += extraLen;
  buffer[byteCount++] = SLComputeFIPChecksum(&buffer[SLFIP_OFFSET_TYPE], buffer[SLFIP_OFFSET_LENGTH] - 1);
  return byteCount;
}

