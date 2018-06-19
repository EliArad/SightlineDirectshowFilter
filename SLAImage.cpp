
/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/

#pragma once
#include "SLAImage.h"
#include "SLAHal.h"

////////////////////////////////////////////////////////////////////////////////
// Set type, dsr, dsc, step, off, etc for an SLYuvImage
static SLStatus setbytype(s16 *dsr, s16 *dsc, SLA_IMAGE_TYPE type)
{
  switch(type) {
    case SLA_IMAGE_YUV_420:
    case SLA_IMAGE_NV12:
      *dsr = *dsc = 1;
      break;
    case SLA_IMAGE_YUV_422:
      *dsc = 1;
      break;
    case SLA_IMAGE_YUYV:
    case SLA_IMAGE_UYVY:
    case SLA_IMAGE_G8:
    case SLA_IMAGE_G16:
    case SLA_IMAGE_C24_PACKED:
    case SLA_IMAGE_BGGR:
      break;
    default:
      return SLA_ERROR; // type not supported.
  }
  return SLA_SUCCESS;
}




SLStatus SLASetupImage(
      SLAImage *image,     //!< Resulting color scale image structure
      SLA_IMAGE_TYPE type, //!< UYVY or YUYV or YUV_420 or YUV_422.
      s32 high,            //!< Height of the image in pixels
      s32 wide,            //!< Width of the image in pixels
      s32 stride,          //!< Pixels from one row to the next
      u8 *pxl              //!< Pointer to the pixel buffer
      )
{
  if(!image)
    return SLA_ERROR;
  SLAMemset(image,0,sizeof(*image));
  image->y       = pxl;
  image->yhigh   = (s16)high;
  image->ywide   = (s16)wide;
  image->ystride = stride;

  s16 dsr, dsc;
  if(setbytype(&dsr, &dsc, type)!=SLA_SUCCESS)
    return SLA_ERROR;

  switch(type) {
    case SLA_IMAGE_YUV_420:
    case SLA_IMAGE_YUV_422:
    case SLA_IMAGE_NV12:
      // NOTE:  Assuming the uv planes come right after the y plane in memory
      image->uvhigh   = (s16)high>>dsr;
      image->uvwide   = (s16)wide>>dsc;
      image->uvstride = (s16)stride>>dsc;
      image->uv[0]   = image->y + high*stride;
      if(type!=SLA_IMAGE_NV12)
        image->uv[1]   = image->uv[0] + image->uvhigh*image->uvstride;
      else {
        image->uv[1] = image->uv[0]+1;
        image->uvstride*=2;
      }
      break;
    default:
      break;
  }
  return SLA_SUCCESS;
}

/*! Set up an SLAImage structure
 * @return SLA_SUCCESS if object was initialized
 */
SLStatus SLASetupImage(
      SLAImage *image,     //!< Resulting color scale image structure
      SLA_IMAGE_TYPE type, //!< UYVY or YUYV or YUV_420 or YUV_422.
      s32 high,            //!< Height of the image in pixels
      s32 wide,            //!< Width of the image in pixels
      s32 ystride,         //!< Luma pixels from one row to the next
      s32 uvstride,        //!< Luma pixels from one row to the next
      u8 *y,               //!< Pointer to the luma pixel buffer
      u8 *u,               //!< Pointer to the u chroma pixel buffer
      u8 *v                //!< Pointer to the v chroma pixel buffer
      )
{
  if(!image)
    return SLA_ERROR;
  SLAMemset(image,0,sizeof(SLAImage));
  image->y        = y;
  image->uv[0]    = u;
  image->uv[1]    = v;
  image->yhigh    = (s16)high;
  image->ywide    = (s16)wide;
  image->ystride  = ystride;
  image->uvstride = uvstride;

  // Set type, dsr, dsc, step, off, etc
  s16 dsr, dsc;
  if(setbytype(&dsr, &dsc, type)!=SLA_SUCCESS)
    return SLA_ERROR;

  switch(type) {
    case SLA_IMAGE_YUV_420:
    case SLA_IMAGE_YUV_422:
    case SLA_IMAGE_NV12:
      // NOTE:  Assuming the uv planes come right after the y plane in memory
      image->uvhigh   = (s16)high>>dsr;
      image->uvwide   = (s16)wide>>dsc;
      break;
    default:
      break;
  }
  return SLA_SUCCESS;
}
//@}

