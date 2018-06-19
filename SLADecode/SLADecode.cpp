/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#include "SLADecode.h"
#include "SLADecodeFFMpeg.h"
#include "SLAImage.h"
#include "SLAKlvDecode.h"
#include "SLAHal.h"

typedef struct {
  SLADecodeFFMPEG ffcam;
  SLAImage image;
  SLAImage *yuvIm;
  void *userContext;
  SLADecodeCB userCb;
  SLAStatsCB  userStatsCb;
  int frameCount;
  int klvCount;
  int haveKLV;
} SLADecodeData;

bool slaCB(SLAImage *image, void *context, u32 capFlags)
{
  SLADecodeData *pData = (SLADecodeData *)context;

  if(!image){
    // indicates a timeout
    pData->userCb(pData->userContext, 0, 0, 0);
    return true;
  }

  pData->image.type = image->type;
  pData->image.yhigh = image->yhigh;
  pData->image.ywide = image->ywide;
  pData->image.ystride = image->ystride*3;  // SLAImage stride in bytes, SLYuvImage stride in pixels
  pData->image.y = image->y;
  pData->yuvIm = image;
//  pData->image.timestamp = image->timestamp;

  // if there isn't any klv, use the image-only callback form
  if(pData->haveKLV==0){
	  pData->userCb(pData->userContext, &pData->image, 0, 0);
  }
  pData->frameCount++;
  return true;
}

void slaStatsCb(CapStats *stats, void *context)
{
  SLADecodeData *pData = (SLADecodeData *)context;	
  SLCapStats myStats;

  // Copy fields from internal klv struct to external
  SLAMemset(&myStats, 0, sizeof(myStats));

  myStats.FrameRate = stats->FrameRate;
  myStats.KlvBitRate = stats->KlvBitRate;
  myStats.TotalBitRate = stats->TotalBitRate;
  myStats.VideoBitRate = stats->VideoBitRate;

  strncpy(myStats.Encapsulation, stats->Encapsulation, CAP_STATS_NAME_LENGTH);
  strncpy(myStats.Codec, stats->Codec, CAP_STATS_NAME_LENGTH);

  switch (stats->Profile){
  default:
  case SLA_PROFILE_NONE:
    myStats.Profile = NONE;
    break;
  case SLA_PROFILE_BASELINE:
    myStats.Profile = BASELINE;
    break;
  case SLA_PROFILE_CONSTRAINED_BASELINE:
    myStats.Profile = CONSTRAINED_BASELINE;
    break;
  case SLA_PROFILE_EXTENDED:
    myStats.Profile = EXTENDED;
    break;
  case SLA_PROFILE_MAIN:
    myStats.Profile = MAIN;
    break;
  case SLA_PROFILE_HIGH:
    myStats.Profile = HIGH;
    break;
  }

  myStats.MaxFrameBytes = stats->MaxFrameBytes;
  myStats.MinFrameBytes = stats->MinFrameBytes;

  myStats.KeyFrames = stats->KeyFrames;
  myStats.IFrames = stats->IFrames;
  myStats.PFrames = stats->PFrames;
  myStats.BFrames = stats->BFrames;
  myStats.OtherFrames = stats->OtherFrames;

  if( pData->userStatsCb )
    pData->userStatsCb( &myStats, pData->userContext );

  // This function happens at about 1Hz.  Figure out if there is KLV data at that rate
  if( pData->klvCount > 0 ) {
    pData->haveKLV = 0;
    pData->klvCount = 0;
  }
}

#pragma warning( disable : 4996 )
void slaKLVCb(KLVData *klv, KLVData *klvRecent, void *context)
{
  SLADecodeData *pData = (SLADecodeData *)context;

  pData->haveKLV = 1;
  SLAImage *cbImg = 0;
  if(pData->frameCount && pData->yuvIm)
    cbImg = &pData->image;
  pData->userCb(pData->userContext, cbImg, klv, klvRecent);

  // Indicate that the image was passed to user application
  pData->klvCount++;
  pData->yuvIm = 0;
}

int __cdecl SLADecode::Create(const char *UDPAddress, SLADecodeCB cb)
{

	return 1;
}
int  __cdecl SLADecode::Create(const char *UDPAddress, SLADecodeCB cb, SLAStatsCB statsCB, void *cbContext, bool rgba)
{
  SLADecodeData *data = new SLADecodeData;

  data->userContext = cbContext;
  data->userCb = cb;
  data->userStatsCb = statsCB;
  data->klvCount = 0;
  data->yuvIm = 0;
  data->frameCount = data->haveKLV = 0;

  // Last parameters are doRGB and noRelease
  SLA_IMAGE_TYPE type = rgba ? SLA_IMAGE_C32_PACKED : SLA_IMAGE_C24_PACKED;
  if (data->ffcam.Initialize(UDPAddress, type, slaCB, data, 1, 0, 1) != SLA_SUCCESS) {
    delete data;
    return -1;
  }

  data->ffcam.SetKLVCallBack(slaKLVCb);
  data->ffcam.SetStatsCallBack(slaStatsCb, data);

  Data = (void*)data;
  return 0;
}

int __cdecl SLADecode::Create(const char *UDPAddress, SLADecodeCB cb, void *cbContext)
{
  return Create(UDPAddress, cb, NULL, cbContext);
}

int SLADecode::Destroy()
{
  SLADecodeData *data = (SLADecodeData*)Data;
  if( data ) {
    StopSaving();
    data->ffcam.Cleanup();
    delete data;
    Data = 0;
    return 0;
  }
  return -1;
}

int SLADecode::SetAddress(const char *UDPAddress)
{
  SLADecodeData *data = (SLADecodeData*)Data;
  //data->ffcam.Cleanup();

  data->frameCount = data->haveKLV = 0;

  if(data->ffcam.Initialize(UDPAddress, SLA_IMAGE_C24_PACKED, slaCB, data, 1, 0, 1)!=SLA_SUCCESS)
    return -1;
  return 0;
}

int SLADecode::SetPALResampling(bool resample)
{
  SLADecodeData *data = (SLADecodeData*)Data;
  data->ffcam.SetPALResample(resample);
  return 0;
}

int SLADecode::SetUpSample(int upSample)
{
  SLADecodeData *data = (SLADecodeData*)Data;
  if(data)
    data->ffcam.SetUpSample(upSample);
  return 0;
}

int SLADecode::GetUpSample()
{
  SLADecodeData *data = (SLADecodeData*)Data;
  if(!data)
    return 1;
  return data->ffcam.GetUpSample();
}

int SLADecode::StartSaving(const char *filename)
{
  SLADecodeData *data = (SLADecodeData*)Data;
  SLStatus rv;
  if (data == NULL)
	  return -1;
  rv = data->ffcam.StartSaving(filename);
  if(rv == SLA_SUCCESS)
    return 0;
  return -1;
}

int SLADecode::StopSaving()
{
  SLADecodeData *data = (SLADecodeData*)Data;
  if (data == NULL)
	  return -1;
  SLStatus rv;
  rv = data->ffcam.StopSaving();
  if(rv == SLA_SUCCESS)
    return 0;
  return -1;
}

int SLADecode::Pause(bool enable)
{
  SLADecodeData *data = (SLADecodeData*)Data;
  if (data == NULL)
	  return -1;
  SLStatus rv;
  rv = data->ffcam.Pause(enable);
  if(rv == SLA_SUCCESS)
    return 0;
  return -1;
}

int SLADecode::Seek(double timeStamp)
{
  //timeStamp = //SLMIN(0, timeStamp); //0 means go back to the start
  SLADecodeData *data = (SLADecodeData*)Data;
  SLStatus rv;
  rv = data->ffcam.Seek(timeStamp);
  if(rv == SLA_SUCCESS)
    return 0;
  return -1;
}

double SLADecode::GetDuration()
{
	SLADecodeData *data = (SLADecodeData*)Data;
	return data->ffcam.GetDuration();
}


s32 SLADecode::BufferToKLVData(KLVData *klv, const u8* buf, u16 len, s32 bufStartOffset)
{
   s32 retVal = -1;
   //returns 1 for success, 0 for failure
   s32 ret =  ReadKlvFrame(klv, buf, len, bufStartOffset);
   if(ret == 1) retVal = 0; // this routine returns 0 for success, -1 for failure
   return retVal;
}

