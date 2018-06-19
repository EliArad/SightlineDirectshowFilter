/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/

#define _CRT_SECURE_NO_WARNINGS
#include <string>
#include "SLAUDPReceive.h"
#include "SLAHal.h"
#include <stdio.h>

#define USE_CUSTDATA 1   // 1=add support for decoding SLA private diagnostic data.

// Enable lots of debug for diagnosing TS parsing problems
#define PACKET_DEBUG 0
// Get a UDP packet
//
#define UDP_PACKET_LEN 1500
#define QSIZE 4

typedef struct {
  u32 len;
//  u32 timestamp;
  u8 *data;
} SL_UDP_PACKET; 


#if USE_CUSTDATA
////////////////////////////////////////////////////////////////////////////////
typedef SLStatus (ReadMtsPktData)(struct UDPReceiveStruct *data, SL_UDP_PACKET *pkt, void *ctxt);



static FILE *MtsOpen(const char *fname)
{
  FILE *fp = fopen(fname, "rb");
  //SLAssert(fp != 0);
  return fp;
}

static SLStatus MtsReadFile(struct UDPReceiveStruct *data, SL_UDP_PACKET *pkt, void *ctxt)
{
  SLStatus st = SLA_SUCCESS;
  FILE *fp = (FILE*)ctxt;

  if (!fp){
    SLASleep(30);
    return SLA_FAIL;
  }
  pkt->len = 188;
  u32 len = fread(pkt->data, 188, 1, fp); // 188 is the MTS packet size.
  if (len != 1) {
    s32 err = ferror(fp);
    if (err != 0) {
      st = SLA_FAIL;
      //SLTrace("%s %d: err=%d\n", __FUNCTION__, __LINE__, err);
    }
    else {
      st = SLA_TERMINATE;
    }
    fclose(fp);
    fp = 0;
  }
  return st;
}

////////////////////////////////////////////////////////////////////////////////
#endif // #if USE_CUSTDATA

// for a single elementary stream
typedef struct {
  u8 *buf;
  int bufLen;
  int PID;
  u64 pts;
  int bufferPos;
  u16 pesDataLen;
  int started;
  int missedPacket;
  int haveFrame;
  int CC;
  int frameDataComplete;
  SLAUdpVideoProtocol streamType;
} PESStruct;

typedef enum {
  RTPFAIL_NONE = 0,           //!< No failure (default)
  RTPFAIL_SEQUENCE_MISMATCH,  //!< usually cause by missed packets.
  RTPFAIL_BUFFER_OVERFLOW,    //!< Expected image data length potentially exceeds receive memory buffer allocated
  RTPFAIL_TYPE_MISMATCH       //!< Unknown or unexpected image type
} RTPFailedType;

typedef struct UDPReceiveStruct {
  void *emptyMbx, *fullMbx;
//  u8 *packets;
  char hostname[1024];
  int port;
  SLASocket RcvSocket;

  SLA_COMPRESSED_FRAME _frame[QSIZE];

  void *lockSem;
  void *doneSem;
  void *dumpSem;
  u32 done;

  SL_UDP_PACKET pkt;
  bool isRTPts;
  int bytesProcessed;
  s32 busy;

  FILE *dumpFile;

  // For TS
  s32 PMT_PID, PES_index;
  s32 ProgramNumber;

#if USE_CUSTDATA
  PESStruct PES[3];  // TS: 0 video, 1 klv, 2 SLA priv diag metadata.
  ReadMtsPktData  *readPkt;
  void            *readPktCtx; 
  bool            useSlDemux;   // Use SLUDPReceive demux instead of FFMPEG internal version. Decoding SLALIB diag data works better with SLUDPReceive. 
#else
  PESStruct PES[2];  // TS: 0 video, 1 klv
#endif
  //PESStruct *currentPES;

  // For RTP
  u16 prevseq;
  u32 quality, wide, high;
  s32 type;
  s32 dataLen;
  u8 lumaq[64], chromaq[64];
  s32 failed;     //!< Count of failures from received packets while building frame
  s32 maxFailCount; //!< log fail count over lifespan
  RTPFailedType lastFailureType;  //!< keep track of the latest type of failure (for reporting)
  s32 status;

} UDPReceiveStruct;

static void initSocket(UDPReceiveStruct *data)
{
  if (data->useSlDemux) {
    data->readPktCtx = MtsOpen(data->hostname);
    data->readPkt = MtsReadFile;
  }
  else {
    // Set up networking
    data->RcvSocket.addr = 0;
    data->RcvSocket.port = data->port;

    u32 tmp = inet_addr(data->hostname);
    s32 multicast = 0;
    if((tmp & 0xFF)>=224 && (tmp & 0xFF)<=239)
      multicast = 1;

    SLASockServerBind(&data->RcvSocket, SOCK_DGRAM, IPPROTO_UDP, 0);
    if(multicast) {
      if (SLASockJoinSourceGroup(&data->RcvSocket, inet_addr(data->hostname), INADDR_ANY, INADDR_ANY) != 0)
      {
        SLATrace("Error joining multicast group.\n");
        SLASockClose(&data->RcvSocket);
        SLASockCleanup();
      }
    }
    // Receive timeout of 10 ms
    //SLSockSetTimeoutMs(&data->RcvSocket, SO_RCVTIMEO, 30);
  }
}

static int udpReceiveTask(void *_data);

void *SLAInitUDPReceive(u32 nPackets, char *hostname, int port, bool useSlDemux)
{
  int i;
  UDPReceiveStruct *data = (UDPReceiveStruct *)SLACalloc(sizeof(UDPReceiveStruct));
  SLASockStartup();
//  SLDebugAssert(sizeof(data->hostname) > strlen(hostname));
  strcpy(data->hostname, hostname);
  data->port = port;
  data->useSlDemux = useSlDemux;

  for(i=0;i<QSIZE;i++){
    data->_frame[i].buffer = (u8*)SLACalloc(MAX_COMPRESSED_BUFFER_SIZE);
    data->_frame[i].maxBufferLen = MAX_COMPRESSED_BUFFER_SIZE;
  }
  data->pkt.data = (u8*)SLACalloc(UDP_PACKET_LEN);

  data->emptyMbx = SLAMbxCreate(sizeof(SLA_COMPRESSED_FRAME), QSIZE, "udpEmpty");
  data->fullMbx = SLAMbxCreate(sizeof(SLA_COMPRESSED_FRAME), QSIZE, "udpFull");
  data->PES[0].buf = (u8*)SLACalloc(MAX_COMPRESSED_BUFFER_SIZE);
  data->PES[0].bufLen = MAX_COMPRESSED_BUFFER_SIZE;
  data->PES[1].buf = (u8*)SLACalloc(MAX_AUXILIARY_BUFFER_SIZE);
  data->PES[1].bufLen = MAX_AUXILIARY_BUFFER_SIZE;
  data->PES[0].CC = data->PES[1].CC = -1;
  data->PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_H264;
  data->PES[1].streamType = SLA_UDP_VIDEO_PROTOCOL_KLV_METADATA;
#if USE_CUSTDATA
  data->PES[2].buf = (u8*)SLACalloc(MAX_AUXILIARY_BUFFER_SIZE);
  data->PES[2].bufLen = MAX_AUXILIARY_BUFFER_SIZE;
  data->PES[2].CC = -1;
  data->PES[2].streamType = SLA_UDP_VIDEO_PROTOCOL_SLA_METADATA;
#endif

  // Fill empty buffer with all the packet buffers
  for(i=0;i<QSIZE;i++){
    SLAMbxPost(data->emptyMbx, &data->_frame[i], SL_FOREVER);
  }

  data->doneSem = SLASemCreate(0);
  data->lockSem = SLASemCreate(1);
  data->dumpSem = SLASemCreate(0);
  data->dumpFile = 0;

  SLACreateThread(udpReceiveTask, 0, "udpReceiveTask", data, SL_PRI_15);

  return (void*)data;
}

void SLAReinitUDPReceive(void *_data, char *hostname, int port)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)_data;

  // Wait for task to initialize -- don't Reinit until we've inited once!
  SLASemPend(data->dumpSem, -1);
  if (data->useSlDemux) {
    SLATrace("WRN: ReinitUDPReceive ignored because reading from file\n");
  }
  else {
    strcpy(data->hostname, hostname);
    data->port = port;

    SLASockDisconnect(&data->RcvSocket);

    initSocket(data);
  }
  SLASemPost(data->dumpSem);
}

SLStatus SLAStartSavingUDP(void *_data, char *fname)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)_data;

  SLASemPend(data->dumpSem, SL_FOREVER);
  if(data->dumpFile)
    fclose(data->dumpFile);
  data->dumpFile = fopen(fname, "wb");
  SLASemPost(data->dumpSem);

  return data->dumpFile?SLA_SUCCESS:SLA_FAIL;
}

SLStatus SLAStopSavingUDP(void *_data)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)_data;

  SLASemPend(data->dumpSem, SL_FOREVER);
  if(data->dumpFile){
    fclose(data->dumpFile);
    data->dumpFile = 0;
  }
  SLASemPost(data->dumpSem);
  return SLA_SUCCESS;
}

void SLADestroyUDPReceive(void *_data)
{
  s32 i;
  UDPReceiveStruct *data = (UDPReceiveStruct *)_data;

  // We're all done
  data->done = 1;

  // Wait for task to finish
  SLASemPend(data->doneSem, SL_FOREVER);

  // Delete all allocated objects
  SLASockDisconnect(&data->RcvSocket);

  SLAMbxDestroy(data->emptyMbx);
  SLAMbxDestroy(data->fullMbx);
  SLASemDestroy(data->doneSem);
  SLASemDestroy(data->lockSem);
  SLASemDestroy(data->dumpSem);
  SLAFree(data->PES[0].buf);
  SLAFree(data->PES[1].buf);
#if USE_CUSTDATA
  SLAFree(data->PES[2].buf);
#endif
  for(i=0;i<QSIZE;i++){
    SLAFree(data->_frame[i].buffer);
  }
  SLAFree(data->pkt.data);
  SLAFree(_data);
}



static int setFrame(SLA_COMPRESSED_FRAME *frame, PESStruct *pes, UDPReceiveStruct *tsData)
{
  if (!pes->haveFrame || pes->missedPacket || pes->bufferPos>pes->bufLen) {
    pes->haveFrame = 0;
    return 0;
  }

#if PACKET_DEBUG
  SLATrace("  copy %d bytes min(%d,%d)\n", SLMIN((s32)frame->maxBufferLen, pes->bufferPos), frame->maxBufferLen, pes->bufferPos);
  SLATrace("  pesDataLen was %d\n", pes->pesDataLen);
#endif
  SLAMemcpy(frame->buffer, pes->buf, SLMIN((s32)frame->maxBufferLen, pes->bufferPos));
  frame->len = pes->bufferPos;
  frame->PID = pes->PID;
  frame->PTS = pes->pts;
  frame->missedPacket = pes->missedPacket;
  frame->high = 0;
  frame->wide = 0;
  frame->frameDataComplete = pes->frameDataComplete;
  frame->streamType = pes->streamType;
  pes->haveFrame = 0;

  // TODO: could look at frame NAL header and identify 
  // frames to discard see http://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=0CCoQFjAA&url=http%3A%2F%2Fdsmc2.eap.gr%2Ffiles%2Fcpubs%2FKapotas_icpr10.pdf&ei=XTcwU-fUI4LooASmwIGIBQ&usg=AFQjCNHkxZ2zbDVHP__Xf7L817u9w5woBA&sig2=zquyznxNG_5YKrKZCAliNA&bvm=bv.62922401,d.cGU
  // Could eliminate frames with nal_ref_idc (bits 5 and 6 in first byte past 000001 marker) set to 0
  // These frames should be marked by encoder but currently are not.

  return 1;
}

// Transport Stream packet in table 2-2
typedef struct {
  u8 sync_byte;
  u8 transport_error_indicator;
  u8 payload_unit_start_indicator;
  u8 transport_priority;
  u16 PID;
  u8 transport_scrambling_control;
  u8 adaptation_field_control;
  u8 continuity_counter;

  // Adaptation field in table 2-6
  u8 adaptation_field_length;
  u8 discontinuity_indicator;
  u8 random_access_indicator;
  u8 elementary_stream_priority_indicator;
  u8 PCR_flag;
  u8 OPCR_flag;
  u8 splicing_point_flag;
  u8 transport_private_data_flag;
  u8 adaptation_field_extension_flag;
  u64 program_clock_reference_base;
  u16 program_clock_reference_extension;
  u64 original_program_clock_reference_base;
  u16 original_program_clock_reference_extension;
  u8 splice_countdown;
  u8 transport_private_data_length;
  // Note: always ignore transport private data
  u8 adapation_field_extension_length;
  // Note: always ignore adaptation field extension data

  // Program clock reference, unified from base and extension data
  s64 Pcr;
  s64 Opcr;

  // Number of stuffing bytes in adaptation field
  u8 Stuffing;

  // Offset in packet of start of payload data
  u8 DataOffset;
} TsPacket;

#define MAX_PAT_ENTRIES 10
#define MAX_ELEMENTARY_STREAMS 10
typedef struct {
  u16 program_number;
  u16 program_map_PID;
} PATEntry;

// Program Association Section in table 2-25
typedef struct {
  u8 table_id;
  u8 section_syntax_indicator;
  u16 section_length;
  u16 transport_stream_id;
  u8 version_number;
  u8 current_next_indicator;
  u8 section_number;
  u8 last_section_number;
  PATEntry patEntry[MAX_PAT_ENTRIES];
  u32 CRC;

  u8 N;  // Actual number of PAT entries
} ProgramAssociation;

typedef struct {
  u8 stream_type;
  u16 elementary_PID;
  u16 ES_info_length;
} ESInfo;

typedef struct {
  u8 table_id;
  u8 section_syntax_indicator;
  u16 section_length;
  u16 program_number;
  u8 version_number;
  u8 current_next_indicator;
  u8 section_number;
  u8 last_section_number;
  u16 PCR_PID;
  u16 program_info_length;
  ESInfo esInfo[MAX_ELEMENTARY_STREAMS];
  u32 CRC;

  u8 N;
} ProgramMap;

typedef struct {
  u8 packet_start_code_prefix[3];
  u8 stream_id;
  u16 PES_packet_length;
  u8 PES_scrambling_control;
  u8 PES_priority;
  u8 data_alignment_indicator;
  u8 copyright;
  u8 original_or_copy;
  u8 PTS_DTS_flags;
  u8 ESCR_flag;
  u8 ES_rate_flag;
  u8 DSM_trick_mode_flag;
  u8 additional_copy_info_flag;
  u8 PES_CRC_flag;
  u8 PES_extension_flag;
  u8 PES_header_data_length;
  u64 PTS;
  u64 DTS;
  u8 PES_private_data_flag;
  u8 pack_header_field_flag;
  u8 program_packet_sequence_counter_flag;
  u8 PSTD_buffer_flag;
  u8 PES_extension_flag2;
  u8 PES_private_data[16];

} PESHeader;

// extract specified bits.  0->lsb
//#define EB(val, high, low) ((val>>low) & ((1<<((high-low)+1))-1))
static u8 EB(u8 val, u8 high, u8 low)
{
  return ((val>>low) & ((1<<((high-low)+1))-1));
}

static s32 parseTsPacket(TsPacket *p, const u8 *buffer, s32 length)
{
  SLAMemset(p, 0, sizeof(*p));

  if(length<4)
    return -1;

  p->sync_byte = buffer[0];
  p->transport_error_indicator = EB(buffer[1], 7, 7);
  if(p->transport_error_indicator)
    return -4;
  p->payload_unit_start_indicator = EB(buffer[1], 6, 6);
  p->transport_priority = EB(buffer[1], 5, 5);
  p->PID = (EB(buffer[1], 4, 0)<<8) | buffer[2];
  p->transport_scrambling_control = EB(buffer[3], 7, 6);
  p->adaptation_field_control = EB(buffer[3], 5, 4);
  p->continuity_counter = EB(buffer[3], 3, 0);

  p->DataOffset = 4;
  // Look at adaptation field if present
  if(p->adaptation_field_control & 0x2){
    if(length<5)
      return -2;
    p->adaptation_field_length = buffer[4];
    if(length<5+p->adaptation_field_length)
      return -3;
    p->DataOffset += 1+p->adaptation_field_length;
    p->Stuffing = p->adaptation_field_length;
    if(p->adaptation_field_length>0){
      const u8 *b;
      p->discontinuity_indicator = EB(buffer[5], 7, 7);
      p->random_access_indicator = EB(buffer[5], 6, 6);
      p->elementary_stream_priority_indicator = EB(buffer[5], 5, 5);
      p->PCR_flag = EB(buffer[5], 4, 4);
      p->OPCR_flag = EB(buffer[5], 3, 3);
      p->splicing_point_flag = EB(buffer[5], 2, 2);
      p->transport_private_data_flag = EB(buffer[5], 1, 1);
      p->adaptation_field_extension_flag = EB(buffer[5], 0, 0);

      p->Stuffing--;
      b = buffer+6;
      if(p->PCR_flag){
        p->Pcr = ( (u64)b[0] << 34 ) |
                  ( (u64)b[1] << 26 ) |
                  ( (u64)b[2] << 18 ) |
                  ( (u64)b[3] << 10 ) |
                  ( ((u64)EB(b[4],7,7)) << 9 ) |
                  ( ((u64)EB(b[4],0,0)) << 8 ) |
                  ( (u64)b[5] );
        p->Stuffing -= 6;
        b += 6;
      }
      if(p->OPCR_flag){
        p->Opcr = ( (u64)b[0] << 34 ) |
                  ( (u64)b[1] << 26 ) |
                  ( (u64)b[2] << 18 ) |
                  ( (u64)b[3] << 10 ) |
                  ( ((u64)EB(b[4],7,7)) << 9 ) |
                  ( ((u64)EB(b[4],0,0)) << 8 ) |
                  ( (u64)b[5] );
        p->Stuffing -= 6;
        b += 6;
      }
      if(p->splicing_point_flag){
        p->splice_countdown = b[0];
        p->Stuffing--;
        b++;
      }
      if(p->transport_private_data_flag){
        p->transport_private_data_length = b[0];
        p->Stuffing -= 1+p->transport_private_data_length;
        b += 1+p->transport_private_data_length;
      }
      if(p->adaptation_field_extension_flag){
        p->adapation_field_extension_length = b[0];
        p->Stuffing -= 1+p->adapation_field_extension_length;
        b += 1+p->adapation_field_extension_length;
      }
    }
  }
  return 0;
}

static s32 parsePAT(ProgramAssociation *pat, u8 *buffer, s32 length)
{
  if(length<8)
    return -1;
  pat->table_id = buffer[0];
  pat->section_syntax_indicator = EB(buffer[1], 7, 7);
  // Check syntax indicator
  if(pat->section_syntax_indicator != 1)
    return -2;
  // Check '0' bits
  if(EB(buffer[1], 6, 6) != 0)
    return -3;
  // Check reserved bits (defined to be 1)
  if(EB(buffer[1], 5, 4) != 0x03)
    return -5;
  pat->section_length = (EB(buffer[1], 3, 0)<<8) | buffer[2];
  if(pat->section_length > 1021)
    return -6;
  if(length<pat->section_length+3)
    return -7;
  pat->transport_stream_id = (buffer[3]<<8) | buffer[4];
  pat->version_number = EB(buffer[5], 5, 1);
  pat->current_next_indicator = EB(buffer[5], 0, 0);
  pat->section_number = buffer[6];
  pat->last_section_number = buffer[7];

  pat->N = 0;
  u8 *b = buffer+8;
  u16 i = 0;
  while(i<pat->section_length-9) {
    pat->patEntry[pat->N].program_number = (b[i]<<8) | b[i+1];
    pat->patEntry[pat->N].program_map_PID = (EB(b[i+2],4,0)<<8) | b[i+3];
    i += 4;
    pat->N++;
  }
  pat->CRC = (b[i]<<24) | (b[i+1]<<16) | (b[i+2]<<8) | b[i+3];
  return 0;
}

static s32 parsePM(ProgramMap *_pmt, PESStruct *PES, u8 *buffer, s32 length, s32 fromRTP)
{
  ProgramMap localPmt;
  ProgramMap *pmt = &localPmt;
  if(length<8)
    return -1;
  pmt->table_id = buffer[0];
  pmt->section_syntax_indicator = EB(buffer[1], 7, 7);
  // Check syntax indicator
  if(pmt->section_syntax_indicator != 1)
    return -2;
  // Check '0' bits
  if(EB(buffer[1], 6, 6) != 0)
    return -3;
  // Check reserved bits (defined to be 1)
  if(EB(buffer[1], 5, 4) != 0x03)
    return -5;
  pmt->section_length = (EB(buffer[1], 3, 0)<<8) | buffer[2];
  if(pmt->section_length > 1021)
    return -6;
  if(length<pmt->section_length+3)
    return -7;
  pmt->program_number = (buffer[3]<<8) | buffer[4];
  pmt->version_number = EB(buffer[5], 5, 1);
  pmt->current_next_indicator = EB(buffer[5], 0, 0);
  pmt->section_number = buffer[6];
  pmt->last_section_number = buffer[7];
  if(EB(buffer[8], 7, 5) != 0x07)
    return -8;
  pmt->PCR_PID = (EB(buffer[8], 4, 0)<<8) | buffer[9];
  if(EB(buffer[10], 7, 4) != 0x0f)
    return -9;
  pmt->program_info_length = (EB(buffer[10], 3, 0)<<8) | buffer[11];

  // ignoring descriptors
  pmt->N = 0;
  u8 *b = buffer+12+pmt->program_info_length;
  u16 i = 0;
  while(i<pmt->section_length-(13+pmt->program_info_length)) {
    pmt->esInfo[pmt->N].stream_type = b[i];
    pmt->esInfo[pmt->N].elementary_PID = (EB(b[i+1],4,0)<<8) | b[i+2];
    pmt->esInfo[pmt->N].ES_info_length = (EB(b[i+3],3,0)<<8) | b[i+4];
    // ignoring per-elementary-stream descriptors
    i += 5+pmt->esInfo[pmt->N].ES_info_length;
    pmt->N++;
  }
  pmt->CRC = (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | b[3];

  for(s32 ii=0;ii<pmt->N;ii++){
    switch(pmt->esInfo[ii].stream_type){
      case 0x02:  // mpeg2 video stream
        PES[0].PID = pmt->esInfo[ii].elementary_PID;
        PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_MPEG2;
        break;
      case 0x10:  // mpeg4 video stream
        PES[0].PID = pmt->esInfo[ii].elementary_PID;
        if (fromRTP)
          PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_RTPMP2MPEG4;
        else
          PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_MPEG4;
        break;
      case 0x1b:  // h.264 video stream
        PES[0].PID = pmt->esInfo[ii].elementary_PID;
        if (fromRTP)
          PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_RTPMP2H264;
        else
          PES[0].streamType = SLA_UDP_VIDEO_PROTOCOL_H264;
        break;
      case 0x06:   // Original: ITU-T Rec. H.222.0 | ISO/IEC 13818-1 PES packets containing private data
      case 0x15:   // Metadata carried in PES packets
        PES[1].PID = pmt->esInfo[ii].elementary_PID; // Metadata
        PES[1].streamType = SLA_UDP_VIDEO_PROTOCOL_KLV_METADATA;
        break;
      case 0x88:  // SLA private diagnostic data.
        PES[2].PID = pmt->esInfo[ii].elementary_PID; // Metadata
        PES[2].streamType = SLA_UDP_VIDEO_PROTOCOL_SLA_METADATA;
        break;
      default:
        // TODO: error handling?
        break;
    }
  }
  SLAMemcpy(_pmt, pmt, sizeof(*pmt));
  return 0;
}

static int parsePESHeader(PESStruct *pes, PESHeader *h, const u8 *p, s32 length)
{
  // Check for start code
  SLAMemset(h, 0, sizeof(*h));
  pes->frameDataComplete = 0;

  pes->pesDataLen = 0;
  h->packet_start_code_prefix[0] = p[0];
  h->packet_start_code_prefix[1] = p[1];
  h->packet_start_code_prefix[2] = p[2];
  if( h->packet_start_code_prefix[0]!=0 || 
      h->packet_start_code_prefix[1]!=0 || 
      h->packet_start_code_prefix[2]!=1)
    return 0;

  h->stream_id = p[3];
  h->PES_packet_length = (p[4]<<8) + p[5];
  pes->pesDataLen = h->PES_packet_length;
  h->PES_scrambling_control = EB(p[6], 5, 4);
  h->PES_priority = EB(p[6], 3, 3);
  h->data_alignment_indicator = EB(p[6], 2, 2);
  h->copyright = EB(p[6], 1, 1);
  h->original_or_copy = EB(p[6], 0, 0);
  h->PTS_DTS_flags = EB(p[7], 7, 6);
  h->ESCR_flag = EB(p[7], 5, 5);
  h->ES_rate_flag = EB(p[7], 4, 4);
  h->DSM_trick_mode_flag = EB(p[7], 3, 3);
  h->additional_copy_info_flag = EB(p[7], 2, 2);
  h->PES_CRC_flag = EB(p[7], 1, 1);
  h->PES_extension_flag = EB(p[7], 0, 0);
  h->PES_header_data_length = p[8];

  const u8 *b = p+9;

  if(h->PTS_DTS_flags & 2){
    h->PTS = (((u64)EB(b[0], 3, 1) << 30) |
      ((u64)EB(b[1], 7, 0) << 22) |
      ((u64)EB(b[2], 7, 1) << 15) |
      ((u64)EB(b[3], 7, 0) << 7) |
      ((u64)EB(b[4], 7, 1)));
    pes->pts = h->PTS;
    b += 5;
  }
  if(h->PTS_DTS_flags == 3){
    h->DTS = (((u64)EB(b[0], 3, 1) << 30) |
      ((u64)EB(b[1], 7, 0) << 22) |
      ((u64)EB(b[2], 7, 1) << 15) |
      ((u64)EB(b[3], 7, 0) << 7) |
      ((u64)EB(b[4], 7, 1)));
    b += 5;
  }
  if(h->ESCR_flag){
    b += 6;
  }
  if(h->ES_rate_flag){
    b += 3;
  }
  if(h->DSM_trick_mode_flag){
    b += 1;
  }
  if(h->additional_copy_info_flag){
    b += 1;
  }
  if(h->PES_CRC_flag){
    b += 2;
  }
  if(h->PES_extension_flag){
    h->PES_private_data_flag = EB(b[0], 7, 7);
    if(h->PES_private_data_flag){
      SLAMemcpy(h->PES_private_data, &b[1], sizeof(h->PES_private_data));
      // Presence of a "1" with the correct alternating pattern
      // indicates that this is the last PES packet at a given PTS
      // Pattern wxyzyzyzyz...yz
      // w=1, x=~1, all y's are don't care, all z's are ~y
      s32 valid = b[1]==1;
      for(int i=0;i<sizeof(h->PES_private_data);i+=2){
        valid = valid && (b[i+1] == (u8)(~b[i+2]));
      }
      if(valid)
        pes->frameDataComplete = b[1];
    }
  }

  // Value output is actual length of PES data w/o PES packet header
  // (or 0 to signify "unknown" if PES_packet_length field is 0)
  if(pes->pesDataLen)
    pes->pesDataLen -= h->PES_header_data_length+3;
  return 9+h->PES_header_data_length;
}

const char indent[6][12] = { "", "  ", "    ", "      ", "        ", "          " };

static void SLTrace(const char *fmt, ...)
{
#ifndef _WIN32
  va_list Argp;
  va_start(Argp, fmt);
  vprintf(fmt, Argp);
  va_end(Argp);
#else
  char str[1024];
  va_list Argp;
  va_start(Argp, fmt);
  vsprintf(str, fmt, Argp);
  va_end(Argp);
  //OutputDebugString(str);
  
  std::string my_string(str);
  //Console::WriteLine(my_string);

#endif
}


#define cdump(x) if(p->x) SLTrace("%s" #x "=%d\n", indent[level], p->x)
#define dump(x) SLTrace("%s" #x "=%d\n", indent[level], p->x)
#define dumpu64(x) SLTrace("%s" #x "=%I64x\n", indent[level], p->x)
#define dumps64(x) SLTrace("%s" #x "=%I64x\n", indent[level], p->x)

void trace(TsPacket *p, int level)
{
  SLTrace("%sTsPacket: PID = %d/%x  (CC=%d)\n", indent[level], p->PID, p->PID, p->continuity_counter);
  level++;
  if (p->sync_byte != 0x47)
    dump(sync_byte);
  cdump(transport_error_indicator);
  cdump(payload_unit_start_indicator);
  cdump(transport_priority);
  //dump(PID);
  cdump(transport_scrambling_control);
  if(p->adaptation_field_control != 1)
    dump(adaptation_field_control);
  //dump(continuity_counter);
  if (p->adaptation_field_control & 1){
    cdump(adaptation_field_length);
    cdump(discontinuity_indicator);
    cdump(random_access_indicator);
    cdump(elementary_stream_priority_indicator);
    cdump(PCR_flag);
    cdump(OPCR_flag);
    cdump(splicing_point_flag);
    cdump(transport_private_data_flag);
    cdump(adaptation_field_extension_flag);
    if (p->PCR_flag){
      dumpu64(program_clock_reference_base);
      dump(program_clock_reference_extension);
      dumps64(Pcr);
    }
    if (p->OPCR_flag){
      dumpu64(original_program_clock_reference_base);
      dump(original_program_clock_reference_extension);
      dumps64(Opcr);
    }
    if (p->splicing_point_flag)
      dump(splice_countdown);
    if (p->transport_private_data_flag){
      dump(transport_private_data_length);
    }
    if (p->adaptation_field_extension_flag) {
      dump(adapation_field_extension_length);
    }
  }
  cdump(Stuffing);
  if (p->DataOffset!=4)
    dump(DataOffset);
}

void trace(PATEntry *p, int level)
{
  dump(program_number);
  dump(program_map_PID);
}

void trace(ProgramAssociation *p, int level)
{
  SLTrace("%sProgramAssociation\n", indent[level]);
  level++;
  dump(table_id);
  dump(section_syntax_indicator);
  dump(section_length);
  dump(transport_stream_id);
  dump(version_number);
  dump(current_next_indicator);
  dump(section_number);
  dump(last_section_number);
  dump(N);
  for (int i = 0; i < p->N; i++)
    trace(&p->patEntry[i], level + 1);
  dump(CRC);
}

void trace(ESInfo *p, int level)
{
  dump(stream_type);
  dump(elementary_PID);
  dump(ES_info_length);
}

void trace(ProgramMap *p, int level)
{
  SLTrace("%sProgramMap\n", indent[level]);
  level++;
  dump(table_id);
  dump(section_syntax_indicator);
  dump(section_length);
  dump(program_number);
  dump(version_number);
  dump(current_next_indicator);
  dump(section_number);
  dump(last_section_number);
  dump(PCR_PID);
  dump(program_info_length);
  dump(N);
  for (int i = 0; i < p->N; i++)
    trace(&p->esInfo[i], level + 1);
  dump(CRC);
}


void trace(PESHeader *p, int level)
{
  SLTrace("%sPESHeader\n", indent[level]);
  level++;

  SLTrace("%spacket_start_code_prefix=%02x%02x%02x\n", indent[level], p->packet_start_code_prefix[0], p->packet_start_code_prefix[1], p->packet_start_code_prefix[2]);
  dump(stream_id);
  dump(PES_packet_length);
  cdump(PES_scrambling_control);
  cdump(PES_priority);
  cdump(data_alignment_indicator);
  cdump(copyright);
  cdump(original_or_copy);
  cdump(PTS_DTS_flags);
  cdump(ESCR_flag);
  cdump(ES_rate_flag);
  cdump(DSM_trick_mode_flag);
  cdump(additional_copy_info_flag);
  cdump(PES_CRC_flag);
  cdump(PES_extension_flag);
  cdump(PES_header_data_length);
  if (p->PTS_DTS_flags>=2)
    dumpu64(PTS);
  if (p->PTS_DTS_flags == 3)
    dumpu64(DTS);
  if (p->PES_extension_flag) {
    cdump(PES_private_data_flag);
    cdump(pack_header_field_flag);
    cdump(program_packet_sequence_counter_flag);
    cdump(PSTD_buffer_flag);
    cdump(PES_extension_flag2);
    if (p->PES_private_data_flag){
      SLTrace("%sPES_private_data=", indent[level]);
      for (int i = 0; i < 16; i++){
        SLTrace("%02x", p->PES_private_data[i]);
      }
      SLTrace("\n");
    }
  }
}





static int demuxTSPacket(UDPReceiveStruct *tsData, SL_UDP_PACKET *packet, SLA_COMPRESSED_FRAME *frame, int *bytesRead, int fromRTP)
{
  s32 rv;

  u32 i=*bytesRead;
  PESStruct *currentPES = 0;

  TsPacket tp;
  ProgramAssociation pa;
  ProgramMap pm;
  PESHeader ph;

  while(i<packet->len){
    rv = parseTsPacket(&tp, packet->data+i, packet->len-i);
    // Check for bad packet
    if(rv<0){
      //SLTrace("*** bad packet (rv = %d)***\n", rv);
      i += 188;
      continue;
    }
    //trace(&tp, 0);

    u8 PSI_pointer_field = packet->data[i+tp.DataOffset];
    u8 psiOffset = tp.DataOffset+PSI_pointer_field+1;

    // ignore NULL packets
    if(tp.PID == NULL_PACKET) {
      //SLTrace("PID %d (NULL packet)\n", tp.PID);
      i += TSPacketSize; //188
      continue;
    }

    // Look for Program Association Table
    if(tp.PID == 0){
      rv = parsePAT(&pa, packet->data+i+psiOffset, packet->len-i-psiOffset);

      // Save off the PID of the Program Map Table
      // TODO: PAT could be more complex than just a single
      // PAT entry repeated over and over...
      if(pa.N > 2){
        // TODO: Not sure what do do with this!
        //SLTrace("Error: multi-packet Progarm Association Table\n");
      }
      //trace(&pa, 1);

      for(int ii=0;ii<pa.N;ii++){
        // program number==0 implies network PID.  Ignore this case
        if(pa.patEntry[ii].program_number != 0) {
          tsData->ProgramNumber = pa.patEntry[ii].program_number;
          tsData->PMT_PID = pa.patEntry[ii].program_map_PID;
        }
      }
      i += 188;
      continue;
    }

    if(tp.PID==tsData->PMT_PID){
      rv = parsePM(&pm, tsData->PES, packet->data+i+psiOffset, packet->len-i-psiOffset, fromRTP);
      //trace(&pm, 1);
      i += 188;
      continue;
    }

    if(tp.PID!=0 && tp.PID == tsData->PES[0].PID)
      currentPES = &tsData->PES[0];
    else if(tp.PID!=0 && tp.PID == tsData->PES[1].PID)
      currentPES = &tsData->PES[1];
    else if(tp.PID!=0 && tp.PID == tsData->PES[2].PID)
      currentPES = &tsData->PES[2];
    else {
      // Don't know what to do with this TS packet
#if PACKET_DEBUG
      SLATrace("Skipping unknown TS packet\n");
#endif
      i+=188;
      continue;
    }

    // When a TS packet arrives with a start_indicator, send out the previously
    // stored PES packet, if any. Since this test can't run until the next packet
    // arrives, it adds up to 33ms latency to the frame decoding.
    // Note that this test is only used to break into frame
    // boundaries when stuffing test and PES length test (below) don't work at the end
    // of the previous PES packet.
    if(currentPES && currentPES->started && tp.payload_unit_start_indicator) {
      //s32 index = 0;
      if(currentPES->haveFrame){
        // Don't consume bytes in this situation.  Calling function takes frame and
        // recalls this function.
        *bytesRead = i;
        return setFrame(frame, currentPES, tsData);
      }
    }

    if((tp.adaptation_field_control & 0x01) && currentPES){
      if(currentPES->CC != -1 && ((currentPES->CC+1)&0xF)!=tp.continuity_counter){
        // TODO: handle out of order packets
        currentPES->missedPacket++;

        currentPES->bufferPos = 0;
        currentPES->started = 0;
        i+=188;
        continue;
      }
      currentPES->CC = tp.continuity_counter;

      int k = 0;
      if(tp.payload_unit_start_indicator) {
        currentPES->bufferPos = 0;
        currentPES->started = 1;
        currentPES->missedPacket = 0;
        k = parsePESHeader(currentPES, &ph, &packet->data[i+tp.DataOffset], packet->len-i-tp.DataOffset);
        //trace(&ph, 1);
        // Skip the 5-byte header associated with synchronous metadata
        if(ph.stream_id == 0xFC && currentPES->streamType == SLA_UDP_VIDEO_PROTOCOL_KLV_METADATA)
          k+=5;
      }
      currentPES->haveFrame = 1;

      s32 payloadLen = 188-tp.DataOffset;

      if(currentPES->started && (currentPES->PID == tsData->PES[0].PID || k<payloadLen)){
        if(payloadLen-k>0){
          if (currentPES->bufferPos + payloadLen - k <= currentPES->bufLen)
            SLAMemcpy(currentPES->buf + currentPES->bufferPos, &packet->data[i + tp.DataOffset + k], payloadLen - k);
          currentPES->bufferPos += payloadLen-k;
        } else {
          // Strange error condition
          currentPES->bufferPos = 0;
          currentPES->started = 0;
          i+=188;
          continue;
        }
      } else {
//        SLTrace("NAL start not found\n");
      }



      // If the frame is complete, process the data
      if(tp.Stuffing || (currentPES->bufferPos==currentPES->pesDataLen && currentPES->pesDataLen!=0)) { 
        // pesDataLen could be undefined (0), and there isn't a "last packet in frame" marker that
        // I can find, so only way to know if this is the final ts packet in
        // a frame is to notice if there are stuffing bytes in the packet. This
        // test will fail whenever frame_length%payload_length is 0, so PID and
        // PTS of the next packet need to be examined to figure out if this packet
        // is the end of a frame.  The stuffing test nearly always works and
        // provides better latency frame delivery so it's used here with the PID
        // and PTS test (above) around to clean up the remaining errors.
        // Using pesDataLen defined by the PES header works too, but video
        // streams may leave this unspecified.  Sightline encoders always
        // fill this field in, so it's ok to use.

        // Encoder may sometimes emit an empty frame, swallow those here
        // by skipping setFrame below
        if(currentPES->bufferPos>0) {
          currentPES->haveFrame = 1;
          *bytesRead = i+188;
          return setFrame(frame, currentPES, tsData);
        }
      }
    }
    i += 188;
  }
  *bytesRead = i;
  return 0;
}



/*
 * Table K.1 from JPEG spec.
 */
static const u8 jpeg_luma_quantizer[64] = {
        16, 11, 10, 16, 24, 40, 51, 61,
        12, 12, 14, 19, 26, 58, 60, 55,
        14, 13, 16, 24, 40, 57, 69, 56,
        14, 17, 22, 29, 51, 87, 80, 62,
        18, 22, 37, 56, 68, 109, 103, 77,
        24, 35, 55, 64, 81, 104, 113, 92,
        49, 64, 78, 87, 103, 121, 120, 101,
        72, 92, 95, 98, 112, 100, 103, 99
};

/*
 * Table K.2 from JPEG spec.
 */
static const u8 jpeg_chroma_quantizer[64] = {
        17, 18, 24, 47, 99, 99, 99, 99,
        18, 21, 26, 66, 99, 99, 99, 99,
        24, 26, 56, 99, 99, 99, 99, 99,
        47, 66, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99,
        99, 99, 99, 99, 99, 99, 99, 99
};

/*
 * Call MakeTables with the Q factor and two u_char[64] return arrays
 */
static void
MakeTables(int q, u8 *lqt, u8 *cqt)
{
  int i;
  int factor = q;

  if (q < 1) factor = 1;
  if (q > 99) factor = 99;
  if (q < 50)
    q = 5000 / factor;
  else
    q = 200 - factor*2;

  for (i=0; i < 64; i++) {
    int lq = (jpeg_luma_quantizer[i] * q + 50) / 100;
    int cq = (jpeg_chroma_quantizer[i] * q + 50) / 100;

    /* Limit the quantizers to 1 <= q <= 255 */
    if (lq < 1) lq = 1;
    else if (lq > 255) lq = 255;
    lqt[i] = lq;

    if (cq < 1) cq = 1;
    else if (cq > 255) cq = 255;
    cqt[i] = cq;
  }
}

static u8 lum_dc_codelens[] = {
        0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
};

static u8 lum_dc_symbols[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static u8 lum_ac_codelens[] = {
        0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d,
};

static u8 lum_ac_symbols[] = {
        0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
        0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
        0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
        0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
        0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
        0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
        0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
        0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
        0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa,
};

static u8 chm_dc_codelens[] = {
        0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};

static u8 chm_dc_symbols[] = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
};

static u8 chm_ac_codelens[] = {
        0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77,
};

static u8 chm_ac_symbols[] = {
        0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
        0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
        0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
        0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
        0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
        0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
        0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
        0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
        0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
        0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
        0xf9, 0xfa,
};

static u16 zzOrder[] = {
  0,  1,  8, 16,  9,  2,  3, 10,
 17, 24, 32, 25, 18, 11,  4,  5,
 12, 19, 26, 33, 40, 48, 41, 34,
 27, 20, 13,  6,  7, 14, 21, 28,
 35, 42, 49, 56, 57, 50, 43, 36,
 29, 22, 15, 23, 30, 37, 44, 51,
 58, 59, 52, 45, 38, 31, 39, 46,
 53, 60, 61, 54, 47, 55, 62, 63,
 63, 63, 63, 63, 63, 63, 63, 63, 
 63, 63, 63, 63, 63, 63, 63, 63
};

static u8 *
MakeHuffmanHeader(u8 *p, u8 *codelens, int ncodes,
                  u8 *symbols, int nsymbols, int tableNo,
                  int tableClass)
{
        *p++ = 0xff;
        *p++ = 0xc4;            /* DHT */
        *p++ = 0;               /* length msb */
        *p++ = 3 + ncodes + nsymbols; /* length lsb */
        *p++ = (tableClass << 4) | tableNo;
        memcpy(p, codelens, ncodes);
        p += ncodes;
        memcpy(p, symbols, nsymbols);
        p += nsymbols;
        return (p);
}

static u8 *
MakeDRIHeader(u8 *p, u_short dri) {
        *p++ = 0xff;
        *p++ = 0xdd;            /* DRI */
        *p++ = 0x0;             /* length msb */
        *p++ = 4;               /* length lsb */
        *p++ = dri >> 8;        /* dri msb */
        *p++ = dri & 0xff;      /* dri lsb */
        return (p);
}

/*
 *  Arguments:
 *    type, width, height: as supplied in RTP/JPEG header
 *    lqt, cqt: quantization tables as either derived from
 *         the Q field using MakeTables() or as specified
 *         in section 4.2.
 *    dri: restart interval in MCUs, or 0 if no restarts.
 *
 *    p: pointer to return area
 *
 *  Return value:
 *    The length of the generated headers.
 *
 *    Generate a frame and scan headers that can be prepended to the
 *    RTP/JPEG data payload to produce a JPEG compressed image in
 *    interchange format (except for possible trailing garbage and
 *    absence of an EOI marker to terminate the scan).
 */
static int MakeHeaders(u8 *p, int type, int w, int h, const u8 *lqt,
                const u8 *cqt, u16 dri)
{
        int i;
        u_char *start = p;

        /* convert from blocks to pixels */
        w <<= 3;
        h <<= 3;
        *p++ = 0xff;
        *p++ = 0xd8;            /* SOI */

        // Quantization table segment
        *p++ = 0xff;
        *p++ = 0xdb;
        s16 len = (2<<6) + 2 + 2;
        *p++ = len>>8;
        *p++ = len & 0xff;
        *p++ = 0;
        for(i=0;i<64;i++)
          *p++ = lqt[zzOrder[i]];
        *p++ = 1;
        for(i=0;i<64;i++)
          *p++ = cqt[zzOrder[i]];

        if (dri != 0)
                p = MakeDRIHeader(p, dri);

        *p++ = 0xff;
        *p++ = 0xc0;            /* SOF */
        *p++ = 0;               /* length msb */
        *p++ = 17;              /* length lsb */
        *p++ = 8;               /* 8-bit precision */
        *p++ = h >> 8;          /* height msb */
        *p++ = h;               /* height lsb */
        *p++ = w >> 8;          /* width msb */
        *p++ = w;               /* wudth lsb */
        *p++ = 3;               /* number of components */
        *p++ = 0;               /* comp 0 */
        if (type == 0)
                *p++ = 0x21;    /* hsamp = 2, vsamp = 1 */
        else
                *p++ = 0x22;    /* hsamp = 2, vsamp = 2 */
        *p++ = 0;               /* quant table 0 */
        *p++ = 1;               /* comp 1 */
        *p++ = 0x11;            /* hsamp = 1, vsamp = 1 */
        *p++ = 1;               /* quant table 1 */
        *p++ = 2;               /* comp 2 */
        *p++ = 0x11;            /* hsamp = 1, vsamp = 1 */
        *p++ = 1;               /* quant table 1 */
        p = MakeHuffmanHeader(p, lum_dc_codelens,
                              sizeof(lum_dc_codelens),
                              lum_dc_symbols,
                              sizeof(lum_dc_symbols), 0, 0);
        p = MakeHuffmanHeader(p, lum_ac_codelens,
                              sizeof(lum_ac_codelens),
                              lum_ac_symbols,
                              sizeof(lum_ac_symbols), 0, 1);
        p = MakeHuffmanHeader(p, chm_dc_codelens,
                              sizeof(chm_dc_codelens),
                              chm_dc_symbols,
                              sizeof(chm_dc_symbols), 1, 0);
        p = MakeHuffmanHeader(p, chm_ac_codelens,
                              sizeof(chm_ac_codelens),
                              chm_ac_symbols,
                              sizeof(chm_ac_symbols), 1, 1);
        *p++ = 0xff;
        *p++ = 0xda;            /* SOS */
        *p++ = 0;               /* length msb */
        *p++ = 12;              /* length lsb */
        *p++ = 3;               /* 3 components */
        *p++ = 0;               /* comp 0 */
        *p++ = 0;               /* huffman table 0 */
        *p++ = 1;               /* comp 1 */
        *p++ = 0x11;            /* huffman table 1 */
        *p++ = 2;               /* comp 2 */
        *p++ = 0x11;            /* huffman table 1 */
        *p++ = 0;               /* first DCT coeff */
        *p++ = 63;              /* last DCT coeff */
        *p++ = 0;               /* sucessive approx. */

        return (p - start);
};







// Adapted from RFC 2435
// The following routine is used to illustrate the RTP/JPEG packet
// fragmentation and header creation.

// For clarity and brevity, the structure definitions are only valid for
// 32-bit big-endian (most significant octet first) architectures. Bit
// fields are assumed to be packed tightly in big-endian bit order, with
// no additional padding. Modifications would be required to construct a
// portable implementation.

/*
* RTP data header from RFC1889
*/

#ifdef _MSC_VER
#pragma pack(push)
#pragma pack(1)
#pragma warning( disable : 4995 4996 )
#endif
//#define TIMESTAMP_DELTA   3003      // 29.97 hz

typedef struct {
  u16 rtpbits;              // ver, p, x, cc, m, pt, seq_num (msb first)
  u16 seq;                  /* sequence number */
  u32 ts;                   /* timestamp */
  u32 ssrc;                 /* synchronization source */
  u32 csrc[1];              /* optional CSRC list */
} rtp_hdr_t;

#define RTP_HDR_SZ 12

/* The following definition is from RFC1890 */
#define RTP_PT_JPEG             26
/* See RFC3551 */
#define RTP_PT_MP2TS            33 
#define RTP_PT_H264             96


struct jpeghdr {
  u32 tspec_off;      //!* type + fragment offset 
  u8 type;            /* id of jpeg decoder params */
  u8 q;               /* quantization factor (or table id) */
  u8 width;           /* frame width in 8 pixel blocks */
  u8 height;          /* frame height in 8 pixel blocks */
};

struct jpeghdr_rst {
  u16 dri;
#if _BIG_ENDIAN
  unsigned short f:1;
  unsigned short l:1;
  unsigned short count:14;
#else
  unsigned short count:14;
  unsigned short l:1;
  unsigned short f:1;
#endif
};

struct jpeghdr_qtable {
  u8  mbz;
  u8  precision;
  u16 length;
};

struct rtph264_hdr {

};

#ifdef _MSC_VER
#pragma pack(pop)
#endif

#if _TMS320C6X
#define HTON32(x) _rotl(_swap4(x),16);
#define HTON16(x) (u16)_swap4(x);
#else
static u32 HTON32(u32 x)
{
  u8 a = (x>>24)&0xff;
  u8 b = (x>>16)&0xff;
  u8 c = (x>>8)&0xff;
  u8 d = (x)&0xff;

  return (b<<8) | (a) | (d<<24) | (c<<16);
}
static u16 HTON16(u16 x)
{
  u8 c = (x>>8)&0xff;
  u8 d = (x)&0xff;

  return (d<<8) | (c);
}
#endif

#define RTP_JPEG_RESTART           0x40

static int demuxRTPMJpeg(UDPReceiveStruct *rtpData, SL_UDP_PACKET *packet, SLA_COMPRESSED_FRAME *frame, int finalFragment, u16 seq)
{
  struct jpeghdr jpghdr;
  s32 bytes = packet->len - (RTP_HDR_SZ + sizeof(jpghdr));

  SLAMemcpy(&jpghdr, packet->data + RTP_HDR_SZ, sizeof(jpghdr));
  u32 offset = HTON32(jpghdr.tspec_off & 0xFFFFFF00);
  if (offset == 0){
    rtpData->quality = jpghdr.q;
    rtpData->high = jpghdr.height;
    rtpData->wide = jpghdr.width;
    rtpData->type = jpghdr.type;
    SLAMemset(frame->buffer, 0, frame->maxBufferLen);
    //startSeq = seq;

    // Create jpeg header so that ffmpeg can decode

    MakeTables(jpghdr.q, rtpData->lumaq, rtpData->chromaq);
    rtpData->dataLen = MakeHeaders(frame->buffer, jpghdr.type, jpghdr.width, jpghdr.height, rtpData->lumaq, rtpData->chromaq, 0);
  }
  // Copy jpeg data to buffer
  if ((s32)offset + bytes < (s32)frame->maxBufferLen) {
    SLAMemcpy(frame->buffer + rtpData->dataLen + offset, packet->data + RTP_HDR_SZ + sizeof(jpghdr), bytes);
  }
  else {
    rtpData->failed++;
    rtpData->lastFailureType = RTPFAIL_BUFFER_OVERFLOW;
    //    SLTrace("bad offset %d (%d)\n", offset, frame->maxBuferLen);
  }

  if (rtpData->type < 0) {
    rtpData->failed++;
    rtpData->lastFailureType = RTPFAIL_TYPE_MISMATCH;
  }

  // TODO: fill in PTS
  if (finalFragment){
    // Same as mp2ts video PID
    frame->PID = 0x44;
    frame->streamType = SLA_UDP_VIDEO_PROTOCOL_RTPMJPEG;
    if (rtpData->failed){
      if ((rtpData->maxFailCount % 5) == 0) SLATrace("frame failed %d; lastFailureType (%d); maxFailCount(%d)\n", rtpData->failed, rtpData->lastFailureType, rtpData->maxFailCount);
      frame->len = 0;
    }
    else {
      frame->len = offset + rtpData->dataLen + packet->len - (RTP_HDR_SZ + sizeof(jpeghdr));
      frame->high = rtpData->high;
      frame->wide = rtpData->wide;
      frame->quality = rtpData->quality;
      frame->type = rtpData->type;
      frame->missedPacket = rtpData->failed;
    }
    rtpData->maxFailCount += rtpData->failed;
    rtpData->failed = 0;
    rtpData->lastFailureType = RTPFAIL_NONE;
  }
  return finalFragment;
}

static int demuxRTPH264(UDPReceiveStruct *rtpData, SL_UDP_PACKET *packet, SLA_COMPRESSED_FRAME *frame, int finalFragment, u16 seq)
{
  static const u8 NAL_HEADER_BYTES[4] = { 0, 0, 0, 1 };
  u8 *d = packet->data + RTP_HDR_SZ;

  // High bit must be 0
  if (d[0] & 0x80)
    return 0;

  u8 type = d[0] & 0x1F;
  if (type <= 23) {
    // Single NALU
    SLAMemcpy(frame->buffer, NAL_HEADER_BYTES, 4);
    SLAMemcpy(frame->buffer+4, d, packet->len - RTP_HDR_SZ);
    frame->len = packet->len - RTP_HDR_SZ + 4;
  }
  else if (type == 24) {
    // Aggregate single-time NALU
    SLAMemcpy(frame->buffer, NAL_HEADER_BYTES, 4);
    SLAMemcpy(frame->buffer+4, d, packet->len - RTP_HDR_SZ);
    frame->len = packet->len - RTP_HDR_SZ + 4;
  }
  else if (type == 28) {
    // NALU fragment
    u8 s, e, r;
    s = d[1] >> 7;
    e = (d[1] >> 6) & 1;
    r = (d[1] >> 5) & 1;
    // Check for invalid format
    if (r == 1)
      return 0;
    // Start of fragment
    if (s) {
      SLAMemcpy(frame->buffer, NAL_HEADER_BYTES, 4);
      frame->buffer[4] = (d[0] & 0xE0) | (d[1] & 0x1F);
      rtpData->dataLen = 5;
    }
    SLAMemcpy(frame->buffer + rtpData->dataLen, d + 2, packet->len - RTP_HDR_SZ - 2);
    rtpData->dataLen += packet->len - RTP_HDR_SZ - 2;
    if (e){
      static int cnt = 0;
      cnt++;
      frame->len = rtpData->dataLen;
      frame->PID = 0x44;
    }

    return e;
  } else {
    // Unknown type code
    SLATrace("%s: unkown type code %d\n", __FUNCTION__, type);
    return 0;
  }

  frame->type = rtpData->type;
  return 1;

}

static int demuxRTPMP2TS(UDPReceiveStruct *rtpData, SL_UDP_PACKET *packet, SLA_COMPRESSED_FRAME *frame, int finalFragment, u16 seq, SL_UDP_PACKET *tsPkt)
{
  tsPkt->data = packet->data + RTP_HDR_SZ;
  tsPkt->len = packet->len - RTP_HDR_SZ;
  return finalFragment;
}

static int demuxRTPPacket(UDPReceiveStruct *rtpData, SL_UDP_PACKET *packet, SLA_COMPRESSED_FRAME *frame, SL_UDP_PACKET *tsPkt)
{
  //static u16 startSeq=0;
  // TODO: Sanity check header -- does it look right???
  rtp_hdr_t rtphdr;

  tsPkt->data = 0;
  tsPkt->len = 0;
  // copy to avoid data alignment problems on TI
  SLAMemcpy(&rtphdr, packet->data, RTP_HDR_SZ);

  u8 rtp_pt = (rtphdr.rtpbits >> 8)&0x7F;
  // set if "m" bit is set
  int finalFragment = (rtphdr.rtpbits >> 15);
  // Detect out-of order or mising sequence numbers
  u16 seq = HTON16(rtphdr.seq);

  if (rtpData->prevseq != -1 && ((rtpData->prevseq + 1) & 0xFFFF) != seq){
    rtpData->failed++;
    rtpData->lastFailureType = RTPFAIL_SEQUENCE_MISMATCH;
    //SLTrace("missed sequence number %i %i\n", rtpData->prevseq-startSeq, seq-startSeq);
    // jas 6/4/15 - usually cause by missed packets.  Is the network recieve buffer big enough? @see slsock.h  MAX_READ_BUFFER_SIZE 
  }
  rtpData->prevseq = seq;

  // TODO: generic timestamp, seqence, etc

  switch (rtp_pt){
  case RTP_PT_JPEG:
    frame->streamType = SLA_UDP_VIDEO_PROTOCOL_RTPMJPEG;
    return demuxRTPMJpeg(rtpData, packet, frame, finalFragment, seq);
    break;
  case RTP_PT_MP2TS:
    frame->streamType = SLA_UDP_VIDEO_PROTOCOL_RTPMP2H264;
    return demuxRTPMP2TS(rtpData, packet, frame, finalFragment, seq, tsPkt);
    break;
  case RTP_PT_H264:
    frame->streamType = SLA_UDP_VIDEO_PROTOCOL_RTPH264;
    return demuxRTPH264(rtpData, packet, frame, finalFragment, seq);
    break;
  default:
    break;
  }

  return 0;

}

static s32 readDataPacket(UDPReceiveStruct *data, SL_UDP_PACKET *pkt, u32 timeout)
{
  s32 rv;
  pkt->len = 0;
  rv = SLASockRecvFrom(&data->RcvSocket, (char*)pkt->data, UDP_PACKET_LEN, timeout);
  if(rv<=0)
    return rv;
//  pkt->timestamp = SLGetTimeTickHighRes();
  pkt->len = rv;
  return rv;
}

static SLStatus _demuxNextFrame(void *UDPReceiveData, SLA_COMPRESSED_FRAME *frame, u32 timeout)
{
  s32 haveFrame = 0;
  UDPReceiveStruct *data = (UDPReceiveStruct *)UDPReceiveData;

  //bool checkForLatePacket = true;
  // Grab UDP packets and demux them until a frame has been decoded.

  while(!haveFrame && !data->done){

    if(data->bytesProcessed >= (s32)data->pkt.len) {
#if USE_CUSTDATA
      if (data->readPkt) {
        SLStatus st = data->readPkt(data, &data->pkt, data->readPktCtx);
        if (st == SLA_TERMINATE) {
          data->done = 1;
          data->pkt.len = 0;
          return st;
        }
        if (st != SLA_SUCCESS) {
          SLATrace("%s %d: st = %d\n", __FUNCTION__, __LINE__, st);
          data->pkt.len = 0;
          return st;
        }
      }
      else {
        readDataPacket(data, &data->pkt, timeout);
      }
#else
      readDataPacket(data, &data->pkt);
#endif

      if(data->pkt.len == 0)  // TODO: fix timeout
        return SLA_TIMEOUT;
#if PACKET_DEBUG
      SLATrace("%d bytes read\n", data->pkt.len);
#endif
      data->bytesProcessed = 0;
  
      // Dump raw input if requested (before demuxing, decoding, etc)
      if(data->pkt.len>0){
        if(data->dumpFile){
          fwrite(data->pkt.data, data->pkt.len, 1, data->dumpFile);
          fflush(data->dumpFile);
        }
      }
    }

    // TODO: more sophisticated ways of figuring out the stream type
    // could be used.  For now, if first byte == 0x47 assume it's 
    // mpeg2ts, else rtpmjpeg
    // 0x47 is the Sync Byte.

    if(!data->isRTPts && data->pkt.data[0] == 0x47) {
      // multiple frames can be present in one packet: use bytesProcessed and pkt->len
      // to determine when packet processing is complete
      haveFrame = demuxTSPacket(data, &data->pkt, frame, &data->bytesProcessed, 0);
      data->isRTPts = false;
#if PACKET_DEBUG
      if(haveFrame)
        SLATrace("--------------------------------\n");
#endif

    } else {
      // RTP doesn't bother with null packets
      if(data->pkt.len>0){
        SL_UDP_PACKET tsPkt;
        if (data->bytesProcessed == 0) {
          // Entire udp packet always consumed (multiple frames do not exist in one packet)
          haveFrame = demuxRTPPacket(data, &data->pkt, frame, &tsPkt);
          // If there is TS data inside the RTP encapsulation
          if (tsPkt.data) {
            data->isRTPts = true;
            haveFrame = demuxTSPacket(data, &tsPkt, frame, &data->bytesProcessed, 1);
            // Skip RTP header
            data->bytesProcessed += (data->pkt.len - tsPkt.len);
          }
          else {
            data->isRTPts = false;
            data->bytesProcessed = data->pkt.len;
          }
        }
        else {
          // bytes left over from single RTP packet
          if (data->isRTPts){
            tsPkt.len = data->pkt.len - data->bytesProcessed;
            tsPkt.data = data->pkt.data + data->bytesProcessed;
            s32 bytes = 0;
            haveFrame = demuxTSPacket(data, &tsPkt, frame, &bytes, 1);
            data->bytesProcessed += bytes;
          }
          else{
            SLTrace("Error: other RTP formats don't multiplex streams\n");
            return SLA_ERROR;
          }
        }
      }
    }
  }
  if(data->done)
    return SLA_ERROR;
  return SLA_SUCCESS;
}

static int udpReceiveTask(void *_data)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)_data;
  initSocket(data);
#if WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

  SLStatus rv = SLA_SUCCESS;
  SLA_COMPRESSED_FRAME frame;

  SLASemPost(data->dumpSem);

  while(!data->done){
    if(!SLAMbxPend(data->emptyMbx, &frame, 0)) {
      SLASemPend(data->lockSem, SL_FOREVER);
      data->busy = 1;
      SLASemPost(data->lockSem);
      while(!data->done && !SLAMbxPend(data->emptyMbx, &frame, 100)){
//        SLTrace("Empty mbx timeout!!!\n");
      }
    }
    //SLMemset(frame.buffer, 0xff, frame.maxBufferLen);
    SLASemPend(data->dumpSem, SL_FOREVER);
    rv = _demuxNextFrame(data, &frame, 100);
    SLASemPost(data->dumpSem);
    if(!data->done){
      if(rv==SLA_TIMEOUT)
        SLAMbxPost(data->emptyMbx, &frame, SL_FOREVER);
      if(rv==SLA_SUCCESS)
        SLAMbxPost(data->fullMbx, &frame, SL_FOREVER);
    }
  }

  if(rv==SLA_TERMINATE) {
    frame.buffer = 0; // Notify we are terminating.
    SLAMbxPost(data->fullMbx, &frame, SL_FOREVER);
  }

  SLASemPost(data->doneSem);

  return 0;
}

SLStatus SLADemuxNextFrame(void *UDPReceiveData, SLA_COMPRESSED_FRAME *frame, u32 timeout)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)UDPReceiveData;
  SLStatus rv = SLA_TIMEOUT;
  // Could eliminate the copy here if interface is changed to a get/release type of thing
  u32 mbl = frame->maxBufferLen;
  u8 *b = frame->buffer;
  if(SLAMbxPend(data->fullMbx, frame, timeout)) {
    if(frame->buffer == 0) {
      rv = SLA_TERMINATE;
    } 
    else {
      s32 cpyLen = SLMIN(frame->len, frame->maxBufferLen);
      if (mbl >= cpyLen){
        SLAMemcpy(b, frame->buffer, cpyLen);
        rv = SLA_SUCCESS;
      }
      else
        rv = SLA_FAIL;
      SLAMbxPost(data->emptyMbx, frame, SL_FOREVER);
    }
  }
  // restore caller-specified fields
  frame->maxBufferLen = mbl;
  frame->buffer = b;

  return rv;
}

SLStatus SLAUDPStatus(void *UDPReceiveData, SLA_UDP_STATUS *status)
{
  UDPReceiveStruct *data = (UDPReceiveStruct *)UDPReceiveData;
  SLASemPend(data->lockSem, SL_FOREVER);
  status->backedUp = data->busy;
  data->busy = 0;
  SLASemPost(data->lockSem);
  return SLA_SUCCESS;
}
