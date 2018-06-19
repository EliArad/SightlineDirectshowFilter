/*
* Copyright (C)2008-2013 SightLine Applications Inc
* SightLine Applications Library of signal, vision, and speech processing
* http://www.sightlineapplications.com
*
*------------------------------------------------------------------------*/
#include <time.h>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>
#include <stdexcept> 
#include "SLARtspClient.h"
#include "SLAUdpReceive.h"

// Disable warnings about vsprintf
#include "WIN_WARN_DISABLE.h"

const std::string RTSPVersion = "RTSP/1.0";

RtspClient * createRtspClient(const char * rtspUrl) {
  RtspClient *rtspClient = new RtspClient();
  if (rtspClient->setUrl(rtspUrl) == SLA_FAIL) {
    SLATrace("Invalid RTSP URL passed \n");
    delete rtspClient;
    rtspClient = NULL;
  }
  return rtspClient;
}

RtspClient::RtspClient() {
  recvBuf = new char[RTSP_RECV_BUF_SIZE];
  isMulticastSession = false;
  cSeq = 0;
  rtpPortNum = 0;
  keepAliveTaskExitSem = SLASemCreate(0, "keepAliveTaskExitSem");
  stopKeepAliveThreadSem = SLASemCreate(0, "stopKeepAliveThreadSem");
  udpReceiver = NULL;

}

RtspClient::~RtspClient() {
  SLATrace("Calling destructor of RtspClient \n");
  if (recvBuf != NULL)
    delete recvBuf;
  SLASemDestroy(keepAliveTaskExitSem);
  SLASemDestroy(stopKeepAliveThreadSem);
}

void RtspClient::setServerIp(std::string ipAddr) {
  serverIp = ipAddr;
}

void RtspClient::setRtspPort(u32 port) {
  rtspPort = port;
}

u32 RtspClient::getRtspPort() {
  return rtspPort;
}

const char * RtspClient::getServerIp() {
  return  serverIp.c_str();
}

SLStatus RtspClient::setMulticastIp(std::string ip) {
  const char * ipStr = ip.c_str();
  u32 tmp = inet_addr(ipStr);
  
  if ((tmp & 0xFF) >= 224 && (tmp & 0xFF) <= 239)
    isMulticastSession = true;
  return SLA_SUCCESS;
}

const char * RtspClient::getConnectionIp() {
  return connectionIp.c_str();
}

SLStatus RtspClient::checkValidIp(std::string ipAddr) {
  std::vector<std::string> ipAddrSplit = splitStream(ipAddr, ".");

  if (ipAddrSplit.size() != 4) {
    // invalid ip
    return SLA_FAIL;
  }
  try {
    if (stoi(ipAddrSplit[0]) > 254 || stoi(ipAddrSplit[1]) > 254 ||
      stoi(ipAddrSplit[2]) > 254 || stoi(ipAddrSplit[3]) > 254) {
      // invalid ip
      return SLA_FAIL;
    }
  } catch (const std::invalid_argument& ia) {
    SLATrace("Exception Invalid ip address %s", ia.what());
    return SLA_FAIL;
  }

  return SLA_SUCCESS;
}

SLStatus RtspClient::setUrl(const char *rtspUrl) {
  char *ipAddr = NULL;
  if (strlen(rtspUrl) > 255) {
    return SLA_FAIL;
  }
  std::string urlString = rtspUrl;
  std::vector<std::string> vUrlSplit = splitStream(urlString, "/"); 

  if (vUrlSplit.size() < 2 || vUrlSplit.size() > 8) {
    return SLA_FAIL;
  }

  u32 tempPort = RTSP_DEFAULT_PORT;
  int pos = vUrlSplit[1].find(':');
  std::string ipAddrStr = "";
  if (pos != std::string::npos) {

    if (pos < 8) {
      return SLA_FAIL; // Too short Ip
    }
    try {
      tempPort = stoi(vUrlSplit[1].substr(pos + 1));
    }
    catch (const std::invalid_argument& ia) {
      SLATrace("Exception Invalid port %s", ia.what());
      tempPort = 0;
    }

    if (tempPort == 0 || tempPort > MAX_PORT_VALUE) {
      SLATrace("Invalid port passed : %d \n", tempPort);
      return SLA_FAIL;
    }
    ipAddrStr = vUrlSplit[1].substr(0, pos);
  } else {
    ipAddrStr = vUrlSplit[1];
  }

  if (checkValidIp(ipAddrStr) != SLA_SUCCESS) {
    SLATrace("Invalid ip in url \n");
    return SLA_FAIL;
  }

  if (urlString.find("net1", 15) == std::string::npos) {
    streamingDispId = STREAMING_NET0;
  }
  else {
    streamingDispId = STREAMING_NET1;
  }
  setServerIp(ipAddrStr);
  setRtspPort(tempPort);
  // Check whether client port is passed in url then that should be the end of the url
  // Strip clientPort=<clientport> from the url
  int portStringPos = 0;
  if (vUrlSplit.size() >= 3 && (portStringPos = urlString.find("clientPort=", 15)) != std::string::npos) {
    int posEnd = urlString.find("/", portStringPos);
    std::string clientPortStr = urlString.substr(portStringPos + 11, posEnd);
    url = urlString.substr(0, portStringPos);
    rtpPortNum = std::stoi(clientPortStr);

  }
  else {
    url = std::string(rtspUrl);
  }
  return SLA_SUCCESS;
}

void RtspClient::setLastCommand(int method) {
  lastCommand = method;
}

int RtspClient::getLastCommand() {
  return lastCommand;
}

SLStatus RtspClient::sendRequest(RTSP_METHOD method) {

  std::stringstream msg("");
  switch (method) {
    case RTSP_OPTIONS:
    {
      msg << "OPTIONS" << " " << url << " " << RTSPVersion << "\r\n";
      msg << "CSeq: " << ++cSeq << "\r\n";
      msg << "\r\n";
      break;
    }
    case RTSP_DESCRIBE:
    {
      msg << "DESCRIBE" << " " << url << " " << RTSPVersion << "\r\n";
      msg << "CSeq: " << ++cSeq << "\r\n";
      msg << "Accept: application/sdp" << "\r\n";
      msg << "\r\n";
      break;
    }
    case RTSP_SETUP:
    { 
      std::stringstream rtpSS;
      std::stringstream rtcpSS;
      rtpSS << rtpPortNum;
      rtcpSS << rtpPortNum + 1;

      // TODO to pass control uri
      msg << "SETUP" << " " << url << " " << RTSPVersion << "\r\n";
      msg << "CSeq: " << ++cSeq << "\r\n";
      msg << "Transport:" << " " << "RTP/UDP;";
      msg << "unicast;" << "client_port=" << rtpSS.str()<< "-" << rtcpSS.str()  << "\r\n";
      msg << "\r\n";
      break;
    }
    case RTSP_PLAY:
    {
      msg << "PLAY" << " " << url << " " << RTSPVersion << "\r\n";
      msg << "CSeq: " << ++cSeq << "\r\n";
      msg << "Session: " << sessionIdString << "\r\n";
      msg << "\r\n";

      break;
    }
    case RTSP_TEARDOWN:
    {
      msg << "TEARDOWN" << " " << url << " " << RTSPVersion << "\r\n";
      msg << "CSeq: " << ++cSeq << "\r\n";
      msg << "Session: " << sessionIdString << "\r\n";
      msg << "\r\n";

      break;
    }
    default:
    {
      SLATrace("Unsupported command \n");
      return SLA_FAIL;
    }
  }
  setLastCommand(method);

  return sendRtspMsg(msg.str(), msg.str().size());
}


SLStatus RtspClient::sendRtspMsg(std::string msg, size_t size) {

  if (size == 0) {
    SLATrace("Error invalid params \n");
    return SLA_FAIL;
  }

  s32 rv = SLASockSendTo(&sock, (char*)msg.c_str(), size);
 
  if (rv < 0) {
    SLATrace(" Unable to write to socket \n");
    return SLA_FAIL;
  }
  return SLA_SUCCESS;
}

SLStatus RtspClient::validateResponse() {
  return SLA_SUCCESS;
};

SLStatus RtspClient::receiveResponse() {
  memset(recvBuf, 0, RTSP_RECV_BUF_SIZE);
  s32 rv = SLASockRecvFrom(&sock, recvBuf, RTSP_RECV_BUF_SIZE, SL_FOREVER);
  if (rv < 0 || (parsePacket(recvBuf, rv) == SLA_FAIL)) {
    SLATrace("ERR: %s_#%d\n", __FUNCTION__, __LINE__);
    return SLA_FAIL;
  }

  SLATrace(" received RTSP response is \n %s", recvBuf);
  
  if (lastCommand == RTSP_SETUP) {
    onResponseSetup();
  }
  else if (lastCommand == RTSP_DESCRIBE) {
    onResponseDescribe();
  }

  return SLA_SUCCESS;
}

SLStatus RtspClient::createTCPSocket() {
  sock.addr = inet_addr(getServerIp());
  sock.port = rtspPort;

  if (s32 rv = SLASockClientConnect(&sock, SL_FOREVER)) {
    SLATrace("ERR: %s_#%d st=%d\n", __FUNCTION__, __LINE__, rv);
    return SLA_FAIL;
  }
  SLATrace("Successfully connected to rtsp server \n");
  return SLA_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////////
SLStatus RtspClient::disconnectSockets() {
  SLASockDisconnect(&sock);
  return SLA_SUCCESS;
}

// Not used for now
// rtcp port is choosen as next higher level
// Pass 0 to choose a random port
// rtp will continue without rtcp if unable to create rtcp port

SLStatus RtspClient::createRtpPorts() {
  SLASockGetHostInfo(0, 0, 0, 0, localIpAddr, 16);
  SLATrace("ip address : %s \n\r",  localIpAddr);
  SLASocket tempSocket;
  tempSocket.addr = inet_addr(localIpAddr);
  if (rtpPortNum == 0 && isMulticastSession == false) {
    int i = 0;
    //rtcpUdpSocket.addr = inet_addr(localIpAddr);
    srand((u32)time(NULL));

    for (i = 0; i < 10; i++) {
      u32 randomPort = 15400 + (rand() % 2000) + i * 2;
      if (randomPort % 2 != 0) randomPort = randomPort + 1;

      tempSocket.port = randomPort;

      if (SLASockServerBind(&tempSocket, SOCK_DGRAM, IPPROTO_UDP, 0)) {
        continue;
      }
      else {
        rtpPortNum = randomPort;
        break;
      }
#if 0
      rtcpUdpSocket.port = randomPort + 1;

      if (SLASockServerBind(&rtcpUdpSocket, SOCK_DGRAM, IPPROTO_UDP, 0)) {
        SLASockDisconnect(&tempSocket);
        SLASockClose(&tempSocket);
      } else {
        rtpPortNum = randomPort;
        break;
      }
#endif
    }

    if (i == 10) {
      SLATrace("Unable to open rtp socket with ports ");
      return SLA_FAIL;
    }
    else {
      SLASockClose(&tempSocket);
      SLASockCleanup();
    }
  }
  else 
  {
#if 0
    // TODO create RTCP port in this case 
    char * connIp = localIpAddr;
    if (isMulticastSession == true) {
      connIp = 0;
    }
    if (rtpPortNum <= 0 && rtpPortNum % 2 != 0) {
      SLATrace("Invalid rtp port number passed %d", rtpPortNum);
      return SLA_FAIL;
    }

    // Set up networking
    rtpUdpSocket.addr = inet_addr("0.0.0.0");
    rtpUdpSocket.port = rtpPortNum;
    rtcpUdpSocket.addr = inet_addr("0.0.0.0");
    rtcpUdpSocket.port = rtpPortNum + 1;
    if (SLASockServerBind(&rtpUdpSocket, SOCK_DGRAM, IPPROTO_UDP, 0) != 0) {
      SLATrace("Unable to create socket with rtp port number %d", rtpPortNum);
      return SLA_FAIL;
    }
#endif
    if (isMulticastSession == false) {
      tempSocket.port = rtpPortNum;

      if (SLASockServerBind(&tempSocket, SOCK_DGRAM, IPPROTO_UDP, 0)) {
        SLATrace("Unable to bound to rtp port number %d", rtpPortNum);
        return SLA_FAIL;
      }
    }
  }

  return SLA_SUCCESS;
}

// splitstream ignores null string, it wont split ab//cd is split to ab and cd ignoring nullstring

std::vector<std::string> RtspClient::splitStream(const std::string& str, const char* delim)
{
  std::vector<std::string> vItems;
  if (str == "")	
    return vItems;
  if (delim == NULL)	
  {
    vItems.push_back(str);
    return vItems;
  }

  std::string strStream = str;
  int nLength = ::strlen(delim);
  int nPos = 0;
  while ((nPos = strStream.find(delim)) != -1)
  {
    if (nPos == 0)
    {
      strStream.erase(0, nLength);		
      continue;
    }
   
    vItems.push_back(strStream.substr(0, nPos));
    strStream.erase(0, nPos + nLength);		
  }
  if (strStream.length() != 0)	
    vItems.push_back(strStream);
  return vItems;
}

void RtspClient::setKeepAlive(int keepAlive) {
  keepAliveTimeout = keepAlive;
}

int RtspClient::getKeepAlive() {
  return keepAliveTimeout;
}

void RtspClient::onResponseDescribe() {
  std::vector<std::string> vDescribeResponse = splitStream(lastRespString, "\r\n");

  std::vector<std::string>::const_iterator iter = vDescribeResponse.begin();
  connectionIp = serverIp;
  for (; iter != vDescribeResponse.end(); iter++)
  {
    if ((*iter)[0] == 'c' && (*iter)[1] == '=')  {
      std::vector<std::string> vConnectionInfo = splitStream(*iter, " ");
      if (vConnectionInfo.size() >= 3) {
        std::size_t pos = vConnectionInfo[2].find("/");
        std::string tempIp = vConnectionInfo[2];
        if (pos != std::string::npos) {
          tempIp = vConnectionInfo[2].substr(0, pos);
        }
        if (checkValidIp(tempIp) == SLA_SUCCESS) {
          connectionIp = tempIp;
          setMulticastIp(tempIp);
        }
      }
    } else if ((*iter)[0] == 'm' && (*iter)[1] == '=') {
      std::vector<std::string> vMediaDesc = splitStream(*iter, " ");
      if (vMediaDesc.size() >= 3) {
        try {
          rtpPortNum = stoi(vMediaDesc[1]);

        }
        catch (const std::invalid_argument& ia) {
          SLATrace("Exception Invalid port %s", ia.what());
          rtpPortNum = 0;
        }
        if (rtpPortNum > MAX_PORT_VALUE) {
          SLATrace("Invalid RTP Port obtained in sdp \n");
          rtpPortNum = 0;
        }
      }
    }
  }
}

void RtspClient::onResponseSetup() {
  std::vector<std::string> vSessionSplit = splitStream(respHeader.sessionLine, ";");
  sessionIdString = vSessionSplit[0];
  std::string timeoutS  = vSessionSplit[1];
  std::size_t pos = timeoutS.find("=");
  int timeout = RTSP_DEFAULT_KEEP_ALIVE;

  if (pos != std::string::npos && pos != timeoutS.size()) {
    int temp = atoi(timeoutS.substr(pos+1).c_str()); // skip '='

    if (temp >= 60) {  // atleast 60 
      timeout = temp;
    }
  }

  setKeepAlive(timeout);
  // TODO need to validate session id string obtained
}

SLStatus RtspClient::parsePacket(const char* cstr, unsigned long size)
{
  if (size == 0 || cstr == NULL) {
    SLATrace("parsePacket received null string \n");
    return SLA_FAIL;
  }
  std::string str(cstr, size);
  std::vector<std::string> vSplitWord;
  std::vector<std::string> vSplitWordList = splitStream(str, "\r\n");
  std::string statusCode;
  std::string statusOk = std::string("200");

  vSplitWord = splitStream(vSplitWordList[0], " ");
  if (vSplitWord[0] != RTSPVersion)
    SLATrace("Invalid RTSP Version\n");
  statusCode = vSplitWord[1];

  if (statusCode.compare(statusOk) != 0) {
    SLATrace("Error code %s received, Cannot stream ", statusCode.c_str());
    return SLA_FAIL;
  }

  std::vector<std::string>::const_iterator iter = vSplitWordList.begin() + 1;
  for (; iter != vSplitWordList.end(); iter++)
  {
    vSplitWord = splitStream(*iter, ": ");	
    if (vSplitWord.size() != 2) break;

    if (vSplitWord[0] == "CSeq")								respHeader.cseq = vSplitWord[1];
    else if (vSplitWord[0] == "Connection")					respHeader.connection = vSplitWord[1];
    else if (vSplitWord[0] == "Date")								respHeader.date = vSplitWord[1];
    else if (vSplitWord[0] == "Session")							respHeader.sessionLine = vSplitWord[1];
    else if (vSplitWord[0] == "Transport")						respHeader.transport = vSplitWord[1];
    else if (vSplitWord[0] == "Allow")								respHeader.allow = vSplitWord[1];
    else if (vSplitWord[0] == "Content-Type")				respHeader.contentType = vSplitWord[1];
    else if (vSplitWord[0] == "Range")								respHeader.range = vSplitWord[1];
    else if (vSplitWord[0] == "Retry-After")						respHeader.retryAfter = vSplitWord[1];
    else if (vSplitWord[0] == "RTP-Info")							respHeader.rtpInfo = vSplitWord[1];
    else if (vSplitWord[0] == "Server")								respHeader.server = vSplitWord[1];
    else if (vSplitWord[0] == "Unsupported")					respHeader.unsupported = vSplitWord[1];
    else if (vSplitWord[0] == "WWWAuthenticate")		respHeader.wwwAuthenticate = vSplitWord[1];
    
    else if (vSplitWord[0] == "Content-Base")					respHeader.contentBase = vSplitWord[1];
    else if (vSplitWord[0] == "Content-Encoding")		respHeader.contentEncoding = vSplitWord[1];
    else if (vSplitWord[0] == "Content-Language")		respHeader.contentLanguage = vSplitWord[1];
    else if (vSplitWord[0] == "Content-Length")			respHeader.contentLength = vSplitWord[1];
    else if (vSplitWord[0] == "Content-Location")			respHeader.contentLocation = vSplitWord[1];
    else;	
  }
  lastRespString = "";
  for (; iter != vSplitWordList.end(); iter++)
    lastRespString += (*iter) + "\r\n";
  //SLATrace("lastRespString parsed string %s", lastRespString.c_str());
  return SLA_SUCCESS;
}

SLStatus RtspClient::stopStreaming(bool destroyReceiver) {
  SLATrace("Received stop streaming \n");
  SLASemPost(stopKeepAliveThreadSem);

  if (destroyReceiver == true && udpReceiver != NULL) {
    SLADestroyUDPReceive(udpReceiver);
    udpReceiver = NULL;
  }

  if (sendRequest(RTSP_TEARDOWN) == SLA_FAIL) {
    SLATrace("Unable to send teardown \n");
  }
  else {
    if (receiveResponse() == SLA_FAIL) {
      SLATrace("receive response returned error \n");
    }
  }

  disconnectSockets();
  SLASemPend(keepAliveTaskExitSem, WAIT_TO_EXIT_KEEP_ALIVE_THREAD);

  return SLA_SUCCESS;
}

SLStatus RtspClient::setRtpPort(u32 rtpPort) {
  if (rtpPortNum != 0 && isMulticastSession == true && rtpPort != rtpPortNum) {
    SLATrace("Unable to set rtp port in a multi cast session new %d curr %d", rtpPort, rtpPortNum);
    return SLA_FAIL;
  }

  if (rtpPort <= 0 && rtpPort % 2 != 0) {
    SLATrace("Invalid rtp port number passed %d", rtpPort);
    return SLA_FAIL;
  }
  rtpPortNum = rtpPort;
  return SLA_SUCCESS;
}

/* Task to send rtsp keep alive periodically, we send rtsp options as keep alive */
int rtpClientKeepAliveTask(void *context) {
  SLATrace("Starting keep alive task \n");
  RtspClient *ctxt = (RtspClient *)(context);
  int sleepDuration = (ctxt->getKeepAlive() - 10) * 1000; // sleep 10 sec less of keep alive
  int sleptTime = 0;
  do {
    Sleep(KEEP_ALIVE_TASK_LOOP_SLEEP);
    sleptTime = KEEP_ALIVE_TASK_LOOP_SLEEP + sleptTime;
    if (sleptTime > sleepDuration) {
      ctxt->sendRequest(RTSP_OPTIONS);
      if (ctxt->receiveResponse() == SLA_FAIL) {
        SLATrace("receive response for option returned error \n");
        break;
      }
      sleptTime = 0;
    }
  } while (!SLASemPend(ctxt->stopKeepAliveThreadSem, 0));
  SLASemPost(ctxt->keepAliveTaskExitSem);
  SLATrace("rtpClientKeepAliveTask exited \n");

  return 0;
}

SLStatus RtspClient::createThreads() {
  SLATrace("Creating rtp Transport receive Thread for stream\n");
  char localIp[16];
  SLASockGetHostInfo(0, 0, 0, 0, localIp, 16);
  char * connIp = localIp;
  if (connIp == NULL) {
    SLATrace("Unable to extract local IP \n");
    return SLA_FAIL;
  }
  if (isMulticastSession == true) {
    connIp = (char *)getConnectionIp();
  }
  //TODO check right priority
  udpReceiver = SLAInitUDPReceive(100, (char *)connIp, rtpPortNum, false);
  // TODO need to create RTCP thread
  SLACreateThread(rtpClientKeepAliveTask, SL_DEFAULT_STACK_SIZE, "rtspClientKeepAliveTask", this, SL_PRI_6);
  return SLA_SUCCESS;

}

void* SLARtspOpenURL(const char * rtspUrl,  /* out*/ RtspClient **rtspClientPtr, int rtpPort) {
  RtspClient *rtspClient = createRtspClient(rtspUrl);

  if (rtspClient == NULL) {
    SLATrace("Unable to create rtsp client \n");
    return NULL;
  }

  if (rtspClient->createTCPSocket() == SLA_FAIL) {
    SLATrace("createTCPListenSocket returned error\n");
    goto error;
  }

  if (rtspClient->sendRequest(RTSP_OPTIONS) == SLA_FAIL) {
    SLATrace("openURL send Options returned error \n");
    goto error;
  }

  if (rtspClient->receiveResponse() == SLA_FAIL) {
    SLATrace("receive response returned error \n");
    goto error;
  }

  if (rtspClient->sendRequest(RTSP_DESCRIBE) == SLA_FAIL) {
    SLATrace("openURL send Describe returned error \n");
    goto error;
  }

  if (rtspClient->receiveResponse() == SLA_FAIL) {
    SLATrace("receive response returned error \n");
    goto error;
  }

  if (rtpPort != 0 && rtspClient->setRtpPort(rtpPort) == SLA_FAIL) {
    SLATrace("Unable to set rtp port %d\n", rtpPort);
    goto error;
  }

  if (rtspClient->createRtpPorts() == SLA_FAIL) {
    SLATrace("CreateRtpPorts returned failure \n");
    goto error;
  }

  if (rtspClient->sendRequest(RTSP_SETUP) == SLA_FAIL) {
    SLATrace("openURL send Setup returned error \n");
    goto error;
  }

  if (rtspClient->receiveResponse() == SLA_FAIL) {
    SLATrace("receive response returned error \n");
    goto error;
  }
  if (rtspClient->createThreads() == SLA_FAIL) {
    SLATrace("Unable to create threads \n");
    goto error;
  }
  if (rtspClient->sendRequest(RTSP_PLAY) == SLA_FAIL) {
    SLATrace("openURL send Play returned error \n");
    goto error;
  }

  if (rtspClient->receiveResponse() == SLA_FAIL) {
    SLATrace("receive response returned error \n");
    goto error;
  }

  *rtspClientPtr = rtspClient;
  return rtspClient->udpReceiver;

error:
  rtspClient->stopStreaming(true);
  delete rtspClient;
  rtspClient = NULL;
  *rtspClientPtr = NULL;
  return NULL;
}

