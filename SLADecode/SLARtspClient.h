/*
* Copyright (C)2007-2016 SightLine Applications Inc
* SightLine Applications Library of signal, vision, and speech processing
* http://www.sightlineapplications.com
*------------------------------------------------------------------------*/

#pragma once

#include "SLAHal.h"
#include <vector>

#define RTSP_DEFAULT_PORT 554
#define RTSP_RECV_BUF_SIZE 4096

#define RTP_DEFAULT_PORT 16024
#define RTP_DEFAULT_PORT_NET1 16044

#define STREAMING_NET0 1
#define STREAMING_NET1 2

#define RTSP_DEFAULT_KEEP_ALIVE 60

#define MAX_PORT_VALUE 65535

#define WAIT_TO_EXIT_KEEP_ALIVE_THREAD 5000
#define KEEP_ALIVE_TASK_LOOP_SLEEP 2000

typedef enum {
  RTSP_OPTIONS = 0,
  RTSP_DESCRIBE,
  RTSP_SETUP,
  RTSP_PLAY,
  RTSP_PAUSE,
  RTSP_GETPARAMETER,
  RTSP_SETPARAMETER,
  RTSP_TEARDOWN,
}RTSP_METHOD;

typedef struct
{
  std::string allow;
  std::string contentType;
  std::string range;
  std::string retryAfter;
  std::string rtpInfo;
  std::string server;
  std::string unsupported;
  std::string wwwAuthenticate;
  std::string cseq;
  std::string cacheControl;
  std::string connection;
  std::string date;
  std::string sessionLine;
  std::string transport;
  std::string contentBase;
  std::string contentEncoding;
  std::string contentLanguage;
  std::string contentLength;
  std::string contentLocation;
  std::string expires;
  std::string lastModified;
} ResponseHeader;

class RtspClient {

private:
  int keepAliveTimeout;
  int lastCommand;

  u32 rtspPort; //default is 554
  std::string serverIp;
  std::string connectionIp;

  std::vector<std::string> splitStream(const std::string& str, const char* delim);
  SLStatus parsePacket(const char* str, unsigned long size);
  void setKeepAlive(int keepAlive);
  void setLastCommand(int cmd);
  int getLastCommand();
  void onResponseSetup();
  void onResponseDescribe();
  void setServerIp(std::string);
  void setRtspPort(u32 port);
  SLStatus checkValidIp(std::string ipAddr);
  SLStatus setMulticastIp(std::string ip);
  const char* getServerIp();
  const char* getConnectionIp();
  SLStatus validateResponse();
  u32 getRtspPort();
  SLStatus sendRtspMsg(std::string msg, size_t size);
  SLStatus disconnectSockets();


public:
  SLASocket sock;
  std::string url;
  int cSeq;

  char *recvBuf;
  char *rtcpRecvBuf;
  ResponseHeader respHeader;
  std::string lastRespString;
  std::string sessionIdString;

  char localIpAddr[16]; // ip addr

  int rtpPortNum;
  SLASocket rtcpUdpSocket;

  SLA_Sem keepAliveTaskExitSem;
  SLA_Sem stopKeepAliveThreadSem;

  void *udpReceiver;

  RtspClient();
  ~RtspClient();

  bool stopStreamingSession;

  bool isMulticastSession;
  int streamingDispId;

  SLStatus sendRequest(RTSP_METHOD method);
  SLStatus receiveResponse();
  SLStatus createTCPSocket();
  SLStatus createThreads();
  SLStatus stopStreaming(bool destroyReceiver = true);
  SLStatus setRtpPort(u32 rtpPort);
  int getKeepAlive();
  SLStatus createRtpPorts();
  SLStatus setUrl(const char *rtspUrl);

};

RtspClient *createRtspClient(char const *url);
void * SLARtspOpenURL(const char * url, RtspClient ** rtspClient, int rtpPort = 0);

