/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#if !USE_SERIAL_PORT // set this in the project proprties preprocessor tab in visual studio.

#include "simpleSocket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma warning( disable : 4995 4996 )

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // Needed for winsock2, otherwise windows.h includes winsock1
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>


WSADATA wsaData;
#pragma comment(lib, "Ws2_32.lib")
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

struct MySocket{
  SOCKET socket;        //!< Actual socket handle
  u32 addr;             //!< address of...
  u16 port;             //!< Port number of open socket
  u32 sndrAddr;         //!< Address of client (where applicable)
};

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

void ClosePort(void * handle)
{
  MySocket * pSock = (MySocket *)handle;
#ifdef _WIN32
  closesocket(pSock->socket);
#else
  close(pSock->handle);
#endif
  delete pSock;
}

void * OpenWritePort(const char * ipAddress, int portNumber)
{
#if _WIN32
  WSAStartup(MAKEWORD(2,2), &wsaData);
#endif

  MySocket * pSock = new(MySocket);
  pSock->addr = inet_addr(ipAddress);
  pSock->port = portNumber;

  if(pSock->socket && pSock->socket!=INVALID_SOCKET) {
    shutdown(pSock->socket, SD_RECEIVE);
    pSock->socket=INVALID_SOCKET;
  }
  if(!pSock->socket || pSock->socket==INVALID_SOCKET) {
    pSock->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  }

  struct sockaddr_in addrObj;

  memset(&addrObj, 0, sizeof(addrObj));
  addrObj.sin_family      = AF_INET;
  addrObj.sin_port        = 0;
  addrObj.sin_addr.s_addr = INADDR_ANY;
  s32 rv = bind(pSock->socket, (struct sockaddr *)&addrObj, sizeof(addrObj));

  return pSock;
}

void * OpenListenPort(int portNumber)
{
  MySocket * pSock = new(MySocket);
  pSock->addr = 0;
  pSock->port = portNumber;
  //
  struct sockaddr_in serverAddrObj;
  memset(&serverAddrObj, 0, sizeof(struct sockaddr_in));
  serverAddrObj.sin_family      = AF_INET;
  serverAddrObj.sin_port        = htons(portNumber);
  serverAddrObj.sin_addr.s_addr = INADDR_ANY;
  
  pSock->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  // buffer:
  int rcvBuffSizeOption = 300*1024;  
  ULONG reuseAddr = 1;
  setsockopt(pSock->socket, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBuffSizeOption, sizeof(rcvBuffSizeOption));
  setsockopt(pSock->socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));

  if( bind(pSock->socket, (struct sockaddr *)&serverAddrObj, sizeof(struct sockaddr_in)) == 0)
    return pSock;

  ClosePort((void*)pSock);

  printf("Failed to LISTEN to on port %d\n", portNumber);
  return 0;
}
/*! Send bytes
 * @param buffer data to be sent
 * @param dataLen number of bytes to send
 * @return Number of bytes sent
 */
int WriteFIP(void * handle, SLPacketType buffer, s32 dataLen)
{
  MySocket * pSock = (MySocket *)handle;

  struct sockaddr_in saddr;

  saddr.sin_addr.s_addr = pSock->addr;
  saddr.sin_port = htons(pSock->port);
  saddr.sin_family = AF_INET;

  int retval = sendto(pSock->socket, (const char *)buffer, dataLen, 0, (sockaddr*)&saddr, sizeof(saddr));

  return retval;
}

/*! Receive bytes
 * @param buffer place where received data is written
 * @param dataLen maximum number of bytes to expect
 * @return Number of bytes received
 */
int ReadFIP(void * handle, SLPacketType buffer, u32 dataLen, s32 timeout)
{
  MySocket * pSock = (MySocket *)handle;

  s32 retval = -1;
  struct sockaddr_in saddr;
  memset( &saddr, 0, sizeof(saddr) );
  s32 saddrlen = sizeof(saddr);

  saddr.sin_addr.s_addr = pSock->addr;
  saddr.sin_port = htons(pSock->port);
  saddr.sin_family = AF_INET;

  s32 error = WSAETIMEDOUT;
  s16 retryCount = 1;
  int test = 0;
  do {

    retval = recvfrom(pSock->socket, (char *)buffer, dataLen, 0, (sockaddr*)&saddr, &saddrlen);

  } while (retval<=0 && error==WSAETIMEDOUT && (--retryCount));

  return retval;
}

#endif // not using serial, using ethernet