/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/

#include "SLAKlvDecode.h"
#include "SLAHal.h"

// Implements MISB EG 0601.1
static const u8 SLUniversalKey[] = {0x06, 0x0E, 0x2B, 0x34,  0x02, 0x0B, 0x01, 0x01,   0x0E, 0x01, 0x03, 0x01,  0x01, 0x00, 0x00, 0x00};

#if (defined linux || defined LINUX)
static const u64 UTC_EPOCH = 1322647200000000LLU;
static u64 CONST_K0 = (u64)0x060e2b34020b0101LLU;
static u64 CONST_K1 = (u64)0x0e01030101000000LLU;
#else
// Starting time (November 30, 2011 ~10am in us since 1/1/1970)
static const u64 UTC_EPOCH = 1322647200000000LU;
static u64 CONST_K0 = (u64)0x060e2b34020b0101LU;
static u64 CONST_K1 = (u64)0x0e01030101000000LU;
#endif
static u8  CONST_K0_FIRST_BYTE = 0x06;


u16 Checksum(const u8 *src, u32 len)
{
  u32 j;
  u32 checksum = 0;
  for(j=0;j<len;j++){
    checksum += src[j] << (8*((j+1)%2));
  }
  return (u16)checksum;
}

static s32 ReadBer(const u8 *buffer, u16 *length)
{
  if(buffer[0]==0x82){
    *length = (buffer[1]<<8) | (buffer[2]);
    return 3;
  }
  if(buffer[0]==0x81){
    *length = (buffer[1]);
    return 2;
  }
  // Check for error
  if (buffer[0] >= 128)
    return -1;

  *length = buffer[0];
  return 1;
}


//static u16 ReadEl(const u8 *buffer, u16 len, char str[])
//{
//  int i;
//  for(i=0;i<len;i++)
//    str[i] = buffer[i];
//  return len;
//}

static s32 ReadEl(const u8 *buffer, u16 len, u8 *val)
{
  if(len!=1)
    return -1;
  *val = buffer[0];
  return len;
}

static s32 ReadEl(const u8 *buffer, u16 len, u16 *val)
{
  if(len != 1 && len != 2)
    return -1;
  if(len==1)
    *val = buffer[0];
  else
    *val = (buffer[0]<<8) | buffer[1];
  return len;
}

static s32 ReadEl(const u8 *buffer, u16 len, u32 *val)
{
  if(len != 1 && len != 2 && len != 4)
    return -1;
  if(len==1)
    *val = buffer[0];
  else if(len==2)
    *val = (buffer[0]<<8) | buffer[1];
  else
    *val = (buffer[0]<<24) | (buffer[1]<<16) | (buffer[2]<<8) | buffer[3];
  return len;
}

static s32 ReadEl(const u8 *buffer, u16 len, u64 *val)
{
  if(len != 1 && len != 2 && len != 4 && len != 8)
    return -1;

  if(len<=4){
    u32 tmp;
    ReadEl(buffer, len, &tmp);
    *val = tmp;
    return len;
  }
  u64 v = 0;
  for(int b=0;b<len;b++){
    u64 tmp = buffer[b];
    v |= tmp<<((7-b)*8);
  }
  *val = v;
  return len;
}

static s32 ReadEl(const u8 *buffer, u16 len, s16 *val)
{
  return ReadEl(buffer, len, (u16*)val);
}

static s32 ReadEl(const u8 *buffer, u16 len, s32 *val)
{
  return ReadEl(buffer, len, (u32*)val);
}

static s32 ReadEl(const u8 *buffer, u16 len, s64 *val)
{
  return ReadEl(buffer, len, (u64*)val);
}

static s32 ReadEl(const u8 *buffer, u16 len, KLVBytes *b)
{
  int i;
  b->len = (u8)SLMIN(len, sizeof(b->data));
  for(i=0;i<b->len;i++)
    b->data[i] = buffer[i];
  return len;
}

static u32 ReadElementBEROID(const u8 *buffer, u32 *val)
{
  u32 v = buffer[0];
  if(buffer[0]<128){
    *val = buffer[0];
    return 1;
  }
  if(buffer[1]<128){
    *val = ((buffer[0]&0x7f)<<7) | (buffer[1]&0x7f);
    return 2;
  }
  if(buffer[2]<128){
    *val = ((buffer[0]&0x7f)<<14) | ((buffer[1]&0x7f)<<7) | (buffer[2]&0x7f);
    return 3;
  }
  if(buffer[3]<128){
    *val = ((buffer[0]&0x7f)<<21) | ((buffer[1]&0x7f)<<14) | ((buffer[2]&0x7f)<<7) | (buffer[3]&0x7f);
    return 4;
  }
  return -1;
}

static s32 ReadV(const u8 *buffer, u16 len, u16 *val)
{
  int i;
  u32 v = 0;
  if(len>2)
    return -1;
  for(i=0;i<len;i++)
    v = (v<<8) | buffer[i];
  *val = v;
  return len;
}

static s32 ReadV(const u8 *buffer, u16 len, u32 *val)
{
  int i;
  u32 v = 0;
  if(len>4)
    return -1;
  for(i=0;i<len;i++)
    v = (v<<8) | buffer[i];
  *val = v;
  return len;
}



//#define Read(b,l,x) if(ReadEl(b, l, x)!=len) SLAssert(0);
#define Read(b,l,x) ReadEl(b, l, x)

static s32 ReadElementSecurity(const u8 *buffer, KLVSecurityLocalSet *s)
{
  int j=1;
  u8 key = buffer[0];
  u16 len;
  s32 rv;
  j += ReadBer(&buffer[1], &len);

  s32 unknown = 0;

  buffer += j;
  if(s){
    switch(key){
      case SL_LDS_KEY_SECURITY_CLASSIFICATION:
        rv = Read(buffer, len, &s->Classification);
        break;
      case SL_LDS_KEY_SECURITY_CLASSIFYINGCOUNTRYCODINGMETHOD:
        rv = Read(buffer, len, &s->ClassifyingCountryCodingMethod);
        break;
      case SL_LDS_KEY_SECURITY_CLASSIFYINGCOUNTRY:
        rv = Read(buffer, len, &s->ClassifyingCountry);
        break;
      case SL_LDS_KEY_SECURITY_SCISHIINFORMATION:
        rv = Read(buffer, len, &s->SCISHIInformation);
        break;
      case SL_LDS_KEY_SECURITY_CAVEATS:
        rv = Read(buffer, len, &s->Caveats);
        break;
      case SL_LDS_KEY_SECURITY_RELEASINGINSTRUCTIONS:
        rv = Read(buffer, len, &s->ReleasingInstructions);
        break;
      case SL_LDS_KEY_SECURITY_OBJECTCOUNTRYCODINGMETHOD:
        rv = Read(buffer, len, &s->ObjectCountryCodingMethod);
        break;
      case SL_LDS_KEY_SECURITY_OBJECTCOUNTRYCODES:
        rv = Read(buffer, len, &s->ObjectCountryCodes);
        break;
      case SL_LDS_KEY_SECURITY_METADATAVERSION:
        rv = Read(buffer, len, &s->SecurityMetadataVersion);
        break;
      default:
        SLATrace("Unkown KLV Security key:%x\n", key);
        rv = len;
        unknown++;
        break;
    }
    if(rv != len)
      return -1;
  }
  return j+len;
}

static s32 ReadEl(const u8 *buffer, u16 len, KLVSecurityLocalSet *s)
{
  // Parse the records
  s32 rv;
  for(u16 k=0;k<len;){
    rv = ReadElementSecurity(buffer+k, s);
    if(rv<0)
      return -1;
    k+=rv;
  }
  return len;
}

static s32 ReadElementTargetPack(const u8 *buffer, KLVVTargetPack *t)
{
  int j=1;
  u8 key = buffer[0];
  u16 len;
  s32 rv;
  j += ReadBer(&buffer[1], &len);

  s32 unknown = 0;

  buffer += j;
  if(t){
    switch(key){
      case SL_LDS_KEY_VTARGET_CENTROID_PIXEL:
        rv = ReadV(buffer, len, &t->TargetCentroidPixelNumber);
        break;
      case SL_LDS_KEY_VTARGET_BOUNDING_BOX_TOP_LEFT:
        rv = ReadV(buffer, len, &t->BoundingBoxTopLeftPixelNumber);
        break;
      case SL_LDS_KEY_VTARGET_BOUNDING_BOX_BOTTOM_RIGHT:
        rv = ReadV(buffer, len, &t->BoundingBoxBottomRightPixelNumber);
        break;
      case SL_LDS_KEY_VTARGET_TARGET_CONFIDENCE_LEVEL:
        rv = Read(buffer, len, &t->TargetConfidenceNumber);
        break;
      case SL_LDS_KEY_VTARGET_TARGET_HISTORY:
        rv = len;
        break;
      default:
        SLATrace("Unkown KLV VMti key:%x\n", key);
        rv = len;
        unknown++;
        break;
    }
    if(rv != len)
      return -1;
  }
  return j+len;
}

static s32 ReadTargetPack(const u8 *buffer, KLVVTargetPack *t)
{
//  u8 key = buffer[0];
  u16 len, beroidLen, j=0;
  s32 rv;

  j += ReadBer(&buffer[0], &len);

  // First value is id number w/o key or length
  beroidLen = ReadElementBEROID(buffer+j, &t->TargetIDNumber);
  //j += beroidLen;

  int k;
  for(k=0;k+beroidLen<len;){
    rv = ReadElementTargetPack(buffer+j+beroidLen+k, t);
    if(rv<0)
      return -1;
    k += rv;
  }

  return j+len;
}

static s32 ReadElementVmti(const u8 *buffer, KLVVmtiLocalSet *v)
{
  int i,nt;
  int j=1;
  u8 key = buffer[0];
  u16 len;
  s32 rv;
  j += ReadBer(&buffer[1], &len);

  s32 unknown = 0;

  buffer += j;
  if(v){
    switch(key){
      case SL_LDS_KEY_VMTI_VERSION:
        rv = ReadV(buffer, len, &v->Version);
        break;
      //case SL_LDS_KEY_VMTI_NUM_TARGETS_DETECTED:
        //rv = ReadV(buffer, len, &v->nTargets);
        //break;
      case SL_LDS_KEY_VMTI_NUM_REPORTED_TARGETS:
        rv = ReadV(buffer, len, &v->nTargets);
        break;
      case SL_LDS_KEY_VMTI_FRAME_WIDTH:
        rv = ReadV(buffer, len, &v->FrameWidth);
        break;
      case SL_LDS_KEY_VMTI_FRAME_HEIGHT:
        rv = ReadV(buffer, len, &v->FrameHeight);
        break;
      case SL_LDS_KEY_VMTI_VTARGET_SERIES:
        i = nt = 0;
        while(i<len && nt<KLV_MAX_NUMBER_OF_TARGETS){
          rv = ReadTargetPack(buffer+i, &v->Target[nt]);
          if(rv<0)
            return -1;
          i += rv;
          nt++;
        }
        v->nTargets = nt;  // may have already been set with SL_LDS_KEY_VMTI_NUM_REPORTED_TARGETS

        rv = i;
        break;
      default:
        SLATrace("Unkown KLV VMti key:%x\n", key);
        unknown++;
        rv = len;
        break;
    }
    if(rv!=len)
      return -1;
  }
  return j+len;
}

static s32 ReadEl(const u8 *buffer, u16 len, KLVVmtiLocalSet *v)
{
  s32 rv;
  // Parse the records
  for(u16 k=0;k<len;){
    rv = ReadElementVmti(buffer+k, v);
    if(rv<0)
      return -1;
    k += rv;
  }
  return len;
}

s32 ReadElement(const u8 *buf, u32 ldsOffset, KLVData *dest)
{
  int j=1;
  const u8 *buffer = buf+ldsOffset;
  u8 key = buffer[0];
  s32 rv;
  u16 len;
  j += ReadBer(&buffer[1], &len);

  s32 unknown = 0;

  buffer += j;
  if(dest){
    switch(key){
      case SL_LDS_KEY_CHECKSUM:
        // Calculate checksum 
        u16 cs, expectedCs;
        expectedCs = Checksum(buf, ldsOffset+2);
        rv = Read(buffer, len, &cs);
        if(expectedCs != cs)
          rv = -2;
        break;
      case SL_LDS_KEY_UTCTIME:
        rv = Read(buffer, len, &dest->Utctime);
        break;
      case SL_LDS_KEY_MISSIONID:
        rv = Read(buffer, len, &dest->Missionid);
        break;
      case SL_LDS_KEY_PLATFORMTAILNUMBER:
        rv = Read(buffer, len, &dest->PlatformTailNumber);
        break;
      case SL_LDS_KEY_PLATFORMHEADINGANGLE:
        rv = Read(buffer, len, &dest->PlatformHeadingAngle);
        break;
      case SL_LDS_KEY_PLATFORMPITCHANGLE:
        rv = Read(buffer, len, &dest->PlatformPitchAngle);
        break;
      case SL_LDS_KEY_PLATFORMROLLANGLE:
        rv = Read(buffer, len, &dest->PlatformRollAngle);
        break;
      case SL_LDS_KEY_PLATFORMTRUEAIRSPEED:
        rv = Read(buffer, len, &dest->PlatformTrueAirSpeed);
        break;
      case SL_LDS_KEY_PLATFORMINDICATEDAIRSPEED:
        rv = Read(buffer, len, &dest->PlatformIndicatedAirSpeed);
        break;
      case SL_LDS_KEY_PLATFORMDESIGNATION:
        rv = Read(buffer, len, &dest->PlatformDesignation);
        break;
      case SL_LDS_KEY_IMAGESOURCESENSOR:
        rv = Read(buffer, len, &dest->ImageSourceSensor);
        break;
      case SL_LDS_KEY_IMAGECOORDINATESYSTEM:
        rv = Read(buffer, len, &dest->ImageCoordinateSystem);
        break;
      case SL_LDS_KEY_SENSORLATITUDE:
        rv = Read(buffer, len, &dest->SensorLatitude);
        break;
      case SL_LDS_KEY_SENSORLONGITUDE:
        rv = Read(buffer, len, &dest->SensorLongitude);
        break;
      case SL_LDS_KEY_SENSORALTITUDE:
        rv = Read(buffer, len, &dest->SensorAltitude);
        break;
      case SL_LDS_KEY_SENSORHORIZONTALFIELDOFVIEW:
        rv = Read(buffer, len, &dest->SensorHorizontalFieldOfView);
        break;
      case SL_LDS_KEY_SENSORVERTICALFIELDOFVIEW:
        rv = Read(buffer, len, &dest->SensorVerticalFieldOfView);
        break;
      case SL_LDS_KEY_SENSORRELATIVEAZIMUTHANGLE:
        rv = Read(buffer, len, &dest->SensorRelativeAzimuthAngle);
        break;
      case SL_LDS_KEY_SENSORRELATIVEELEVATIONANGLE:
        rv = Read(buffer, len, &dest->SensorRelativeElevationAngle);
        break;
      case SL_LDS_KEY_SENSORRELATIVEROLLANGLE:
        rv = Read(buffer, len, &dest->SensorRelativeRollAngle);
        break;
      case SL_LDS_KEY_SLANTRANGE:
        rv = Read(buffer, len, &dest->SlantRange);
        break;
      case SL_LDS_KEY_TARGETWIDTH:
        rv = Read(buffer, len, &dest->TargetWidth);
        break;
      case SL_LDS_KEY_FRAMECENTERLATITUDE:
        rv = Read(buffer, len, &dest->FrameCenterLatitude);
        break;
      case SL_LDS_KEY_FRAMECENTERLONGITUDE:
        rv = Read(buffer, len, &dest->FrameCenterLongitude);
        break;
      case SL_LDS_KEY_FRAMECENTERELEVATION:
        rv = Read(buffer, len, &dest->FrameCenterElevation);
        break;
      case SL_LDS_KEY_OFFSETCORNERLATITUDEPOINT1:
        rv = Read(buffer, len, &dest->OffsetCornerLatitudePoint1);
        break;
      case SL_LDS_KEY_OFFSETCORNERLONGITUDEPOINT1:
        rv = Read(buffer, len, &dest->OffsetCornerLongitudePoint1);
        break;
      case SL_LDS_KEY_OFFSETCORNERLATITUDEPOINT2:
        rv = Read(buffer, len, &dest->OffsetCornerLatitudePoint2);
        break;
      case SL_LDS_KEY_OFFSETCORNERLONGITUDEPOINT2:
        rv = Read(buffer, len, &dest->OffsetCornerLongitudePoint2);
        break;
      case SL_LDS_KEY_OFFSETCORNERLATITUDEPOINT3:
        rv = Read(buffer, len, &dest->OffsetCornerLatitudePoint3);
        break;
      case SL_LDS_KEY_OFFSETCORNERLONGITUDEPOINT3:
        rv = Read(buffer, len, &dest->OffsetCornerLongitudePoint3);
        break;
      case SL_LDS_KEY_OFFSETCORNERLATITUDEPOINT4:
        rv = Read(buffer, len, &dest->OffsetCornerLatitudePoint4);
        break;
      case SL_LDS_KEY_OFFSETCORNERLONGITUDEPOINT4:
        rv = Read(buffer, len, &dest->OffsetCornerLongitudePoint4);
        break;
      case SL_LDS_KEY_ICINGDETECTED:
        rv = Read(buffer, len, &dest->IcingDetected);
        break;
      case SL_LDS_KEY_WINDDIRECTION:
        rv = Read(buffer, len, &dest->WindDirection);
        break;
      case SL_LDS_KEY_WINDSPEED:
        rv = Read(buffer, len, &dest->WindSpeed);
        break;
      case SL_LDS_KEY_STATICPRESSURE:
        rv = Read(buffer, len, &dest->StaticPressure);
        break;
      case SL_LDS_KEY_DENSITYALTITUDE:
        rv = Read(buffer, len, &dest->DensityAltitude);
        break;
      case SL_LDS_KEY_OUTSIDEAIRTEMP:
        rv = Read(buffer, len, (u8*)&dest->OutsideAirTemp);
        break;
      case SL_LDS_KEY_TARGETLOCATIONLATITUDE:
        rv = Read(buffer, len, &dest->TargetLocationLatitude);
        break;
      case SL_LDS_KEY_TARGETLOCATIONLONGITUDE:
        rv = Read(buffer, len, &dest->TargetLocationLongitude);
        break;
      case SL_LDS_KEY_TARGETLOCATIONELEVATION:
        rv = Read(buffer, len, &dest->TargetLocationElevation);
        break;
      case SL_LDS_KEY_TARGETTRACKGATEWIDTH:
        rv = Read(buffer, len, &dest->TargetTrackGateWidth);
        break;
      case SL_LDS_KEY_TARGETTRACKGATEHEIGHT:
        rv = Read(buffer, len, &dest->TargetTrackGateHeight);
        break;
      case SL_LDS_KEY_TARGETERRORESTIMATECE90:
        rv = Read(buffer, len, &dest->TargetErrorEstimateCE90);
        break;
      case SL_LDS_KEY_TARGETERRORESTIMATELE90:
        rv = Read(buffer, len, &dest->TargetErrorEstimateLE90);
        break;
      case SL_LDS_KEY_GENERICFLAGDATA01:
        rv = Read(buffer, len, &dest->GenericFlagData);
        break;
      case SL_LDS_KEY_SECURITYLOCALMETADATASET:
        rv = Read(buffer, len, &dest->SecurityLDS);
        break;
      case SL_LDS_KEY_DIFFERENTIALPRESSURE:
        rv = Read(buffer, len, &dest->DifferentialPressure);
        break;
      case SL_LDS_KEY_PLATFORMANGLEOFATTACK:
        rv = Read(buffer, len, &dest->PlatformAngleOfAttack);
        break;
      case SL_LDS_KEY_PLATFORMVERTICALSPEED:
        rv = Read(buffer, len, &dest->PlatformVerticalSpeed);
        break;
      case SL_LDS_KEY_PLATFORMSIDESLIPANGLE:
        rv = Read(buffer, len, &dest->PlatformSideSlipAngle);
        break;
      case SL_LDS_KEY_AIRFIELDBAROMETRICPRESSURE:
        rv = Read(buffer, len, &dest->AirfieldBarometricPressure);
        break;
      case SL_LDS_KEY_ELEVATION:
        rv = Read(buffer, len, &dest->Elevation);
        break;
      case SL_LDS_KEY_RELATIVEHUMIDITY:
        rv = Read(buffer, len, &dest->RelativeHumidity);
        break;
      case SL_LDS_KEY_PLATFORMGROUNDSPEED:
        rv = Read(buffer, len, &dest->PlatformGroundSpeed);
        break;
      case SL_LDS_KEY_GROUNDRANGE:
        rv = Read(buffer, len, &dest->GroundRange);
        break;
      case SL_LDS_KEY_PLATFORMFUELREMAINING:
        rv = Read(buffer, len, &dest->PlatformFuelRemaining);
        break;
      case SL_LDS_KEY_PLATFORMCALLSIGN:
        rv = Read(buffer, len, &dest->PlatformCallSign);
        break;
      case SL_LDS_KEY_WEAPONLOAD:
        rv = Read(buffer, len, &dest->WeaponLoad);
        break;
      case SL_LDS_KEY_WEAPONFIRED:
        rv = Read(buffer, len, &dest->WeaponFired);
        break;
      case SL_LDS_KEY_LASERPRFCODE:
        rv = Read(buffer, len, &dest->LaserPRFCode);
        break;
      case SL_LDS_KEY_SENSORFIEDLOFVIEWNAME:
        rv = Read(buffer, len, &dest->SensorFieldOfViewName);
        break;
      case SL_LDS_KEY_PLATFORMMAGNETICHEADING:
        rv = Read(buffer, len, &dest->PlatformMagneticHeading);
        break;
      case SL_LDS_KEY_UASLDSVERSIONNUMBER:
        rv = Read(buffer, len, &dest->UasLDSVersionNumber);
        break;
      case SL_LDS_KEY_TARGETLOCATIONCOVARIANCEMATRIX:
        // This is TBD in MISB ST 0601.7, so ignore
        rv = len;
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMLATITUDE:
        rv = Read(buffer, len, &dest->AlternatePlatformLatitude);
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMLONGITUDE:
        rv = Read(buffer, len, &dest->AlternatePlatformLongitude);
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMALTITUDE:
        rv = Read(buffer, len, &dest->AlternatePlatformAltitude);
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMNAME:
        rv = Read(buffer, len, &dest->AlternatePlatformName);
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMHEADING:
        rv = Read(buffer, len, &dest->AlternatePlatformHeading);
        break;
      case SL_LDS_KEY_EVENTSTARTTIME:
        rv = Read(buffer, len, &dest->EventStartTime);
        break;
      case SL_LDS_KEY_RVTLOCALDATASET:
        // Could add parsing of this LDS in the future
        // For now it's treated as a block of bytes
        rv = Read(buffer, len, &dest->Rvt);
        break;
      case SL_LDS_KEY_VMTILOCALDATASET:
        rv = Read(buffer, len, &dest->VMti);
        break;
      case SL_LDS_KEY_SENSORELLIPSOIDHEIGHT:
        rv = Read(buffer, len, &dest->SensorEllipsoidHeight);
        break;
      case SL_LDS_KEY_ALTERNATEPLATFORMELLIPSOIDHEIGHT:
        rv = Read(buffer, len, &dest->AlternatePlatformEllipsoidHeight);
        break;
      case SL_LDS_KEY_OPERATIONALMODE:
        rv = Read(buffer, len, &dest->OperationalMode);
        break;
      case SL_LDS_KEY_FRAMECENTERHEIGHTABOVEELLIPSOID:
        rv = Read(buffer, len, &dest->FrameCenterHeightAboveEllipsoid);
        break;
      case SL_LDS_KEY_SENSORNORTHVELOCITY:
        rv = Read(buffer, len, &dest->SensorNorthVelocity);
        break;
      case SL_LDS_KEY_SENSOREASTVELOCITY:
        rv = Read(buffer, len, &dest->SensorEastVelocity);
        break;
      case SL_LDS_KEY_IMAGEHORIZONPIXELPACK:
        // Could add parsing of this field in the future
        // For now it's treated as a block of bytes
        rv = Read(buffer, len, &dest->ImageHorizonPixelPack);
        break;
      case SL_LDS_KEY_CORNERLATITUDEPOINT1FULL:
        rv = Read(buffer, len, &dest->CornerLatitudePoint1Full);
        break;
      case SL_LDS_KEY_CORNERLONGITUDEPOINT1FULL:
        rv = Read(buffer, len, &dest->CornerLongitudePoint1Full);
        break;
      case SL_LDS_KEY_CORNERLATITUDEPOINT2FULL:
        rv = Read(buffer, len, &dest->CornerLatitudePoint2Full);
        break;
      case SL_LDS_KEY_CORNERLONGITUDEPOINT2FULL:
        rv = Read(buffer, len, &dest->CornerLongitudePoint2Full);
        break;
      case SL_LDS_KEY_CORNERLATITUDEPOINT3FULL:
        rv = Read(buffer, len, &dest->CornerLatitudePoint3Full);
        break;
      case SL_LDS_KEY_CORNERLONGITUDEPOINT3FULL:
        rv = Read(buffer, len, &dest->CornerLongitudePoint3Full);
        break;
      case SL_LDS_KEY_CORNERLATITUDEPOINT4FULL:
        rv = Read(buffer, len, &dest->CornerLatitudePoint4Full);
        break;
      case SL_LDS_KEY_CORNERLONGITUDEPOINT4FULL:
        rv = Read(buffer, len, &dest->CornerLongitudePoint4Full);
        break;
      case SL_LDS_KEY_PLATFORMPITCHANGLEFULL:
        rv = Read(buffer, len, &dest->PlatformPitchAngleFull);
        break;
      case SL_LDS_KEY_PLATFORMROLLANGLEFULL:
        rv = Read(buffer, len, &dest->PlatformRollAngleFull);
        break;
      case SL_LDS_KEY_PLATFORMANGLEOFATTACKFULL:
        rv = Read(buffer, len, &dest->PlatformAngleOfAttackFull);
        break;
      case SL_LDS_KEY_PLATFORMSIDESLIPANGLEFULL:
        rv = Read(buffer, len, &dest->PlatformSideSlipAngleFull);
        break;
      case SL_LDS_KEY_COREIDENTIFIER:
        rv = Read(buffer, len, &dest->MotionImageryCoreIdentifier);
        break;
      case SL_LDS_KEY_SARMOTIONIMAGERYMETADATA:
        // Could add parsing of this LDS in the future
        // For now it's treated as a block of bytes
        rv = Read(buffer, len, &dest->SARMotionImageryMetadata);
        break;
      case SL_LDS_KEY_APPEND_METADATA:
        // Parsing of this LDS is done by the user. 
        // Also, This is not yet offically in the standard (AD, 6.22.2016)
        rv = Read(buffer, len, &dest->ASMAappend);
        break;
      default:
        SLATrace("Unkown KLV key:%x\n", key);
        unknown++;
        rv = len;
        break;
    }
    if(rv!=len)
      return -1;
  }
  return j+len;
}

s32 ReadKeyLength(const u8* buf, u16 len, u16 *bytesRead, u16 *klvLen, u16 *headerStart)
{
  if(len<19) {
    *bytesRead = *klvLen = 0;
    return 0;
  }

  int j=0;
  u64 k0,k1;
  do {
    k0 = k1 = 0;

    // Scan for first byte of universal key
    for(;j<len && buf[j]!= CONST_K0_FIRST_BYTE; j++);

    if(len < j+19) {
      if(headerStart)
        *headerStart = j;
      *bytesRead = j;
      *klvLen = 0;
      return 0;
    }

    // Check for universal key
    for(int b=0;b<8;b++){
      u64 tmp = buf[j+b];
      k0 |= tmp<<((7-b)*8);
    }
    for(int b=0;b<8;b++){
      u64 tmp = buf[j+b+8];
      k1 |= tmp<<((7-b)*8);
    }
    // If do loop continues, next byte should be inspected
    j++;
  } while ((k0 != CONST_K0) || (k1 != CONST_K1));
  // Adjust j to past the 16-byte key
  j += 15;

  s32 rv = ReadBer(buf+j, klvLen);
  if(rv<0){
    if(headerStart)
      *headerStart = j;
    *bytesRead = j;
    *klvLen = 0;
    return 0;
  }

  if(headerStart)
    *headerStart = j-16;
  j += rv;
  *bytesRead = j;
  return 1;
}





s32 ReadKlvElements(KLVData *klv, const u8* buf, u16 ldsOffset, u16 klvLen)
{
  u16 k;
  s32 rv;

  for(k=ldsOffset;k<klvLen+ldsOffset;) {
    rv = ReadElement(buf, k, klv);
    if(rv<0)
      return -1;
    k += rv;
  }
  return k-ldsOffset;
}

s32 ReadKlvFrame(KLVData *klv, const u8* buf, u16 len, s32 bufStartOffset)
{
  u16 headerOffset;
  u16 bytesRead;
  u16 klvLen;
  s32 rv;
  buf += bufStartOffset;
  len -= bufStartOffset;

  // ReadKlvFrame expects the 16-byte universal code to start at
  // offset 0. Pass len of 19 when looking for key and length to
  // prevent the function from searching for the key in the entire
  // buffer.  19 = 16-byte universal code + 3-byte (max) length
  if(ReadKeyLength(buf,SLMIN(19,len), &bytesRead, &klvLen, &headerOffset)) {
    rv = ReadKlvElements(klv, buf+headerOffset, bytesRead-headerOffset, SLMIN(len-bytesRead, klvLen));
    if(rv<0)
      return 0;
    return 1;
  }
  return 0;
}

#define CopyEl(dst, src, u, el) if(s->el!=u.el) d->el = s->el
#define CopyElBytes(dst, src, u, el) if(s->el.len != u.el.len) d->el = s->el

void SLCopyChangedKLV(KLVData *d, KLVData *s)
{
  CopyEl(d,s,KLVUnknown,Utctime);
  CopyElBytes(d,s,KLVUnknown,Missionid);
  CopyElBytes(d,s,KLVUnknown,PlatformTailNumber);
  CopyEl(d,s,KLVUnknown,PlatformHeadingAngle);
  CopyEl(d,s,KLVUnknown,PlatformPitchAngle);
  CopyEl(d,s,KLVUnknown,PlatformRollAngle);
  CopyEl(d,s,KLVUnknown,PlatformTrueAirSpeed);
  CopyEl(d,s,KLVUnknown,PlatformIndicatedAirSpeed);
  CopyElBytes(d,s,KLVUnknown,PlatformDesignation);
  CopyElBytes(d,s,KLVUnknown,ImageSourceSensor);
  CopyElBytes(d,s,KLVUnknown,ImageCoordinateSystem);
  CopyEl(d,s,KLVUnknown,SensorLatitude);
  CopyEl(d,s,KLVUnknown,SensorLongitude);
  CopyEl(d,s,KLVUnknown,SensorAltitude);
  CopyEl(d,s,KLVUnknown,SensorHorizontalFieldOfView);
  CopyEl(d,s,KLVUnknown,SensorVerticalFieldOfView);
  CopyEl(d,s,KLVUnknown,SensorRelativeAzimuthAngle);
  CopyEl(d,s,KLVUnknown,SensorRelativeElevationAngle);
  CopyEl(d,s,KLVUnknown,SensorRelativeRollAngle);
  CopyEl(d,s,KLVUnknown,SlantRange);
  CopyEl(d,s,KLVUnknown,TargetWidth);
  CopyEl(d,s,KLVUnknown,FrameCenterLatitude);
  CopyEl(d,s,KLVUnknown,FrameCenterLongitude);
  CopyEl(d,s,KLVUnknown,FrameCenterElevation);
  CopyEl(d,s,KLVUnknown,OffsetCornerLatitudePoint1);
  CopyEl(d,s,KLVUnknown,OffsetCornerLongitudePoint1);
  CopyEl(d,s,KLVUnknown,OffsetCornerLatitudePoint2);
  CopyEl(d,s,KLVUnknown,OffsetCornerLongitudePoint2);
  CopyEl(d,s,KLVUnknown,OffsetCornerLatitudePoint3);
  CopyEl(d,s,KLVUnknown,OffsetCornerLongitudePoint3);
  CopyEl(d,s,KLVUnknown,OffsetCornerLatitudePoint4);
  CopyEl(d,s,KLVUnknown,OffsetCornerLongitudePoint4);
  CopyEl(d,s,KLVUnknown,IcingDetected);
  CopyEl(d,s,KLVUnknown,WindDirection);
  CopyEl(d,s,KLVUnknown,WindSpeed);
  CopyEl(d,s,KLVUnknown,StaticPressure);
  CopyEl(d,s,KLVUnknown,DensityAltitude);
  CopyEl(d,s,KLVUnknown,OutsideAirTemp);
  CopyEl(d,s,KLVUnknown,TargetLocationLatitude);
  CopyEl(d,s,KLVUnknown,TargetLocationLongitude);
  CopyEl(d,s,KLVUnknown,TargetLocationElevation);
  CopyEl(d,s,KLVUnknown,TargetTrackGateWidth);
  CopyEl(d,s,KLVUnknown,TargetTrackGateHeight);
  CopyEl(d,s,KLVUnknown,TargetErrorEstimateCE90);
  CopyEl(d,s,KLVUnknown,TargetErrorEstimateLE90);
  CopyEl(d,s,KLVUnknown,GenericFlagData);
  CopyEl(d,s,KLVUnknown,SecurityLDS.Classification);
  CopyEl(d,s,KLVUnknown,SecurityLDS.ClassifyingCountryCodingMethod);
  CopyElBytes(d,s,KLVUnknown,SecurityLDS.ClassifyingCountry);
  CopyElBytes(d,s,KLVUnknown,SecurityLDS.SCISHIInformation);
  CopyElBytes(d,s,KLVUnknown,SecurityLDS.Caveats);
  CopyElBytes(d,s,KLVUnknown,SecurityLDS.ReleasingInstructions);
  CopyEl(d,s,KLVUnknown,SecurityLDS.ObjectCountryCodingMethod);
  CopyElBytes(d,s,KLVUnknown,SecurityLDS.ObjectCountryCodes);
  CopyEl(d,s,KLVUnknown,SecurityLDS.SecurityMetadataVersion);
  CopyEl(d,s,KLVUnknown,DifferentialPressure);
  CopyEl(d,s,KLVUnknown,PlatformAngleOfAttack);
  CopyEl(d,s,KLVUnknown,PlatformVerticalSpeed);
  CopyEl(d,s,KLVUnknown,PlatformSideSlipAngle);
  CopyEl(d,s,KLVUnknown,AirfieldBarometricPressure);
  CopyEl(d,s,KLVUnknown,Elevation);
  CopyEl(d,s,KLVUnknown,RelativeHumidity);
  CopyEl(d,s,KLVUnknown,PlatformGroundSpeed);
  CopyEl(d,s,KLVUnknown,GroundRange);
  CopyEl(d,s,KLVUnknown,PlatformFuelRemaining);
  CopyElBytes(d,s,KLVUnknown,PlatformCallSign);
  CopyEl(d,s,KLVUnknown,WeaponLoad);
  CopyEl(d,s,KLVUnknown,WeaponFired);
  CopyEl(d,s,KLVUnknown,LaserPRFCode);
  CopyEl(d,s,KLVUnknown,SensorFieldOfViewName);
  CopyEl(d,s,KLVUnknown,PlatformMagneticHeading);
  CopyEl(d,s,KLVUnknown,UasLDSVersionNumber);
  CopyEl(d,s,KLVUnknown,AlternatePlatformLatitude);
  CopyEl(d,s,KLVUnknown,AlternatePlatformLongitude);
  CopyEl(d,s,KLVUnknown,AlternatePlatformAltitude);
  CopyElBytes(d,s,KLVUnknown,AlternatePlatformName);
  CopyEl(d,s,KLVUnknown,AlternatePlatformHeading);
  CopyEl(d,s,KLVUnknown,EventStartTime);
  CopyElBytes(d,s,KLVUnknown,Rvt);
  CopyEl(d,s,KLVUnknown,VMti.FrameHeight);
  CopyEl(d,s,KLVUnknown,VMti.FrameWidth);
  CopyEl(d,s,KLVUnknown,VMti.Version);
  d->VMti.nTargets = s->VMti.nTargets;
  if(s->VMti.nTargets != KLVUnknown.VMti.nTargets)
    SLAMemcpy(d->VMti.Target, s->VMti.Target, sizeof(s->VMti.Target));
  CopyEl(d,s,KLVUnknown,SensorEllipsoidHeight);
  CopyEl(d,s,KLVUnknown,AlternatePlatformEllipsoidHeight);
  CopyEl(d,s,KLVUnknown,OperationalMode);
  CopyEl(d,s,KLVUnknown,FrameCenterHeightAboveEllipsoid);
  CopyEl(d,s,KLVUnknown,SensorNorthVelocity);
  CopyEl(d,s,KLVUnknown,SensorEastVelocity);
  CopyElBytes(d,s,KLVUnknown,ImageHorizonPixelPack);
  CopyEl(d,s,KLVUnknown,CornerLatitudePoint1Full);
  CopyEl(d,s,KLVUnknown,CornerLongitudePoint1Full);
  CopyEl(d,s,KLVUnknown,CornerLatitudePoint2Full);
  CopyEl(d,s,KLVUnknown,CornerLongitudePoint2Full);
  CopyEl(d,s,KLVUnknown,CornerLatitudePoint3Full);
  CopyEl(d,s,KLVUnknown,CornerLongitudePoint3Full);
  CopyEl(d,s,KLVUnknown,CornerLatitudePoint4Full);
  CopyEl(d,s,KLVUnknown,CornerLongitudePoint4Full);
  CopyEl(d,s,KLVUnknown,PlatformPitchAngleFull);
  CopyEl(d,s,KLVUnknown,PlatformRollAngleFull);
  CopyEl(d,s,KLVUnknown,PlatformAngleOfAttackFull);
  CopyEl(d,s,KLVUnknown,PlatformSideSlipAngleFull);
  CopyElBytes(d,s,KLVUnknown,MotionImageryCoreIdentifier);
  CopyElBytes(d,s,KLVUnknown,SARMotionImageryMetadata);
  CopyElBytes(d,s,KLVUnknown,ASMAappend);
}

