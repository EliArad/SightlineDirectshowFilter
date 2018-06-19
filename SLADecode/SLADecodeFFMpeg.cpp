/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "SLADecodeFFMpeg.h"
#include "SLAImage.h"
#include "SLAHal.h"
#include "SLAKlvDecode.h"
#include "SLAUdpReceive.h"
#include "SLARtspClient.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // Needed for winsock2, otherwise windows.h includes winsock1
#endif

#ifdef _WIN32
#include <windows.h>
#endif

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavutil/avutil.h>
}

#define MAX_KLV_BUFFER_LENGTH (2048)          //!< H264 only. KLV data size.
#define FFMPEG_MAX_HEIGHT 1080
#define FFMPEG_MAX_WIDTH  1920

typedef enum {
  IMAGE_NOT_IN_USE = 0,
  IMAGE_IN_USE     = 1
} IMAGE_USE;

#define N_IMAGE_BUFS 1 // NOTE:  Has to be 1 currently for stabtrack to work on PC

typedef enum {
  INPUT_FILE,             //!< From a file/directory source
  INPUT_NETWORK,          //!< SLA Network Source (UDP)
  INPUT_DEVICE,           //!< From physical hardware connected to a PC
  INPUT_FFMPEG_NETWORK    //!< pass a valid URL as a file name
} INPUT_TYPE;

typedef struct {
  SLA_Sem camSemaphore;
  bool isInit;
  bool skipOpen;
  char fName[100];
  INPUT_TYPE inputType;
  s32 nLoop;

  s32 high, wide, startFrame, frame;
  SLA_IMAGE_TYPE slOutType;
  SLA_IMAGE_TYPE slInType;
  enum PixelFormat ffOutType;
  enum PixelFormat ffInType;
  s32 videoStream, klvStream;
  AVPacket packet;
  AVInputFormat *fmt;
  AVIOContext *pIOCtx;
  AVDictionary *ioptions;
  AVFormatContext *pFormatCtx;
  AVCodecContext *pCodecCtx;
  AVCodec *pCodec;
  RtspClient *rtspClient;
  void *udpRx;
  SLA_COMPRESSED_FRAME compressedFrame;
  SLAUdpVideoProtocol lastStreamType;
  u8 cFrameData[MAX_COMPRESSED_BUFFER_SIZE];
  int skippedFrame;       // frame decode skip due to packet loss error
  int skipDisplay;        // Should skip display of frame to save processing time

  // Timeout management
  u32 timeExpired;
  s64 tmaxDelay;
  u32 t0Val;
  FILE *dumpFile;
  
  //file support
  int isPaused;

  AVFrame *pFrame, *pFrameOut;
  SwsContext *img_convert_ctx;
  u8 *buffer, *bufferOut;

  u32 done;
  u32 taskDone;
  SLA_Sem taskDoneSem;
  SLA_Sem imageSem;
  SLA_Sem processingSem;
  void* hmbx;          // mailbox handle for ffmpeg task

  s32 noRelease;       // passed in flag -- not using release to indicate that image has been consumed.
  PixelFormat inputFormat;
  SLAImage imageOut;
  void *callBackContext;
  SLCaptureCallback callBack;

  SLKLVCallback klvCallBack;

  // All KLV data ever received
  KLVData klv;

  // KLV data received this frame (non-received data elements marked invalid)
  KLVData klvRecent;

  SLStatsCallback statsCallBack;
  void *statsContext;
  CapStats stats;
  int frameCount;
  int byteCount, videoByteCount, klvByteCount;
  u64 tic0;

  void *cam;
  volatile bool started;

  SLMtsPrivCallback   mtsPrivCallback;
  bool                usePtsTimeStamp;

  //!< Use SLUDPReceive demux instead of FFMPEG internal version. Decoding SLALIB diag data works better with SLUDPReceive.
  bool                useSlDemux;
  bool resamplePAL;

  int upSample; // upsample factor (1, 2, or 4)

} FFCameraData;

// States of FFMPEG_task
typedef enum {
  TASK_ERROR,
  TASK_FIND_INPUT_FORMAT,
  TASK_OPEN_INPUT,
  TASK_FIND_STREAM_INFO,
  TASK_OPEN2,
  TASK_READ_FRAME_BUFFERED,
  TASK_READ_FRAME,
  TASK_DECODE_VIDEO2,
  TASK_REOPEN2,
  TASK_READ_FRAME_FINISHED,
  TASK_TIMEOUT,
  TASK_LOOP,
  TASK_EOF
} FFSTATE;

static char ffstateName[][80] = {
  "TASK_ERROR",
  "TASK_FIND_INPUT_FORMAT",
  "TASK_OPEN_INPUT",
  "TASK_FIND_STREAM_INFO",
  "TASK_OPEN2",
  "TASK_READ_FRAME_BUFFERED",
  "TASK_READ_FRAME",
  "TASK_DECODE_VIDEO2",
  "TASK_REOPEN2",
  "TASK_READ_FRAME_FINISHED",
  "TASK_TIMEOUT",
  "TASK_EOF"
};

enum {
  READ_FRAME_TIMEOUT = 500000   // 1/2 second timeout
};

// Command Types
typedef enum {
  FF_CONTROL_NAME,
  FF_CONTROL_INDEX,
  FF_CONTROL_SAVEFILE,
  FF_CONTROL_SEEK,
} FF_CONTROL;

typedef struct {
  FF_CONTROL type;
  s32 modifier;
  void *ptr;
  char name[1024];
  
} FFMpegControlPacket;

#define SLA_URL_PREFIX "udp://"
#define RTSP_URL_PREFIX "rtsp://"

//
// SLA streams tend to start with "udp://", so lets assume if we detect this prefix
// it is a SLA network stream and will use our custome DEMUX.
//
inline bool isUDPURL(const char *dirName)
{
  return (strncmp(dirName, SLA_URL_PREFIX, strlen(SLA_URL_PREFIX)) == 0);
}

//
// RTSP streams start with "rtsp://", so lets assume if we detect this prefix
// we need to use RTSP custom parser.
//
inline bool isRTSPURL(const char *path)
{
  return (strncmp(path, RTSP_URL_PREFIX, strlen(RTSP_URL_PREFIX)) == 0);
}

static enum PixelFormat SLAImageTypeToFFmpeg(SLA_IMAGE_TYPE sltype)
{
  switch(sltype){
    default:
    case SLA_IMAGE_UNKNOWN:
    case SLA_IMAGE_YUV_420:
      return PIX_FMT_YUV420P;
    case SLA_IMAGE_YUV_422:
      return PIX_FMT_YUV422P;
    case SLA_IMAGE_UYVY:
      return PIX_FMT_UYVY422;
    case SLA_IMAGE_YUYV:
      return PIX_FMT_YUYV422;
    case SLA_IMAGE_G8:
      return PIX_FMT_GRAY8;
    case SLA_IMAGE_G16:
      return PIX_FMT_GRAY16LE;
    case SLA_IMAGE_C24_PACKED:
      return PIX_FMT_BGR24;
    case SLA_IMAGE_C32_PACKED:
      return PIX_FMT_BGRA;
    case SLA_IMAGE_NV12:
      return PIX_FMT_NV12;
  }
}

static SLA_IMAGE_TYPE FFmpegToSLAImageType(enum PixelFormat fftype)
{
  switch(fftype){
    default:
      SLATrace("WARN: PixelFormat not supported.  Using SL_IMAGE_YUV420.\n");
    case PIX_FMT_YUVJ420P:
    case PIX_FMT_YUV420P:
      return SLA_IMAGE_YUV_420;
    case PIX_FMT_YUVJ422P:
    case PIX_FMT_YUV422P:
      return SLA_IMAGE_YUV_422;
    case PIX_FMT_UYVY422:
      return SLA_IMAGE_UYVY;
    case PIX_FMT_YUYV422:
      return SLA_IMAGE_YUYV;
    case PIX_FMT_GRAY8:
      return SLA_IMAGE_G8;
    case PIX_FMT_GRAY16LE:
      return SLA_IMAGE_G16;
    case PIX_FMT_BGR24:
      return SLA_IMAGE_C24_PACKED;
    case PIX_FMT_BGRA:
      return SLA_IMAGE_C32_PACKED;
  }
}


static s32 SLAImageTypeBytesPerPixel(SLA_IMAGE_TYPE sltype)
{
  switch(sltype){
    default:
    case SLA_IMAGE_UNKNOWN:
    case SLA_IMAGE_YUV_420:
    case SLA_IMAGE_YUV_422:
    case SLA_IMAGE_G8:
    case SLA_IMAGE_NV12:
      return 1;
    case SLA_IMAGE_UYVY:
    case SLA_IMAGE_YUYV:
      return 2;
    case SLA_IMAGE_C24_PACKED:
      return 3;
    case SLA_IMAGE_C32_PACKED:
      return 4;
  }
}


static FFSTATE TASK_find_input_format(FFCameraData *cam)
{
  if(!cam->skipOpen) {
    cam->pFormatCtx = avformat_alloc_context( );

    av_dict_set(&cam->ioptions, "fflags", "nobuffer", 0);
    av_dict_set(&cam->ioptions, "fflags", "igndts", 0);

    cam->fmt = NULL;
    if(cam->inputType == INPUT_DEVICE)
    {
      // To get a list of devices, run from dos:  "ffmpeg -list_devices true -f dshow -i dummy"
      // Add the name of your video input device to the list here
      char dshowNames[10][80] = {"video=Integrated Camera", // Built in camera, use "-i 0"
                                 "video=USB2.0 ATV",        // EasyCap USB camera, use "-i 1"
                                 "video=USB Video Device",  // Logitech - Jeremy's, use "-i 2"
                                 "",
                                 "",
                                 "",
                                 "",
                                 "",
                                 "",
                                 ""};
      avdevice_register_all();
      cam->fmt = av_find_input_format("dshow");
      strcpy(cam->fName, dshowNames[atol(cam->fName)]);
      av_dict_set(&cam->ioptions, "video_size", "640x480", 0);
      av_dict_set(&cam->ioptions, "framerate", "30000/1001", 0);
    }
  }
  return TASK_OPEN_INPUT;
}

static FFSTATE TASK_open_input(FFCameraData *cam)
{
  int rv;
  if(!cam->skipOpen){
    if(cam->inputType != INPUT_DEVICE){
      rv = avio_open(&cam->pIOCtx, cam->fName, AVIO_FLAG_READ);
      if(rv<0)
        return TASK_ERROR;

      cam->pFormatCtx->pb = cam->pIOCtx;
    }

    rv = avformat_open_input(&cam->pFormatCtx, cam->fName, cam->fmt, &cam->ioptions);
    if(cam->ioptions){
      AVDictionaryEntry *t = NULL;
      while ((t = av_dict_get(cam->ioptions, "", t, AV_DICT_IGNORE_SUFFIX)) != NULL) {
        SLATrace("Option: %s:%s\n",t->key, t->value);
      }
      av_dict_free(&cam->ioptions);
    }
    if(rv!=0) {
      if(rv == AVERROR_EXIT) {
        // callback-based timeout
        return TASK_FIND_INPUT_FORMAT;
      }

      char estr[1000] = {0};
      av_strerror(rv, estr, 1000);
      SLATrace("FFMPEG: %s\n", estr);
      return TASK_ERROR; // Couldn't open file
    }
  }
  return TASK_FIND_STREAM_INFO;
}

static FFSTATE TASK_find_stream_info(FFCameraData *cam)
{
  int rv;

  rv = avformat_find_stream_info(cam->pFormatCtx, NULL);
  if(rv<0) {
    return TASK_ERROR; // Couldn't find stream information
  }

  s32 i;

  // Find the first video stream
  cam->videoStream = cam->klvStream = -1;
  for(i=0; i<(s32)cam->pFormatCtx->nb_streams; i++) {
    switch(cam->pFormatCtx->streams[i]->codec->codec_type)
    {
    case AVMEDIA_TYPE_VIDEO:
      cam->videoStream=i;
      break;
    case AVMEDIA_TYPE_UNKNOWN:  ///< Usually treated as AVMEDIA_TYPE_DATA
    case AVMEDIA_TYPE_AUDIO:
    case AVMEDIA_TYPE_DATA:          ///< Opaque data information usually continuous
    case AVMEDIA_TYPE_SUBTITLE:
    case AVMEDIA_TYPE_ATTACHMENT:    ///< Opaque data information usually sparse
    case AVMEDIA_TYPE_NB:
    default:
        cam->klvStream = cam->videoStream+1;
        cam->pFormatCtx->streams[cam->klvStream]->need_parsing = AVSTREAM_PARSE_NONE;
    }
  }

  if(cam->videoStream==-1){
    printf("No video stream found\n");
    return TASK_ERROR; // Didn't find a video stream
  }
  return TASK_OPEN2;
}

static FFSTATE TASK_open2(FFCameraData *cam)
{
  int bypassCodecOpen = 0;

  cam->frameCount = cam->byteCount = cam->videoByteCount = cam->klvByteCount = 0;
  cam->stats.MaxFrameBytes = 0;
  cam->stats.MinFrameBytes = 10000000;
  cam->stats.KeyFrames = 0;
  cam->stats.IFrames = cam->stats.BFrames = cam->stats.PFrames = cam->stats.OtherFrames = 0;

  if(cam->inputType == INPUT_NETWORK){
    switch(cam->lastStreamType) {
      case SLA_UDP_VIDEO_PROTOCOL_MPEG2:
        cam->pCodec = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
        break;
      case SLA_UDP_VIDEO_PROTOCOL_MPEG4:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2MPEG4:
        cam->pCodec = avcodec_find_decoder(AV_CODEC_ID_MPEG4);
        break;
      case SLA_UDP_VIDEO_PROTOCOL_H264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPH264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2H264:
        cam->pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
        break;
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEG:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEGRAW:
        cam->pCodec = avcodec_find_decoder(AV_CODEC_ID_MJPEG);
        break;
      default:
        //SLTrace("Unknown video protocol %d\n", cam->lastStreamType);
        return TASK_READ_FRAME;
        break;
    }

    cam->pCodecCtx = avcodec_alloc_context3(cam->pCodec);

    // Initialize to unknown size (0x0)
    cam->wide = cam->pCodecCtx->width;
    cam->high = cam->pCodecCtx->height;   
  } else {
    // Get a pointer to the codec context for the video stream
    cam->pCodecCtx = cam->pFormatCtx->streams[cam->videoStream]->codec;

    // Find the decoder for the video stream
    cam->pCodec = avcodec_find_decoder(cam->pCodecCtx->codec_id);

    cam->wide = cam->pCodecCtx->width;
    cam->high = cam->pCodecCtx->height;
  }
  if(!bypassCodecOpen){
    if(cam->pCodec==NULL) {
      SLATrace("Unsupported codec!\n");
      return TASK_ERROR; // Codec not found
    }

    // Open codec
    av_dict_set(&cam->ioptions, "flags", "low_delay", 0);
    int rv;
    if((rv=avcodec_open2(cam->pCodecCtx, cam->pCodec, &cam->ioptions))<0) {
      return TASK_ERROR; // Could not open codec
    }

    if(cam->ioptions){
      AVDictionaryEntry *t = NULL;
      while ((t = av_dict_get(cam->ioptions, "", t, AV_DICT_IGNORE_SUFFIX)) != NULL) {
        SLATrace("Option: %s:%s\n",t->key, t->value);
      }
      av_dict_free(&cam->ioptions);
    }
  }

  // Allocate video frame (raw frame)
  cam->pFrame=av_frame_alloc();

  // Allocate an AVFrame structure (converted & scaled to YUV)
  cam->pFrameOut=av_frame_alloc();
  if(cam->pFrameOut==NULL)
    return TASK_ERROR;

  int numBytesIn, numBytesOut;

  cam->ffOutType = SLAImageTypeToFFmpeg(cam->slOutType);
  // TODO: is there a way to know "best" input format for a codec?
  cam->ffInType = PIX_FMT_YUV420P;
  cam->slInType = SLA_IMAGE_YUV_420;


  // Allocate largest image type so buffer only needs to be resized
  // if dimensions change: don't have to worry about type
  numBytesIn=avpicture_get_size(PIX_FMT_BGRA, FFMPEG_MAX_WIDTH, FFMPEG_MAX_HEIGHT);
  numBytesOut=avpicture_get_size(PIX_FMT_BGRA, FFMPEG_MAX_WIDTH, FFMPEG_MAX_HEIGHT);
  cam->buffer=(uint8_t *)av_malloc(numBytesIn*sizeof(uint8_t));
  if(!cam->buffer)    return TASK_ERROR; // allocation failed
  cam->bufferOut=(uint8_t *)av_malloc(numBytesOut*sizeof(uint8_t));
  if(!cam->bufferOut)    return TASK_ERROR; // allocation failed

  // Assign appropriate parts of buffer to image planes in pFrameRGB
  // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
  // of AVPicture
  //avpicture_fill((AVPicture *)cam->pFrame, cam->buffer, cam->ffInType,
  //  cam->pCodecCtx->width, cam->pCodecCtx->height);
  //avpicture_fill((AVPicture *)cam->pFrameOut, cam->bufferOut, cam->ffOutType,
  //  cam->pCodecCtx->width, cam->pCodecCtx->height);
  avpicture_fill((AVPicture *)cam->pFrame, cam->buffer, cam->ffInType,
    FFMPEG_MAX_WIDTH, FFMPEG_MAX_HEIGHT);
  avpicture_fill((AVPicture *)cam->pFrameOut, cam->bufferOut, cam->ffOutType,
    FFMPEG_MAX_WIDTH, FFMPEG_MAX_HEIGHT);
  if(cam->pFrameOut->data[0])
    SLAMemset(cam->pFrameOut->data[0], 128, cam->pFrameOut->linesize[0]);
  if(cam->pFrameOut->data[1])
    SLAMemset(cam->pFrameOut->data[1], 128, cam->pFrameOut->linesize[1]);
  if(cam->pFrameOut->data[2])
    SLAMemset(cam->pFrameOut->data[2], 128, cam->pFrameOut->linesize[2]);

  // Initialize conversion context
  cam->img_convert_ctx = NULL;

  if(cam->inputType == INPUT_FILE) {
    // Seek to the correct position for starting
    if(cam->frame<cam->startFrame) {
      cam->frame = cam->startFrame;
      double seconds = 1.0 * cam->frame/30; // Assume 30fps
      int64_t seekTarget = (int64_t) (AV_TIME_BASE * seconds);
      AVRational tbq = {1, AV_TIME_BASE};
      seekTarget = av_rescale_q(seekTarget, tbq,
                    cam->pFormatCtx->streams[cam->videoStream]->time_base);
      //int rv = av_seek_frame(cam->pFormatCtx, cam->videoStream, seekTarget, 0);
      (void)av_seek_frame(cam->pFormatCtx, cam->videoStream, seekTarget, 0);
    }
    return TASK_READ_FRAME;
  }

  return TASK_READ_FRAME;
}

static FFSTATE TASK_read_frame_buffered(FFCameraData *cam)
{
  int rv;
  if(cam->inputType != INPUT_NETWORK)
    return TASK_READ_FRAME;
  // Read buffered packets.  When there begins to be a noticeable
  // lag, the buffered frames have all been consumed and we're into
  // live video.

  u64 t0, t1, diff;
  SLAGetMHzTime(&t0);
  rv = av_read_frame(cam->pFormatCtx, &cam->packet);
  SLAGetMHzTime(&t1);
  diff = t1-t0;

  if(rv==AVERROR_EXIT || cam->timeExpired)
    return TASK_TIMEOUT;
  if(rv<0)
    return TASK_ERROR;

  // Keep reading the buffered frames until done
  if(diff<5000)
    return TASK_READ_FRAME_BUFFERED;
  else
    return TASK_READ_FRAME;
}

static s32 nFrames = 0;
static FFSTATE TASK_read_frame(FFCameraData *cam)
{
  int rv;

  if(cam->inputType == INPUT_NETWORK){
    if(cam->compressedFrame.len==0){  // if don't already have a frame from codec change detection below
      SLStatus ret = SLADemuxNextFrame(cam->udpRx, &cam->compressedFrame, READ_FRAME_TIMEOUT/1000);
      if(ret == SLA_TIMEOUT)
        return TASK_TIMEOUT;
      if(ret == SLA_TERMINATE)
        return TASK_EOF;
      if(ret != SLA_SUCCESS)
        return TASK_ERROR;
      if(cam->compressedFrame.streamType != cam->lastStreamType && !SLAIsMetaDataProtocol(cam->compressedFrame.streamType)){
        cam->lastStreamType = cam->compressedFrame.streamType;
        if(cam->pCodecCtx) {
          avcodec_close(cam->pCodecCtx);
          av_free(cam->pCodecCtx);
        }
        return TASK_OPEN2;
      }
    }
    if(!SLAIsMetaDataProtocol(cam->compressedFrame.streamType))
      cam->lastStreamType = cam->compressedFrame.streamType;

    // Fill up the AVPacket
    av_init_packet(&cam->packet);
    if(cam->compressedFrame.missedPacket) {
      cam->packet.flags = AV_PKT_FLAG_CORRUPT;
      SLATrace("*** corrupt frame\n");
    }
    cam->packet.data = cam->compressedFrame.buffer;
    cam->packet.size = cam->compressedFrame.len;
    cam->packet.pts = cam->compressedFrame.PTS;
    cam->packet.stream_index = cam->compressedFrame.PID;

    // Get PIDs from demuxer
    switch(cam->compressedFrame.streamType) {
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEG:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEGRAW:
      case SLA_UDP_VIDEO_PROTOCOL_MPEG2:
      case SLA_UDP_VIDEO_PROTOCOL_MPEG4:
      case SLA_UDP_VIDEO_PROTOCOL_H264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPH264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2H264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2MPEG4:
          cam->videoStream = cam->compressedFrame.PID;
        break;
      case SLA_UDP_VIDEO_PROTOCOL_KLV_METADATA:
        cam->klvStream = cam->compressedFrame.PID;
        break;
      case SLA_UDP_VIDEO_PROTOCOL_SLA_METADATA:
        // stream_index should be 2 for SLA diag data.
        cam->packet.stream_index = 2;
        break;
    }
  } else {
    rv = av_read_frame(cam->pFormatCtx, &cam->packet);

    if(rv==AVERROR_EXIT || cam->timeExpired)
      return TASK_TIMEOUT;
    if(rv==AVERROR_EOF)
      return TASK_LOOP;
    if(rv<0)
      return TASK_ERROR;
  }

  nFrames++;
  cam->byteCount+=cam->packet.size;

  // Is this a packet from the video stream?
  if(cam->packet.stream_index==cam->videoStream) {
    // More processing needed: update state
    cam->videoByteCount += cam->packet.size;
    cam->stats.MaxFrameBytes = SLMAX(cam->stats.MaxFrameBytes, (u32)cam->packet.size);
    cam->stats.MinFrameBytes = SLMIN(cam->stats.MinFrameBytes, (u32)cam->packet.size);

    return TASK_DECODE_VIDEO2;
  } else {
    // Not video, could be KLV
    u8 *buf;
    buf = cam->packet.data;
    if(buf){
      s32 rv;
      SLAMemcpy(&cam->klvRecent, &KLVUnknown, sizeof(KLVData));
      rv = ReadKlvFrame(&cam->klvRecent, buf, cam->packet.size, 0);
      if(!rv){
        // If decode fails, try again with offset 5 -- this is to support
        // file-based reading using ffmpeg demux which may return buffer that
        // includes 5-byte mpeg2 header.
        rv = ReadKlvFrame(&cam->klvRecent, buf, cam->packet.size, 5);
      }
      if(rv) {
        cam->klvByteCount += cam->packet.size;
        SLCopyChangedKLV(&cam->klv, &cam->klvRecent);
        if(cam->klvCallBack){
          cam->klvCallBack(&cam->klv, &cam->klvRecent, cam->callBackContext);
        }
      }
      else {
        // could be SightLine Applications private data
        u32 streamType = 0;
        if (cam->pFormatCtx && cam->pFormatCtx->nb_streams>2) {
          // codec_tag is the streamType. Should be 0x88 .. 0x8F.
          streamType = cam->pFormatCtx->streams[2]->codec->codec_tag;
        }
        // Is this SLA private diagnostic data?
        if (cam->mtsPrivCallback) {
          if (cam->packet.stream_index == 2 // SLA stream appears at index==2.
            || (streamType >= SLA_DG_STREAM_TYPE_MIN && streamType <= SLA_DG_STREAM_TYPE_MAX))
          {
            if (streamType == 0)
              streamType = SLA_DG_STREAM_TYPE_MIN;
            cam->mtsPrivCallback(buf, cam->packet.size, cam->callBackContext, cam->packet.pts);
          }
        }
      }
      cam->compressedFrame.len = 0;
    } else {
      SLATrace("buffer = NULL\n");
    }
    av_free_packet(&cam->packet);
    return TASK_READ_FRAME;
  }
}

static FFSTATE TASK_seek_frame(FFCameraData *cam, double timeStamp)
{
  if(cam->inputType == INPUT_NETWORK){
	  //what do we do if we seek a frame in with a network input.
	  //dvr support.  one day	  
  }  else {
      double seconds = timeStamp;
      int64_t seekTarget = (int64_t) (AV_TIME_BASE * seconds);
      AVRational tbq = {1, AV_TIME_BASE};
      seekTarget = av_rescale_q(seekTarget, tbq,
                    cam->pFormatCtx->streams[cam->videoStream]->time_base);
      
	 int rv = av_seek_frame(cam->pFormatCtx, cam->videoStream, cam->pFormatCtx->streams[cam->videoStream]->start_time + seekTarget, 0);
	  avcodec_flush_buffers (cam->pCodecCtx); //is this doing what I expect it to.
    if(rv==AVERROR_EXIT || cam->timeExpired)
      return TASK_TIMEOUT;
    if(rv==AVERROR_EOF)
      return TASK_LOOP;
    if(rv<0)
      return TASK_ERROR;

	//todo rg get the frame and decode the frame
  }
  return TASK_READ_FRAME;
}

void SLReadH264(u8 *nal, int len);

static FFSTATE TASK_decode_video2(FFCameraData *cam)
{
  int rv = 0, frameFinished = 0;

  if(cam->skippedFrame==0 && cam->compressedFrame.missedPacket){
    // Skipping frame
    cam->skippedFrame = 1;
    rv = 1;
    frameFinished = 1;
  } else {
    {
      //SLReadH264(cam->packet.data, cam->packet.size);

      rv = avcodec_decode_video2(cam->pCodecCtx, cam->pFrame, &frameFinished, &cam->packet);
#if 0
      if(rv<=0){
        char ebuf[1024];
        av_make_error_string(ebuf, sizeof(ebuf), rv);
        SLATrace("failed to decode %d bytes\n",cam->packet.size);
      }
#endif
      cam->inputFormat = cam->pCodecCtx->pix_fmt;
    }
    cam->skippedFrame = 0;
  }
  cam->compressedFrame.len = 0;

  if(rv <= 0) {
    if(cam->inputType == INPUT_NETWORK) {
      return TASK_READ_FRAME;
    } else
    // Could be a resolution change (which used to work, but
    // does no longer in latest 9/3/2012 version of ffmpeg
    // Try to close and reopen codec
    avcodec_close(cam->pCodecCtx);
    if (avcodec_open2(cam->pCodecCtx, cam->pCodec, NULL)<0) {
      // Not sure what to do if reopening fails
      return TASK_ERROR;
    }
    return TASK_READ_FRAME;
  }
  if(rv > 0) {
    // Did we get a video frame?
    if (frameFinished) {
      if (cam->pFrame->key_frame)
        cam->stats.KeyFrames++;
      switch (cam->pFrame->pict_type){
      case AV_PICTURE_TYPE_I:
        cam->stats.IFrames++;
        break;
      case AV_PICTURE_TYPE_P:
        cam->stats.PFrames++;
        break;
      case AV_PICTURE_TYPE_B:
        cam->stats.BFrames++;
        break;
      default:
        cam->stats.OtherFrames++;
      }
      return TASK_READ_FRAME_FINISHED;
    }
    else
      return TASK_READ_FRAME;
  }
  return TASK_ERROR;
}

static FFSTATE TASK_reopen2(FFCameraData *cam)
{
  // Could be a resolution change (which used to work, but
  // does no longer in latest 9/3/2012 version of ffmpeg
  // Try to close and reopen codec
  avcodec_close(cam->pCodecCtx);
  if(avcodec_open2(cam->pCodecCtx, cam->pCodec, NULL)<0) {
    // Not sure what to do if reopening fails
    return TASK_ERROR;
  }
  return TASK_DECODE_VIDEO2;
}

// Determine fullHeight, fullWidth, downsample factor from frame size
static void getFullSize(int *fullHigh, int *fullWide, int *ds, int high, int wide, bool resamplePAL, int upSample)
{
  *fullHigh = high;
  *fullWide = wide;
  *ds = 1;

#if 1
  // Controlled upsample, auto matching causes problems for 320x240 size, etc.
  if(upSample==2 || upSample==4) {
    *fullHigh = SLMIN(high*upSample, FFMPEG_MAX_HEIGHT);
    *fullWide = SLMIN(wide*upSample, FFMPEG_MAX_WIDTH);
    *ds = upSample;
  }
#else
  // Look for downsamples of common sizes to work with compression downsample feature
  // TODO:  This will incorrectly upsample some formats such as 320x240 camera that has not been downsampled
  // TODO:  Are 480x720 or 576x720 really valid any more?
  int highs[] = {480, 480, 576, 576,  720, 720, 1080};
  int wides[] = {640, 720, 767, 720, 1280, 960, 1920};
  int nsize = sizeof(highs)/sizeof(highs[0]);
  for(int shift=1; shift<=2; shift++) {
    for(int n=0; n<nsize; n++) {
      if((highs[n]>>shift)==high && (wides[n]>>shift)==wide) {
        *fullHigh = high<<shift;
        *fullWide = wide<<shift;
        *ds = 1<<shift;
        break;
      }
    }
  }
#endif

  if (resamplePAL && high==576 && wide==720) {
    // Resample pal 720 wide to 768 wide
    *fullWide = 768;
    *fullHigh = high;
  }
}

static FFSTATE TASK_read_frame_finished(FFCameraData *cam)
{
  int fullHigh, fullWide, ds;

  if(cam->inputType == INPUT_NETWORK) {
    getFullSize(&fullHigh, &fullWide, &ds, cam->pFrame->height, cam->pFrame->width, cam->resamplePAL, cam->upSample);
  } else {
    fullHigh = cam->pFrame->height;
    fullWide = cam->pFrame->width;
  }
  
  // Resize image if needed (buffer is already allocated at max size)
  // This should only happen at startup or when switching channels (eg. display NTSC, then display PAL)
  if(true/*fullHigh != cam->high || fullWide != cam->wide*/){
    avpicture_fill((AVPicture *)cam->pFrameOut, cam->bufferOut, cam->ffOutType, fullWide, fullHigh);
    s32 ystride = cam->pFrameOut->linesize[0]/SLAImageTypeBytesPerPixel(cam->slOutType);
    s32 uvstride = cam->pFrameOut->linesize[1];
    SLASetupImage(&cam->imageOut, cam->slOutType, fullHigh, fullWide, ystride, uvstride,
                    cam->pFrameOut->data[0], cam->pFrameOut->data[1], cam->pFrameOut->data[2]);

  }
  cam->high = fullHigh;
  cam->wide = fullWide;

  cam->frameCount++;
  u64 tic;
  SLAGetMHzTime(&tic);
  u64 diff = tic - cam->tic0;

  // Every 1 second, notify application about framerate and bitrate
  if(diff > 1000000) {
    cam->stats.FrameRate = 1000000.0f*cam->frameCount/diff;
    cam->stats.TotalBitRate = 8000.0f*cam->byteCount/diff;
    cam->stats.VideoBitRate = 8000.0f*cam->videoByteCount/diff;
    cam->stats.KlvBitRate = 8000.0f*cam->klvByteCount/diff;

    if (cam->inputType == INPUT_NETWORK) {
      // demux was via SLAUdpReceive
      char *name;
      switch (cam->lastStreamType){
      case SLA_UDP_VIDEO_PROTOCOL_MPEG2:
      case SLA_UDP_VIDEO_PROTOCOL_MPEG4:
      case SLA_UDP_VIDEO_PROTOCOL_H264:
        name = "mpegts";
        break;
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2MPEG4:
      case SLA_UDP_VIDEO_PROTOCOL_RTPH264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMP2H264:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEG:
      case SLA_UDP_VIDEO_PROTOCOL_RTPMJPEGRAW:
        name = "rtp";
        break;
      default:
        name = "unknown";
        break;
      }
      strncpy(cam->stats.Encapsulation, name, STATS_NAME_LENGTH);
    }
    else {
      if (cam->pFormatCtx && cam->pFormatCtx->iformat)
        strncpy(cam->stats.Encapsulation, cam->pFormatCtx->iformat->name, STATS_NAME_LENGTH);
      else
        strncpy(cam->stats.Encapsulation, "Unknown", STATS_NAME_LENGTH);
    }
    cam->stats.Encapsulation[STATS_NAME_LENGTH - 1] = 0;
    if (cam->pCodecCtx) {
      switch (cam->pCodecCtx->profile){
      case FF_PROFILE_H264_BASELINE:
        cam->stats.Profile = SLA_PROFILE_BASELINE;
        break;
      case FF_PROFILE_H264_CONSTRAINED_BASELINE:
        cam->stats.Profile = SLA_PROFILE_CONSTRAINED_BASELINE;
        break;
      case FF_PROFILE_H264_MAIN:
        cam->stats.Profile = SLA_PROFILE_MAIN;
        break;
      case FF_PROFILE_H264_EXTENDED:
        cam->stats.Profile = SLA_PROFILE_EXTENDED;
        break;
      case FF_PROFILE_H264_HIGH:
        cam->stats.Profile = SLA_PROFILE_HIGH;
        break;
      default:
        cam->stats.Profile = SLA_PROFILE_NONE;
        break;
      }

      const AVCodecDescriptor *cdesc = avcodec_descriptor_get(cam->pCodecCtx->codec_id);

      strncpy(cam->stats.Codec, cdesc->name, STATS_NAME_LENGTH);
    }
    else
      strncpy(cam->stats.Codec, "Unknown", STATS_NAME_LENGTH);
    cam->stats.Codec[STATS_NAME_LENGTH - 1] = 0;

    if(cam->statsCallBack)
      cam->statsCallBack(&cam->stats, cam->statsContext);
    cam->tic0 = tic;
    cam->frameCount = cam->byteCount = cam->videoByteCount = cam->klvByteCount = 0;
    cam->stats.MaxFrameBytes = 0;
    cam->stats.MinFrameBytes = 10000000;
    cam->stats.KeyFrames = 0;
    cam->stats.IFrames = cam->stats.PFrames = cam->stats.BFrames = cam->stats.OtherFrames;
  }

  cam->skipDisplay = 0;
  if(cam->inputType == INPUT_NETWORK) {
    // If the UDP receiver is getting backed up skip displaying
    // a frame to let system catch up
    SLA_UDP_STATUS stat;
    SLAUDPStatus(cam->udpRx, &stat);
    if(cam->skipDisplay==0) {
      cam->skipDisplay = 0; //stat.backedUp;
    }
  }

  if(cam->skipDisplay){
    if(!cam->noRelease){
      SLASemPost(cam->processingSem);
    }
  } else {
    cam->img_convert_ctx = 
      sws_getCachedContext(cam->img_convert_ctx,
                            cam->pFrame->width, cam->pFrame->height, 
                            cam->inputFormat, 
                            fullWide, fullHigh, cam->ffOutType,
                            SWS_FAST_BILINEAR,
                            NULL, NULL, NULL);
    if(cam->img_convert_ctx == NULL) {
      av_free_packet(&cam->packet);
      return TASK_ERROR;
    }

    if (cam->inputFormat == PIX_FMT_YUV422P && cam->slOutType == SLA_IMAGE_YUV_420 && cam->pFrame->linesize[0] >= cam->pFrameOut->linesize[0] &&
      cam->pFrame->height>=cam->high) {
      // NOTE:  Just clipping a bigger image to the cam size, not resizing
      // 422 to 420 - just change the uv stride
      s32 ystride = cam->pFrame->linesize[0];
      s32 uvstride = cam->pFrame->linesize[1];
      SLASetupImage(&cam->imageOut, cam->slOutType, cam->high, cam->wide, ystride, uvstride*2,
                      cam->pFrame->data[0], cam->pFrame->data[1], cam->pFrame->data[2]);
    } else {
      // Convert the image from its native format to output format
      sws_scale(cam->img_convert_ctx, cam->pFrame->data, 
                cam->pFrame->linesize, 0, 
                cam->pFrame->height, 
                cam->pFrameOut->data, cam->pFrameOut->linesize);
      s32 ystride = cam->pFrameOut->linesize[0]/SLAImageTypeBytesPerPixel(cam->slOutType);
      s32 uvstride = cam->pFrameOut->linesize[1];
      SLASetupImage(&cam->imageOut, cam->slOutType, cam->high, cam->wide, ystride, uvstride,
                      cam->pFrameOut->data[0], cam->pFrameOut->data[1], cam->pFrameOut->data[2]);
    }

    SLAImage *outImage = &cam->imageOut;
    outImage->type = cam->slOutType;

    if(cam->callBack) {
      cam->callBack(outImage, cam->callBackContext, 0);
    }
    // Throttle file input
    if(cam->inputType == INPUT_FILE)
      SLASleep(25);
  } // !cam->skipDisplay

  cam->frame++;
  av_free_packet(&cam->packet);

  return TASK_READ_FRAME;
}

static FFSTATE TASK_timeout(FFCameraData *cam)
{
  cam->stats.FrameRate = 0;
  cam->stats.TotalBitRate = 0;
  cam->stats.VideoBitRate = 0;
  cam->stats.KlvBitRate = 0;
  if(cam->statsCallBack)
    cam->statsCallBack(&cam->stats, cam->statsContext);

  SLAGetMHzTime(&cam->tic0);

  if(cam->pFrameOut) {
    if(cam->callBack)
      cam->callBack(NULL, cam->callBackContext, 0);
    cam->frame++;
  }

  if(cam->inputType == INPUT_NETWORK){
    // Cause the codec to close and reopen to avoid
    // flicker of old video when a stream restarts
    cam->compressedFrame.streamType = SLA_UDP_VIDEO_PROTOCOL_NONE;
    cam->compressedFrame.len = 0;
    return TASK_READ_FRAME;
  } else {
    avformat_close_input(&cam->pFormatCtx);
    return TASK_FIND_INPUT_FORMAT;
  }
}

static FFSTATE TASK_Loop(FFCameraData *cam)
{
  FFSTATE nextState;
  if(--cam->nLoop > 0) {
    double seconds = 1.0 * (double)cam->startFrame/30; // Assume 30fps
    int64_t seekTarget = (int64_t) (AV_TIME_BASE * seconds);
    AVRational tbq = {1, AV_TIME_BASE};
    seekTarget = av_rescale_q(seekTarget, tbq,
      cam->pFormatCtx->streams[cam->videoStream]->time_base);
    int rv = av_seek_frame(cam->pFormatCtx, -1, seekTarget, AVSEEK_FLAG_ANY);
    (void)rv;
    cam->frame = cam->startFrame;
    nextState = TASK_READ_FRAME;
  } else {
    nextState = TASK_EOF;
  }
  return nextState;
}

static int ffmpegTask(void *pCamera)
{
  FFCameraData *cam = (FFCameraData*)pCamera;
  FFSTATE ffState = TASK_FIND_INPUT_FORMAT, nextState;
  FFMpegControlPacket pkt;

  // Allow callbacks, etc to get initialized before
  // video processing starts up
  SLASleep(100);

  if(cam->inputType == INPUT_NETWORK) {
    char proto[80] = {0};
    int proto_size = 80;
    char authorization[80] = {0};
    int authorization_size = 80;
    char path[80] = {0};
    int path_size = 80;

    char hostname[80] = {0};
    int port = 0;

    av_url_split(proto,proto_size,authorization,authorization_size, hostname, sizeof(hostname)-1, &port, path, path_size, cam->fName);
    if (isRTSPURL(cam->fName)) {
      cam->udpRx = SLARtspOpenURL(cam->fName, &cam->rtspClient);
      if (!cam->udpRx) {
        printf("Unable to create RTSP client for url : %s\n", cam->fName);
        SLASemPost(cam->taskDoneSem);
        return -1;
      }
    }
    else if(cam->useSlDemux) {
      cam->udpRx = SLAInitUDPReceive(100, cam->fName, port, cam->useSlDemux);
      if(!cam->udpRx) {
        printf("Unable to create UDP receiver.\n");
        SLASemPost(cam->taskDoneSem);
        return -1;
      }
    } 
    else if(port >=0 ) {
      cam->udpRx = SLAInitUDPReceive(100, hostname, port);
      if(!cam->udpRx) {
        printf("Unable to create UDP receiver.\n");
        SLASemPost(cam->taskDoneSem);
        return -1;
      }
    } 
    else {
      printf("Unable to open network stream: %s.\n", cam->fName);
      SLASemPost(cam->taskDoneSem);
      return -1;
    }
    ffState = TASK_OPEN2;
  }

  // Lock GetImageInfo out until stream/file is
  // located

  while(!cam->done){
    // Look for input
    if(SLAMbxPend(cam->hmbx, &pkt, 0) ) {
      // Command packet
      switch(pkt.type){
        case FF_CONTROL_NAME:
          if(cam->done)
            break;
          if (isRTSPURL(cam->fName)) {
            if (cam->rtspClient != NULL) {
              cam->rtspClient->stopStreaming(false);
              delete cam->rtspClient;
              cam->rtspClient = NULL;
            }
          }
          strncpy(cam->fName, pkt.name, sizeof(cam->fName));
          if(cam->inputType == INPUT_NETWORK){
            char hostname[80];
            int port;
            av_url_split(0,0,0,0,hostname, sizeof(hostname)-1, &port, 0, 0, cam->fName);
            SLAReinitUDPReceive(cam->udpRx, hostname, port);
          } else {
            ffState = TASK_TIMEOUT;
          }
          break;
        case FF_CONTROL_INDEX:
          break;
        case FF_CONTROL_SAVEFILE:
          cam->dumpFile = (FILE*)pkt.ptr;
          break;
		case FF_CONTROL_SEEK:
			double timeStamp;
			memcpy(&timeStamp, pkt.name,sizeof(double)); 
			 ffState = TASK_seek_frame(cam, timeStamp);
          break;
      }
    } else {
      // Run through state machine
      switch(ffState){
        case TASK_FIND_INPUT_FORMAT:
          // Lock semaphore if it was previously unlocked
          if(cam->frame>0) {
            SLASemPend(cam->camSemaphore, 10);
          }
          cam->frame = 0;
          nextState = TASK_find_input_format(cam);
          break;
        case TASK_OPEN_INPUT:
#ifdef WIN32
          SLASleep(50);
#endif
          nextState = TASK_open_input(cam);
          break;
        case TASK_FIND_STREAM_INFO:
          nextState = TASK_find_stream_info(cam);
          break;
        case TASK_OPEN2:
          nextState = TASK_open2(cam);
          break;
        case TASK_READ_FRAME_BUFFERED:
          nextState = TASK_read_frame_buffered(cam);
          break;
        case TASK_READ_FRAME:
          {
          nextState = TASK_read_frame(cam);
          }
          break;
        case TASK_DECODE_VIDEO2:
          {
          nextState = TASK_decode_video2(cam);
          }
          break;
        case TASK_REOPEN2:
          nextState = TASK_reopen2(cam);
          break;
        case TASK_READ_FRAME_FINISHED:
          {
          nextState = TASK_read_frame_finished(cam);

          // OK for GetImageInfo to access high, wide, type members
          if(cam->frame-cam->startFrame==1)
            SLASemPost(cam->camSemaphore);
          // Lock semaphore, unlocked by ::Release
          if(!cam->noRelease) {
            SLASemPost(cam->imageSem);
            while(!cam->done && !SLASemPend(cam->processingSem, 200))
              SLASleep(100);
          }
          }
          break;
        case TASK_TIMEOUT:
          // Update stats and send a blank frame
          nextState = TASK_timeout(cam);
          break;
        case TASK_LOOP:
          nextState = TASK_Loop(cam);
          break;

      }
      // Check for end of file
      if((cam->inputType==INPUT_FILE || cam->useSlDemux) && ffState == TASK_EOF)
        break;

      // Could implement error handling TODO: free context
      if(nextState == TASK_ERROR){
        cam->compressedFrame.len = 0;
        if(cam->inputType == INPUT_NETWORK) {
          nextState = TASK_REOPEN2;
        } else
          nextState = TASK_FIND_INPUT_FORMAT;
      }

      ffState = nextState;
    }
  }

  // File is done, continue to send blank images to display thread
  while(!cam->done && cam->callBack){
    cam->callBack(NULL, cam->callBackContext, 1);
    SLASleep(30);
  }

  while(SLASemPend(cam->imageSem, 100)!=SLA_SUCCESS)
    ;

  if(cam->inputType == INPUT_NETWORK) {
    if (isRTSPURL(cam->fName)) {
      if (cam->rtspClient != NULL) {
        cam->rtspClient->stopStreaming(); // This will internally call SLADestroyUDPReceive
        delete cam->rtspClient;
        cam->rtspClient = NULL;
      }
    }
    else 
    {
      SLADestroyUDPReceive(cam->udpRx);
    }
  }

  cam->done = 1;
  cam->taskDone = 1;
  SLASemPost(cam->taskDoneSem);

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
SLADecodeFFMPEG::SLADecodeFFMPEG()
{
  Data=NULL;
}

///////////////////////////////////////////////////////////////////////////////
SLADecodeFFMPEG::~SLADecodeFFMPEG()
{
  this->Cleanup();
}

//
// Just checks to see if input is in an "acceptable" format to be a file name or path
// Look for drive letter to indicate local file or "//" to indicate network path
//
inline bool isFileName(const char *dirName)
{
  if((dirName[0]=='.') || (dirName[1]==':') || (dirName[0]=='\\' && dirName[1]=='\\')) {
    return true;
  }
  return false;
}

void SLADecodeFFMPEG::SetPALResample(bool flag)
{
  FFCameraData *cam = (FFCameraData*)Data;
  if (cam)
    cam->resamplePAL = flag;

}

///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Initialize( const char *dirName,
                                    SLA_IMAGE_TYPE outType,
                                    SLCaptureCallback callBack, void *context,
                                    s32 nLoop, s32 startFrame, s32 noRelease,
                                    SLMtsPrivCallback mtsPrivCallback,
                                    bool useSlDemux // Use SLUDPReceive demux instead of FFMPEG internal version. Decoding SLALIB diag data works better with SLUDPReceive. 
                                    )
{
  SLStatus rv = SLA_SUCCESS;  // assume fail

  if(outType == SLA_IMAGE_UNKNOWN)
    outType = SLA_IMAGE_YUV_420;

  // Second call to Initialize (presumably with different dirName)
  if(Data){
    FFMpegControlPacket pkt;
    FFCameraData *cam = (FFCameraData*)Data;

    pkt.type = FF_CONTROL_NAME;
    strncpy(pkt.name, dirName, sizeof(pkt.name));
    SLAMbxPost(cam->hmbx, &pkt, SEM_FOREVER);

    return SLA_SUCCESS;
  }

  if(!Data) {
    Data = SLACalloc(sizeof(FFCameraData));
    FFCameraData *cam = (FFCameraData*)Data;
    cam->camSemaphore = SLASemCreate(1, "Cam FFMPEG sem");
    cam->taskDoneSem = SLASemCreate(0);
    cam->imageSem = SLASemCreate(0);
    cam->processingSem = SLASemCreate(0);
    cam->useSlDemux = useSlDemux;
    cam->resamplePAL = false;
    cam->upSample = 1;

    // Set up compression buffer
    cam->compressedFrame.buffer = cam->cFrameData;
    cam->compressedFrame.maxBufferLen = sizeof(cam->cFrameData);

    av_register_all();        // Formats and protocols
    avcodec_register_all();   // Codecs
    avformat_network_init();
  }
  FFCameraData *cam = (FFCameraData*)Data;

  SLASemPend(cam->camSemaphore, -1);
  cam->noRelease = noRelease;
  if(callBack) {
    cam->callBack = callBack;
    cam->callBackContext = context;
    cam->cam = this;
  }
  cam->hmbx = SLAMbxCreate(sizeof(FFMpegControlPacket), 10, "hmbxFFMPEG");

  cam->taskDone = 0;

  cam->slOutType = outType;
  cam->nLoop = nLoop==0 ? 1 : nLoop;
  cam->startFrame = startFrame;
  cam->frame = 0;
  strncpy(cam->fName, dirName, sizeof(cam->fName));

  cam->inputType = INPUT_NETWORK;
  cam->lastStreamType = SLA_UDP_VIDEO_PROTOCOL_NONE;

  // Check if file name or camera (0)
  if(strlen(dirName)>1) {
    if (isUDPURL(dirName) || isRTSPURL(dirName)) {
      cam->inputType = INPUT_NETWORK;  // assume this an SLA produced stream
    }
    else if( isFileName(dirName) ) {
      // Sanity check that the file exists
      FILE *f = fopen(dirName, "rb");
      if(f) {   // OK
        fclose(f);
        cam->inputType = INPUT_FILE;
      } else {  // FAIL
        SLASemPost(cam->taskDoneSem);
        Cleanup();
        Data = NULL;
        return SLA_ERROR;
      }
    } else {
      cam->inputType = INPUT_FFMPEG_NETWORK;
    }
  } else {
    cam->inputType = INPUT_DEVICE;
  }
 
  if (mtsPrivCallback) {
    cam->mtsPrivCallback = mtsPrivCallback;
    cam->usePtsTimeStamp = true;
  }
  if (cam->useSlDemux) {
    cam->inputType = INPUT_NETWORK; // hack: in SLUDPReceive.cpp, this will force to read a MTS file. Search for "useSlDemux".
  }

  if(!SLACreateThread(ffmpegTask, 8*SL_DEFAULT_STACK_SIZE, "ffmpegTask", (void*)cam, SL_PRI_4)) {
    rv = SLA_FAIL;
  }
  return rv;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Cleanup( )
{
  if(!Data)
    return SLA_FAIL;

  FFCameraData *cam = (FFCameraData*)Data;

  cam->done = 1;

  FFMpegControlPacket pkt;
  pkt.type = FF_CONTROL_NAME;
  SLAMbxPost(cam->hmbx, &pkt, SEM_FOREVER);

  // Wait for signal that readFramesTask has exited
  SLASemPend(cam->taskDoneSem, SEM_FOREVER);

  if(cam->dumpFile)
    fclose(cam->dumpFile);
  cam->dumpFile = 0;

  // Free the YUV image
  if(cam->buffer) av_free(cam->buffer);
  if(cam->bufferOut) av_free(cam->bufferOut);
  if(cam->pFrameOut) av_free(cam->pFrameOut);

  // Free the YUV frame
  if(cam->pFrame) av_free(cam->pFrame);

  // Close the codec
  if(cam->pCodecCtx) avcodec_close(cam->pCodecCtx);

  // Close the video file
  if(cam->pFormatCtx) avformat_close_input(&cam->pFormatCtx);

  if(cam->img_convert_ctx)
    sws_freeContext(cam->img_convert_ctx);

  if(cam->imageSem)
    SLASemDestroy(cam->imageSem);

  if(cam->processingSem)
    SLASemDestroy(cam->processingSem);

  if(cam->camSemaphore)
    SLASemDestroy(cam->camSemaphore);

  if(cam->taskDoneSem)
    SLASemDestroy(cam->taskDoneSem);

  if(cam->hmbx)
    SLAMbxDestroy(cam->hmbx);


  SLAFree(cam);
  Data = NULL;

  return SLA_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Get( SLAImage * pImage)
{
  FFCameraData *cam = (FFCameraData*)Data;
  if(!cam || !pImage)
    return SLA_ERROR;

  if(!cam->noRelease) {
    if(cam->done)
      return SLA_FAIL; 
    while(!cam->done && !SLASemPend(cam->imageSem, 2));
  }
  SLASemPend(cam->camSemaphore, -1);
  *pImage = cam->imageOut;
  SLASemPost(cam->camSemaphore);

  return SLA_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Release( SLAImage * pImage )
{
  FFCameraData *cam = (FFCameraData*)Data;
  SLASemPost(cam->processingSem);
 
  return SLA_ERROR;
}
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Reset( )
{
  return SLA_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void SLADecodeFFMPEG::SetKLVCallBack(SLKLVCallback callBack)
{
  FFCameraData *cam = (FFCameraData*)Data;
  cam->klvCallBack = callBack;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void SLADecodeFFMPEG::SetStatsCallBack(SLStatsCallback callBack, void *context)
{
  FFCameraData *cam = (FFCameraData*)Data;
  cam->statsCallBack = callBack;
  cam->statsContext = context;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void SLADecodeFFMPEG::GetImageInfo(s16 *phigh,  s16 *pwide)
{
  FFCameraData *cam = (FFCameraData*)Data;
  if(!cam) return;

  while(!cam->pCodecCtx || cam->pCodecCtx->height<=0 || cam->pCodecCtx->width<=0)
    SLASleep(100);

  if(phigh)
    *phigh = cam->pCodecCtx->height;
  if(pwide)
    *pwide = cam->pCodecCtx->width;
}

SLStatus SLADecodeFFMPEG::StartSaving(const char *filename)
{
  FFCameraData *cam = (FFCameraData*)Data;

  if(cam->dumpFile)
    return SLA_ERROR;

  char fname[1024];

  // Convert slash to Windows backslash
  strncpy(fname, filename, sizeof(fname));
  char *c = fname;
  while(*c) {
    if(*c == '/') *c = '\\';
    c++;
  }

  if(cam->inputType == INPUT_NETWORK) {
    if(!cam->udpRx) return SLA_ERROR;
    return SLAStartSavingUDP(cam->udpRx, fname);
  } else {
    FILE *f = fopen(fname, "wb");
    if(f){
      FFMpegControlPacket pkt;

      pkt.type = FF_CONTROL_SAVEFILE;
      pkt.ptr = f;
      SLAMbxPost(cam->hmbx, &pkt, SEM_FOREVER);

      return SLA_SUCCESS;
    }
    return SLA_ERROR;
  }
}

SLStatus SLADecodeFFMPEG::StopSaving()
{
  FFCameraData *cam = (FFCameraData*)Data;

  if(cam->inputType == INPUT_NETWORK) {
    if(!cam->udpRx) return SLA_ERROR;
    return SLAStopSavingUDP(cam->udpRx);
  } else {
    if(cam->dumpFile){
      FFMpegControlPacket pkt;

      pkt.type = FF_CONTROL_SAVEFILE;
      pkt.ptr = 0;
      SLAMbxPost(cam->hmbx, &pkt, SEM_FOREVER);

      return SLA_SUCCESS;
    }
    return SLA_ERROR;
  }
}

SLStatus SLADecodeFFMPEG::Start()
{
  FFCameraData *cam = (FFCameraData*)this->Data;
  cam->started = true;
  return SLA_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
SLStatus SLADecodeFFMPEG::Pause(bool pause)
{
  //rg partially completed  Pause the decoding of frames
  //not sure what is does to the metadata stream
  //to do replace with a packet in the mail box
  FFCameraData *cam = (FFCameraData*)this->Data;
  cam->isPaused = pause;
  SLATrace("Is Paused: %d\n",pause);
  return SLA_SUCCESS;
}


 SLStatus SLADecodeFFMPEG::Seek(double timeStamp)
 {
  FFCameraData *cam = (FFCameraData*)Data;

  if(cam->inputType == INPUT_NETWORK) {
   //do somthing can we save to a tmp file or something aways? 
  }
  else if (cam->inputType == INPUT_FILE){
    FFMpegControlPacket pkt;

    pkt.type = FF_CONTROL_SEEK;
    memcpy(pkt.name, &timeStamp, sizeof(double)); //dodgy but unsure how else to pass arguments rg
    SLAMbxPost(cam->hmbx, &pkt, SEM_FOREVER);

    return SLA_SUCCESS;
  }  
  return SLA_ERROR;
 }

double SLADecodeFFMPEG::GetDuration()
 {
  FFCameraData *cam = (FFCameraData*)Data;

  if(cam->inputType != INPUT_FILE)
  {
    return 0;
  }

  return (double)cam->pFormatCtx->duration / (double)AV_TIME_BASE;
 }

void SLADecodeFFMPEG::SetUpSample(int upsample)
{
  FFCameraData *cam = (FFCameraData*)Data;
  if(cam) {
    cam->upSample = SLLIMIT(upsample, 1, 4);
  }
}

int SLADecodeFFMPEG::GetUpSample()
{
  FFCameraData *cam = (FFCameraData*)Data;
  if(cam) {
    return cam->upSample;
  }
  return 1;
}

