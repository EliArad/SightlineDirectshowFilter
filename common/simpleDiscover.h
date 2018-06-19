/*
 * Copyright (C)2007-2015 SightLine Applications Inc.
 * SightLine Applications Library of signal, vision, and speech processing
 *               http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/
#pragma once
#include "sltypes.h"
#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <ipmib.h>
#include <iphlpapi.h>

#define MAX_NICS 10
#define SL_MAGIC_NUMBER 0x51acd00d
#define SLFIP_TO_BOARD_PORT     14001 //!< UDP port for sending fip to the video processing board
#define SLFIP_FROM_BOARD_PORT   14002 //!< UDP port for receiving fip from the video processing board

/*! Contains socket releated information */
struct SLSocket {
  SOCKET socket;    //!< Actual socket handle
  u32 addr;         //!< address of...
  u16 port;         //!< Port number of open socket
  u32 sndrAddr;     //!< Address of client (where applicable)

  SLSocket() : socket(0), addr(0), port(0), sndrAddr(0) {
  };
};

typedef struct {
  u32 addr;
  u32 mask;
} SL_NIC;

static SL_NIC nic[MAX_NICS];
static int nNics = 0;
static u32 SLDiscoverPort = 51000;
static const char *SLDiscoverRequest  = "SLDISCOVER";
static u32 SLDiscoverRequestLen = 10;
static u32 SLDiscoverTimeoutMs = 2000;
static const char * STR_FORMAT_IPADDR 	= "%d.%d.%d.%d";

/// System information
typedef struct {
  u16 svc;            //!< Servces provided,
  u16 boardType;      //!< Board type, one of SL_BOARD_TYPE (since v0.2).
  char mac[20];       //!< Mac address string
  char ipaddr[16];    //!< Ip address string
  char netmaskstr[16];//!< Netmask string
  u32 netmask;        //!< Netmask in binary format
  char videoAddr[16]; //!< Video send to address
  char name[32];      //!< System name
  u16 videoPort;      //!< Video send to port number
  u16 comsPort;       //!< Input command port number (new in 2.17, assume 14001 if not present)
} SLSystemInfo;

/// List of available systems on the network
typedef struct {
  u32 num;            //!< Number of systems on the network
  SLSystemInfo *info; //!< Network information for each camera found
  // Private members
  u32 numAlloc;       //!< Private
} SLSystemList;

#define SL_DISCOVER_VERS_MINOR 4  // Change this if adding to the end of the struct
#define SL_DISCOVER_VERS_MAJOR 0  // Changing this breaks firmware upgrade

// NOTE:  Don't change this structure without changing the discover version
typedef struct {
  u32 magic;          //!< Magic identifier number
  u32 len;            //!< Discover message length
  u16 versMinor;      //!< Discover protocol minor version.
  u16 versMajor;      //!< Discover protocol major version.
  u16 type;           //!< Services provided
  u16 boardType;      //!< Board type, one of SL_BOARD_TYPE (since v0.2).
  char mac[20];       //!< MAC address of sender
  char ipaddr[16];    //!< IP address of sender
  char netmask[16];   //!< netmask assocated with ipaddr
  char name[32];      //!< Human Readable name of sender
  u16 videoPort;      //!< Port number where images are sent
  u16 comsPort;       //!< Input command port number (new in 2.17, assume 14001 if not present)
} SLDiscoverInfo;



#ifdef __cplusplus
extern "C" {
#endif

s32 SLADiscover(SLSystemList *sysList);

#ifdef __cplusplus
} // extern "C"
#endif