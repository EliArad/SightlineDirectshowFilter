/*
 * Copyright (C)2007-2016 SightLine Applications Inc
 * SightLine Applications Library of signal, vision, and speech processing
 * http://www.sightlineapplications.com
 *------------------------------------------------------------------------*/


#define _CRTDBG_MAP_ALLOC
#include "SLAHal.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN  // Needed for winsock2, otherwise windows.h includes winsock1
#endif
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>


#ifdef _MSC_VER
#pragma warning( disable : 4995 4996 )
#endif


void SLATrace (const char *fmt, ...)
{
#ifndef _WIN32
  va_list Argp;
  va_start (Argp, fmt);
  vprintf(fmt, Argp);
  va_end(Argp);
#else
  char str[1024];
  va_list Argp;
  va_start (Argp, fmt);
  vsprintf(str, fmt, Argp);
  va_end(Argp);
  OutputDebugString(str);

#endif
}


///////////////////////////////////////////////////////////////////////////////
// For thread/stack debugging.
///////////////////////////////////////////////////////////////////////////////

#define DEBUG_THREAD_VERBOSE  0 //!< 1=enable verbose thread debugging.

#ifndef NDEBUG
#define DEBUG_STACK           1 //!< 1=enable runtime stack size check.
#else
#define DEBUG_STACK           0 
#endif

#define STACK_MARGIN  (1024 * 1) //!< If the available stack size is less than this, get assertion.


static CRITICAL_SECTION CriticalSection;
static bool CriticalSectionInitialized = false;

// Forward declarations.
static void SLAPrintStackUsage(bool printToStdout = false);
void SLACheckStackUsage();
struct SlThreadInfo;
static SlThreadInfo *GetFreeEntry();
static void SetHwBpToNewThread();


///////////////////////////////////////////////////////////////////////////////
// Get Current Stack Size for the Thread.
// Code from : http://stackoverflow.com/questions/1740888/determining-stack-space-with-visual-studio
///////////////////////////////////////////////////////////////////////////////
static size_t SLAGetStackUsage()
{
  MEMORY_BASIC_INFORMATION mbi;
  VirtualQuery(&mbi, &mbi, sizeof(mbi));
  // now mbi.AllocationBase = reserved stack memory base address

  VirtualQuery(mbi.AllocationBase, &mbi, sizeof(mbi));
  // now (mbi.BaseAddress, mbi.RegionSize) describe reserved (uncommitted) portion of the stack
  // skip it

  VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));
  // now (mbi.BaseAddress, mbi.RegionSize) describe the guard page
  // skip it

  VirtualQuery((char*)mbi.BaseAddress + mbi.RegionSize, &mbi, sizeof(mbi));
  // now (mbi.BaseAddress, mbi.RegionSize) describe the committed (i.e. accessed) portion of the stack

  return mbi.RegionSize;
}

struct SlThreadInfo {
  char      threadName[16];       //<! Name of the thread.
  DWORD     threadId;             //<! Thread ID.
  u32       initialStackSize;
  int       (*fn)(void *);        //<! Actual thread main function.
  void      *data;
  SLA_Task  hThread;
};

// Use global variable for ease of debugging. You can see the current threads in watch window.
SlThreadInfo SlaThreads[32];



// Dummy main funcion for threads.
static int SLThreadMain(void *p)
{
  SlThreadInfo *th = (SlThreadInfo*)p;
  int ret;
#if DEBUG_THREAD_VERBOSE
  SLTrace("Thread: %s starting...\n", th->threadName);
  ret = th->fn(th->data);
  SLTrace("Thread: %s terminating\n", th->threadName);
#else
  ret = th->fn(th->data);
#endif
#if DEBUG_STACK
  SLACheckStackUsage();
  SLAPrintStackUsage(true);
#else
  SLACheckStackUsage();
  //SLPrintStackUsage();
#endif
  // Release the entry.
  th->fn = 0;
  return ret;
}

static const SlThreadInfo *FindThreadEntry()
{
  DWORD id = GetCurrentThreadId();
  s32 i;
  for (i = 0; i < sizeof(SlaThreads) / sizeof(SlaThreads[0]); i++) {
    if (SlaThreads[i].threadId == id)
      break;
  }
  if (i == sizeof(SlaThreads) / sizeof(SlaThreads[0])) {
    // Assume this is the "main" thread.
    // Make sure "main" has not been added yet.
    for (i = 0; i < sizeof(SlaThreads) / sizeof(SlaThreads[0]); i++) {
      if (strcmp(SlaThreads[i].threadName, "main") == 0)
        return 0; // workaround for non-VideoTrack application, such as SLAPanel.
      //SLAssert(strcmp(SlaThreads[i].threadName, "main") != 0);
    }
    SlThreadInfo *th = GetFreeEntry();
    th->threadId = id;
    strcpy(th->threadName, "main");
    // We really don't know the actual stack size for the main thread, so use a tentative value.
    th->initialStackSize = SL_DEFAULT_STACK_SIZE + 1024*256; // todo: what is the right value? 
    return th;
  }
  return &SlaThreads[i];
}

// WARNING: This function may not work for Main thread.
static const DWORD SLGetThreadId(SLA_Task hThread)
{
  s32 i;
  for (i = 0; i < sizeof(SlaThreads) / sizeof(SlaThreads[0]); i++) {
    if (SlaThreads[i].hThread == hThread)
      break;
  }
  if (i == sizeof(SlaThreads) / sizeof(SlaThreads[0])) {
    // Assume this is the "main" thread.
    return 0;
  }
  return SlaThreads[i].threadId;
}

const char *SLAGetThreadName()
{
  const SlThreadInfo *th = FindThreadEntry();
  if(th)
    return th->threadName;
  return 0;
}

static __declspec( thread ) void *tlPtr = 0;

void *SLAGetThreadLocalPtr()
{
  return tlPtr;
}

void SLASetThreadLocalPtr(void *val)
{
  tlPtr = val;
}



// Check current stack usage.
void SLACheckStackUsage()
{
  if (!CriticalSectionInitialized) { // SLA2000-PC cames here immediately.
    InitializeCriticalSection(&CriticalSection);
    CriticalSectionInitialized = true;
  }

  size_t stackSize = SLAGetStackUsage();
  const SlThreadInfo *th = FindThreadEntry();
  if (th == 0)
    return;
  // don't check stack size if size==0 is initially specified (such as, udpReceiveTask as of 201601xx).
  if (th->initialStackSize == 0)
    return;

  // On windows, the stack size increases by page size or something like that, so should not use the MARGIN.
  if (stackSize > th->initialStackSize/*- STACK_MARGIN*/) {
    SLAPrintStackUsage(true);
    __asm { int 3 }; // Break into debugger.
  }
}
// Print current stack usage.
static void SLAPrintStackUsage(bool printToStdout)
{
  const SlThreadInfo *th = FindThreadEntry();
  if (th == 0)
    return;

  const size_t stackSize = SLAGetStackUsage();
  f32 usedRatio = (f32)stackSize / th->initialStackSize;

  // Print only when used ratio is high or low.
  if (usedRatio >= 0.8f || usedRatio <= 0.2f) {
    char tmps[128];
    sprintf_s(tmps, sizeof(tmps), "Thread: %-12s  stack used: %d bytes (initial %d)\n", th->threadName, stackSize, th->initialStackSize);
    if (printToStdout)
      SLATrace(tmps);
    OutputDebugString(tmps);	
  }
}

static SlThreadInfo *GetFreeEntry()
{
  s32 i;
  EnterCriticalSection(&CriticalSection); {
    for (i = 0; i < sizeof(SlaThreads) / sizeof(SlaThreads[0]); i++) {
      if (SlaThreads[i].fn == 0) {
        SlaThreads[i].fn = SLThreadMain; // reserve the slot by putting a dummy func.
        break;
      }
    }
  }
  LeaveCriticalSection(&CriticalSection);
  if (i == sizeof(SlaThreads) / sizeof(SlaThreads[0]))
     return 0;
  return &SlaThreads[i];
}
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// @see http://msdn.microsoft.com/en-us/library/xcb2z8hs%28VS.71%29.aspx
///////////////////////////////////////////////////////////////////////////////

typedef struct tagTHREADNAME_INFO
{
  DWORD dwType;     // must be 0x1000
  LPCSTR szName;    // pointer to name (in user addr space)
  DWORD dwThreadID; // thread ID (-1=caller thread)
  DWORD dwFlags;    // reserved for future use, must be zero
} THREADNAME_INFO;

static void SetThreadName( DWORD dwThreadID, LPCSTR szThreadName)
{
  THREADNAME_INFO info;
  {
    info.dwType = 0x1000;
    info.szName = szThreadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
  }
  __try
  {
    RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD), (DWORD*)&info );
  }
  __except (EXCEPTION_CONTINUE_EXECUTION)
  {
  }
}


static s32 SemCount = 0;

SLA_Sem SLASemCreate(int count, const char * name)
{
  const int maxCount = SL_SEM_MAX_COUNT;
  InterlockedIncrement((LONG*)&SemCount);
  return CreateSemaphore(NULL, count, maxCount, NULL);
}

void SLASemDestroy(SLA_Sem sem)
{
  InterlockedDecrement((LONG*)&SemCount);
  CloseHandle(sem);
}

bool SLASemPend(SLA_Sem sem, u32 howlong)
{
  return (WaitForSingleObject(sem, howlong) == WAIT_OBJECT_0);
}

void SLASemPost(SLA_Sem sem)
{
  LONG previousCount;
  ReleaseSemaphore(sem, 1, &previousCount);
}

///////////////////////////////////////////////////////////////////////////////
void SLASleep(u32 ms)
{
  Sleep(ms);
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void* SLACreateThread(int (*fn)(void *), u32 stacksize, const char *name, void *data, SLTaskPriority priority, bool joinable_ignored)
{
  void *hThread;

  // Initialize 
  if (!CriticalSectionInitialized) {
    InitializeCriticalSection(&CriticalSection);
    CriticalSectionInitialized = true;
  }

  // Get an empty slot from pool, and fill out the members.
  SlThreadInfo *th = GetFreeEntry();
  th->fn = fn;
  th->threadName[0] = 0;
  strncat(th->threadName, name, sizeof(th->threadName)-1);
  th->initialStackSize = stacksize;
  th->data = data;
  
  hThread = (void*) CreateThread(
                     NULL,       // default security attributes
                     0,          // default stack size
                     (LPTHREAD_START_ROUTINE) SLThreadMain,
                     th,       // no thread function arguments
                     0,          // default creation flags
                     &th->threadId); // receive thread identifier

  if( hThread == NULL ) {
    printf("CreateThread error: %d\n", GetLastError());
  }
  SetThreadName(th->threadId, name);
  th->hThread = hThread;
  SetThreadPriority(GetCurrentThread(), priority);
  return hThread;
}

void SLADestroyThread( void * handle )
{
#if 0  // Not confident in the following code.  -jsarao
  DWORD exitCode;
  do {
    GetExitCodeThread((HANDLE)handle,&exitCode);
    SLSleep(100);
  }while(exitCode!=STILL_ACTIVE);
  if(CloseHandle(handle)==false)
    SLTrace("ERROR: Thread could not be closed %d\n", GetLastError());
#endif
}


///////////////////////////////////////////////////////////////////////////////
// Mail Box
///////////////////////////////////////////////////////////////////////////////
typedef struct {
  HANDLE          hSemPut;        // Semaphore controlling queue "putting"
  HANDLE          hSemGet;        // Semaphore controlling queue "getting"
  CRITICAL_SECTION CritSect;      // Thread serialization
  int             nMax;           // Max objects allowed in queue
  u32             size;
  int             iNextPut;       // Array index of next "PutMsg"
  int             iNextGet;       // Array index of next "GetMsg"
  u8*             QueueObjects;   // Array of objects (ptr's to void)
  u32*            TypeClasses;    // Array of type classes corresponding to objects
} SLMBX_OBJ;

void* SLAMbxCreate(u32 size, u32 length, const char * name)
{
  SLMBX_OBJ *obj;

  obj = (SLMBX_OBJ *)SLAMalloc(sizeof(SLMBX_OBJ));

  obj->iNextPut = obj->iNextGet = 0;
  obj->nMax = length;
  obj->size = size;
  InitializeCriticalSection(&obj->CritSect);
  obj->hSemPut = CreateSemaphore(NULL, length, length, NULL);
  obj->hSemGet = CreateSemaphore(NULL, 0, length, NULL);
  obj->QueueObjects = (u8*)SLAMalloc(size * length);
  obj->TypeClasses = (u32*)SLACalloc(sizeof(u32)*length);
  return (void*)obj;
}

void SLAMbxDestroy(void* mbx)
{
  if (mbx == NULL) return;

  SLMBX_OBJ *obj = (SLMBX_OBJ *)mbx;
  SLAFree(obj->QueueObjects);
  SLAFree(obj->TypeClasses);
  DeleteCriticalSection(&obj->CritSect);
  CloseHandle(obj->hSemPut);
  CloseHandle(obj->hSemGet);
  SLAFree(obj);
  mbx = NULL;
}

bool SLAMbxPend(void* mbx, void* msg, u32 timeout)
{
  if(mbx == NULL) return false;
  int iSlot=0;
  LONG lPrevious=0;
  SLMBX_OBJ *obj = (SLMBX_OBJ *)mbx;
  DWORD rv;

#if DEBUG_STACK
  SLACheckStackUsage();
#endif
  // Wait for someone to put something on our queue, returns straight
  // away is there is already an object on the queue.
  //
  rv = WaitForSingleObject(obj->hSemGet, timeout);

  if(WAIT_OBJECT_0 == rv){
    EnterCriticalSection(&obj->CritSect);
    iSlot = obj->iNextGet++ % obj->nMax;
    memcpy(msg, obj->QueueObjects + obj->size*iSlot, obj->size);
    obj->TypeClasses[iSlot] = 0;
    LeaveCriticalSection(&obj->CritSect);

    // Release anyone waiting to put an object onto our queue as there
    // is now space available in the queue.
    //
    ReleaseSemaphore(obj->hSemPut, 1L, &lPrevious);
    return true;
  } else {
    return false;
  }
}

bool SLAMbxPost(void *mbx, void* msg, u32 timeout, u32 typeClass)
{
  if(mbx == NULL) return false;
  int iSlot;
  LONG lPrevious;
  SLMBX_OBJ *obj = (SLMBX_OBJ *)mbx;
  DWORD rv;

#if DEBUG_STACK
  SLACheckStackUsage();
#endif
  // Wait for someone to get something from our queue, returns straight
  // away is there is already an empty slot on the queue.
  //
  rv = WaitForSingleObject(obj->hSemPut, timeout);
  if(rv == 0xFFFFFFFF){
    DWORD err = GetLastError();
  }

  if(WAIT_OBJECT_0 == rv){
    EnterCriticalSection(&obj->CritSect);
    iSlot = obj->iNextPut++ % obj->nMax;
    memcpy(obj->QueueObjects + iSlot*obj->size, msg, obj->size);
    obj->TypeClasses[iSlot] = typeClass;
    LeaveCriticalSection(&obj->CritSect);

    // Release anyone waiting to remove an object from our queue as there
    // is now an object available to be removed.
    //
    ReleaseSemaphore(obj->hSemGet, 1L, &lPrevious);
    return true;
  } else {
    return false;
  }
}

s32 SLAMbxCount(SLA_Mbx mbx, u32 typeClass)
{
  s32 i, count = 0;
  // typeClass should not be 0
  if(typeClass == 0)
    return SLA_ERROR;

  SLMBX_OBJ *obj = (SLMBX_OBJ *)mbx;
  EnterCriticalSection(&obj->CritSect);
  for(i=0;i<obj->nMax;i++)
    count += (obj->TypeClasses[i]==typeClass);
  LeaveCriticalSection(&obj->CritSect);

  return count;
}



///////////////////////////////////////////////////////////////////////////////
void SLAGetMHzTime(u64 *time)
{
  f64 UsPeriod;

  LARGE_INTEGER tick, ticksPerSecond;
  QueryPerformanceFrequency(&ticksPerSecond);
  UsPeriod = 1000000.0/ticksPerSecond.QuadPart;

  QueryPerformanceCounter(&tick);
  *time = (u64)(UsPeriod * tick.QuadPart);
}



/// Sockets
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class SLAWinsockModule {
  WSADATA wsaData;
public:
  SLAWinsockModule();
  ~SLAWinsockModule();
};

SLAWinsockModule::SLAWinsockModule()
{
  s32 rv;
  // Initialize Winsock
  rv = WSAStartup(MAKEWORD(2,2), &wsaData);
}

SLAWinsockModule::~SLAWinsockModule()
{
  WSACleanup();
}

// Instantiate so that constructor is called at startup
static SLAWinsockModule globalWinsockModule;

s32 SLASockStartup(void *hTsk)
{
  // TODO: add sanity checking to ensure that this function is called
  // 1 time per thread
  return 0;
}

void SLASockCleanup()
{
  // TODO: is there a windows function that needs to be called per thread?
  // TODO: add sanity checking to ensure that this function is called
  // 1 time per thread
}

void SLASockClose(SLASocket *sock)
{
  closesocket(sock->socket);
}

u32 SLASockError()
{
  return WSAGetLastError();
}

s32 SLASockSetTimeoutMs(SLASocket *sock, s32 name, u32 timeoutms)
{
  return setsockopt(sock->socket, SOL_SOCKET, name, (char*)&timeoutms, sizeof(u32));
}


s32 SLASockServerBind(SLASocket *sock, s32 socktype, s32 protocol, u32 do_listen, const char *srcAddr)
{
  struct sockaddr_in serverAddrObj;

  SLAMemset(&serverAddrObj, 0, sizeof(struct sockaddr_in));
  serverAddrObj.sin_family      = AF_INET;
  serverAddrObj.sin_port        = htons(sock->port);
  if(srcAddr==NULL)
    serverAddrObj.sin_addr.s_addr = INADDR_ANY;
  else
    serverAddrObj.sin_addr.s_addr = inet_addr(srcAddr);

  if( (!sock->socket) || (sock->socket == INVALID_SOCKET) ) {
   sock->socket = socket(AF_INET, socktype, protocol);
  } else {
    SLATrace("Socket already bound.  Need to shutdown.\n");\
  }
 
  if (sock->socket == INVALID_SOCKET) {
    SLATrace("Error at socket(): %d\n", SLASockError());
    return -1;
  }

  int rcvBuffSizeOption = 1024*1024;  // see MAX_READ_BUFFER_SIZE - jsarao
  ULONG reuseAddr = 1;
  setsockopt(sock->socket, SOL_SOCKET, SO_RCVBUF, (char*)&rcvBuffSizeOption, sizeof(rcvBuffSizeOption));
  setsockopt(sock->socket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr));

  s32 rv = bind(sock->socket, (struct sockaddr *)&serverAddrObj, sizeof(struct sockaddr_in));

  if(rv) {
    SLASockClose(sock);
    sock->socket = INVALID_SOCKET;
    SLATrace("SLSockServerBind failed to bind to port %d, %d\n", sock->port, rv);
    return -1;
  }

  if(do_listen) {
    // Put the socket into listen mode
    if(listen(sock->socket, 1)) {
      SLASockClose(sock);
      sock->socket = INVALID_SOCKET;
      return -1;
    }
  }
  return 0;
}

s32 SLASockClientConnect(SLASocket *sock, s32 timeout)
{
  if (timeout < 0 && timeout != SL_FOREVER) {
    SLATrace("Invalid timeout value passed in SLASockClientConnect \n");
    return -1;
  }

  struct sockaddr_in clientAddrObj;

  // Setup the address object
  SLAMemset(&clientAddrObj, 0, sizeof(struct sockaddr_in));
  clientAddrObj.sin_family = AF_INET;
  clientAddrObj.sin_port = htons(sock->port);
  clientAddrObj.sin_addr.s_addr = sock->addr;

  // Create a SOCKET for connecting to server
  if (!sock->socket || sock->socket == INVALID_SOCKET) {
    sock->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  }
  if (sock->socket == INVALID_SOCKET) {
    SLATrace("Error at socket(): %ld\n", SLASockError());
    return -1;
  }

  s32 rv = -1;
  if (timeout == -1) {  // no timeout
    rv = connect(sock->socket, (struct sockaddr *)&clientAddrObj, sizeof(struct sockaddr));
  }
  else {
    // set to non-blocking mode to handle timeout.
    u_long val = 1;
    ioctlsocket(sock->socket, FIONBIO, &val);

    // Try to connect...
    rv = connect(sock->socket, (struct sockaddr *)&clientAddrObj, sizeof(struct sockaddr));

    if (rv != SOCKET_ERROR) {
      // Success!
    }
    else {
      s32 err = WSAGetLastError();
      if (err != WSAEWOULDBLOCK) {		//check if error was WSAEWOULDBLOCK, where we'll wait
        goto leave; // error.
      }
      fd_set Write, Err;
      FD_ZERO(&Write);
      FD_ZERO(&Err);
      FD_SET(sock->socket, &Write);
      FD_SET(sock->socket, &Err);

      timeval tv;
      tv.tv_sec = timeout / 1000;
      tv.tv_usec = (timeout % 1000) * 1000;

      // Wait until times out.
      rv = select(0, NULL, &Write, &Err, &tv);
      if (rv == 0) { // timed out.
        rv = -1;
        goto leave; // error.
      }
      if (FD_ISSET(sock->socket, &Err)) {       // error.
        rv = -1;
        goto leave; // error.
      }
      if (FD_ISSET(sock->socket, &Write)) {     // Success! 
        rv = 0; // Indicate success.
      }
    }
  leave:
    if (timeout != (u32)-1) { // Reset to blocking mode.
      u_long val = 0;
      ioctlsocket(sock->socket, FIONBIO, &val);
    }
  }
  return rv;
}

void SLASockDisconnect(SLASocket *sock)
{
  int iResult;//, total=0;
  
  if( (!sock->socket) || (sock->socket==INVALID_SOCKET)) return;
  
  // shutdown the connection since no more data will be sent
  iResult = shutdown(sock->socket, 0x1/*SD_SEND(pc) SHUT_WR(ti)*/);

  if (iResult == SOCKET_ERROR) {
      SLATrace("shutdown failed: %d\n", SLASockError());
      SLASockClose(sock);
      sock->socket = INVALID_SOCKET;
      return;
  }
  // cleanup
  SLASockClose(sock);
  sock->socket = INVALID_SOCKET;
}

s32 SLASockUDPInit(SLASocket *sock, u32 addr, u16 port)
{
  sock->addr = addr;
  sock->port = port;
  int how;
#if WIN32
  how = SD_RECEIVE;
#else
  how = SHUT_RDWR;
#endif
  if(sock->socket && sock->socket!=INVALID_SOCKET) {
    shutdown(sock->socket, how);
  }
  if(!sock->socket || sock->socket==INVALID_SOCKET)
    sock->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  return sock->socket != INVALID_SOCKET;
}

s32 SLASockUDPInit(SLASocket *sock, const char *addr, u16 port)
{
  return SLASockUDPInit(sock, inet_addr(addr), port);
}

s32 SLASockSendTo(SLASocket *sock, char *buf, s32 len)
{
  s32 rv;
  struct sockaddr_in saddr;
  saddr.sin_addr.s_addr = sock->addr;
  saddr.sin_port = htons(sock->port);
  saddr.sin_family = AF_INET;
  rv = sendto(sock->socket, buf, len, 0, (sockaddr*)&saddr, sizeof(saddr));
  if(rv<0){
//    SLTrace("sendto(0x%08x, %d) returned %d; error = %d\n", buf, len, rv, SLSockError());
  } else {
//    SLTrace("sendto(0x%08x, %d) passed\n", buf, len);
  }
  return rv;
}

s32 SLASockRecvFrom(SLASocket *sock, char *buf, s32 len, s32 timeoutms)
{
  s32 rv = -1;
  struct sockaddr_in saddr;
  SLAMemset( &saddr, 0, sizeof(saddr) );

  s32 saddrlen = sizeof(saddr);

  saddr.sin_addr.s_addr = sock->addr;
  saddr.sin_port = htons(sock->port);
  saddr.sin_family = AF_INET;

  if(timeoutms < 0) {
    s32 error = WSAETIMEDOUT;
    s16 retryCount = 1;
    int test = 0;
    do {
      rv = recvfrom(sock->socket, buf, len, 0, (sockaddr*)&saddr, &saddrlen);
      if(rv==-1){
        error = WSAGetLastError();
        if(error != WSAETIMEDOUT){
          test = 1;
          SLATrace("recvfrom error = %d\n", error);
        }
      }
    } while (rv<=0 && error==WSAETIMEDOUT && (--retryCount));
  } else {
    timeval tv;
    fd_set fds;

    FD_ZERO(&fds);
    FD_SET(sock->socket, &fds);

    tv.tv_usec = timeoutms*1000;
    tv.tv_sec = 0;

    int n = select(sock->socket, &fds, NULL, NULL, &tv);
    if(n <= 0) {
      rv = -1;
    } else {
      rv = recvfrom(sock->socket, buf, len, 0, (sockaddr *)&saddr, &saddrlen);
    }
  }

  sock->sndrAddr = saddr.sin_addr.s_addr;
 
  return rv;
}



s32 SLASockJoinSourceGroup(SLASocket *sock, u32 grpaddr, u32 srcaddr, u32 iaddr)
{
   struct ip_mreq_source imr; 
   
   imr.imr_multiaddr.s_addr  = grpaddr; 
   imr.imr_sourceaddr.s_addr = srcaddr; 
   imr.imr_interface.s_addr  = iaddr;
   return setsockopt(sock->socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *)&imr, sizeof(imr));

/* 
  // NOTE: default TTL is 1.  If packets area expected to be routed, then increasing
  // the TTL will be necessary.  (http://support.microsoft.com/kb/131978)
  int ttl = 7 ; // Arbitrary TTL value.
  setsockopt(sock->socket, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl))
*/

}

s32 SLASockLeaveSourceGroup(SLASocket *sock, u32 grpaddr, 
                            u32 srcaddr, u32 iaddr)
{
   struct ip_mreq_source imr;

   imr.imr_multiaddr.s_addr  = grpaddr;
   imr.imr_sourceaddr.s_addr = srcaddr;
   imr.imr_interface.s_addr  = INADDR_ANY;
   return setsockopt(sock->socket, IPPROTO_IP, IP_DROP_SOURCE_MEMBERSHIP, (const char*)&imr, sizeof(imr));
}

s32 SLASockGetHostInfo(char *name, u32 namesize, 
                       char *mac, u32 macsize, 
                       char *ipaddr, u32 ipaddrsize, 
                       char *netmaskstr, u32 netmasksize, u32 *pnetmask, char * iName)
{
  // Get local host name
  if(name && namesize && gethostname(name, namesize)) {
    // Error handling -> call 'WSAGetLastError()'
    return -1;
  }

  IP_ADAPTER_INFO *pai, *bestPai;
  IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information
                                         // for up to 16 NICs
  DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

  DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
    AdapterInfo,                 // [out] buffer to receive data
    &dwBufLen);                  // [in] size of receive data buffer
  
  if(dwStatus != ERROR_SUCCESS) return 0;  // Verify return value is
                                      // valid, no buffer overflow

  pai = &AdapterInfo[0];
  bestPai = 0;

  // Find the first ethernet connection
  // if no ethernet connection, find first wireless connection
  // if neither is defined, find first connection
  // Always ignore loopback unless it's the only connection
  // Skip virtual network connections, identified as suggested by
  // http://stackoverflow.com/questions/3062594/differentiate-vmware-network-adapter-from-physical-network-adapters-or-detect
  for(int i=0; i<16 && pai; i++, pai = pai->Next){
    _strupr(pai->Description);
    if( !strstr(pai->Description, "VMWARE") && !strstr(pai->Description, "VIRTUAL")){
      if( strncmp("0.0.0.0", pai->IpAddressList.IpAddress.String, 8)!=0){
        switch(pai->Type){
          case MIB_IF_TYPE_LOOPBACK:
            break;
          case MIB_IF_TYPE_ETHERNET:
            if(!bestPai || bestPai->Type != MIB_IF_TYPE_ETHERNET){
              bestPai = pai;
            }
            break;
          case IF_TYPE_IEEE80211:
            if(!bestPai || bestPai->Type != MIB_IF_TYPE_ETHERNET && bestPai->Type != IF_TYPE_IEEE80211){
              bestPai = pai;
            }
            break;
          case MIB_IF_TYPE_OTHER:
          default:
            // Select any type of connection over loopback
            if(!bestPai || bestPai->Type == MIB_IF_TYPE_LOOPBACK){
              bestPai = pai;
            }
            break;
        }
      }
    }
  }

  // Error condition??
  if(!bestPai)
    return 0;

  if(mac && macsize){
    sprintf(mac, MAC_STR_FORMAT, 
      bestPai->Address[0],
      bestPai->Address[1],
      bestPai->Address[2],
      bestPai->Address[3],
      bestPai->Address[4],
      bestPai->Address[5] );
  }

  if(ipaddr && ipaddrsize)
    strncpy(ipaddr, bestPai->IpAddressList.IpAddress.String, ipaddrsize);

  // Just a default in case all the winsock calls fail to return real system netmask
  if( netmasksize>0 && netmaskstr)
    strncpy(netmaskstr, bestPai->IpAddressList.IpMask.String, netmasksize);

  unsigned int a,b,c,d;
  sscanf(bestPai->IpAddressList.IpMask.String, "%d.%d.%d.%d", &a, &b, &c, &d);
  if(pnetmask)
    *pnetmask = (a) | (b<<8) | (c<<16) | (d<<24);

  return 0;
}



///////////////////////////////////////////////////////////////////////////////
//
//                            SLSOCKUDP
//
///////////////////////////////////////////////////////////////////////////////
SLASockUDP::SLASockUDP() : Connected(0)
{
  m_bufidx = 0;
  m_buflen = 0;
  SLAMemset(&ReadSock, 0, sizeof(ReadSock));
  SLAMemset(&WriteSock, 0, sizeof(WriteSock));
}

SLASockUDP::~SLASockUDP()
{
  //Sock.Close();
}

////////////////////////////////////////////////////////////////////////////////
SLStatus SLASockUDP::Initialize(u32 addr, u32 writePortNumber, u32 readPortNumber, bool broadcast)
{

  if(writePortNumber) {
    if(!SLASockUDPInit(&WriteSock, addr, writePortNumber))
      return SLA_ERROR;
    if(broadcast) {
      u32 optval = TRUE;
      if(setsockopt(WriteSock.socket, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(u32)))
        return SLA_ERROR;
    }
  }
  if(readPortNumber) {
    ReadSock.addr = 0;
    ReadSock.port = readPortNumber;
    if(SLASockServerBind(&ReadSock, SOCK_DGRAM, IPPROTO_UDP, 0))
      return SLA_ERROR;
  }
  return SLA_SUCCESS;
}

s32 SLASockUDP::Close()
{
  SLASockDisconnect(&ReadSock);
  SLASockDisconnect(&WriteSock);

  //SLMemset(&ReadSock, 0, sizeof(Sock));
  //SLMemset(&WriteSock, 0, sizeof(Sock));


  return SLA_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
SLStatus SLASockUDP::Initialize(const char *addr, u32 writePortNumber, u32 readPortNumber, bool broadcast)
{
  u32 iaddr = addr ? inet_addr(addr) : 0;
  return Initialize(iaddr, writePortNumber, readPortNumber, broadcast);
}

////////////////////////////////////////////////////////////////////////////////
s32 SLASockUDP::Read(void *data, u32 len, s32 timeout)
{
  if (m_buflen - m_bufidx<(s32)len) {
    for (s32 i = 0; i<m_buflen - m_bufidx; i++)
      m_buf[i] = m_buf[m_bufidx + i];
    m_buflen = m_buflen - m_bufidx;
    m_bufidx = 0;
    s32 rv;
    do {
      rv = SLASockRecvFrom(&ReadSock, &m_buf[m_buflen], sizeof(m_buf) - m_buflen);
    } while(rv==0);
    if(rv>0)
      m_buflen = m_buflen + rv;
    if(rv<0)
      return -1;
  }
  if (m_buflen - m_bufidx >= (s32)len) {
    SLAMemcpy(data, &m_buf[m_bufidx], len);
    m_bufidx += len;
    return len;
  } else {
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
s32 SLASockUDP::Write(const void *data, u32 len)
{
  s32 rv = SLASockSendTo(&WriteSock, (char*)data, len);
  return rv;
}



typedef struct {
  HANDLE hPort;
  bool readWaiting;
  OVERLAPPED osReader;
  OVERLAPPED osWriter;
} SLRs232Context;

SLARs232::SLARs232() : port(0), handle(0), enableReadWaiting(true), isAsyncronous(false)
{
}



s32 SLARs232::Open(const char * portNumberStr, u32 baud, u32 dataBits, u32 stopBits, u32 parity, u32 flags)
{
  // To open 2 digit com ports (eg. COM10), the name needs to be "\\.\COM10"
  // works for 1 digit com ports as well.  http://support.microsoft.com/kb/115831
  //char name[80] = { 0 };
  char name[80] = { 0 };
  sprintf(name, "\\\\.\\%s", portNumberStr);

  handle = malloc(sizeof(SLRs232Context));
  SLRs232Context *cntx = (SLRs232Context*)handle;
  memset(cntx, 0, sizeof(SLRs232Context));
  DWORD dwFlagsAndAttributes = FILE_FLAG_OVERLAPPED;

  HANDLE hPort = CreateFile(name, // Pointer to the name of the port
                            GENERIC_READ | GENERIC_WRITE,
                            0,            // Share mode
                            NULL,         // Pointer to the security attribute
                            OPEN_EXISTING,// How to open the serial port
                            dwFlagsAndAttributes,  // Port attributes
                            NULL);        // Handle to port with attribute to copy
  if (hPort == INVALID_HANDLE_VALUE) {
    SLATrace("failed to open serial %s, bail\n", portNumberStr);
    return -1;
  }
  isAsyncronous = (dwFlagsAndAttributes & FILE_FLAG_OVERLAPPED) ? true : false;

  u32 winParity;
  // MARKPARITY, SPACEPARITY not supported
  switch (parity) {
    case SLA_UART_PARITY_NONE:
      winParity = NOPARITY;
      break;
    case SLA_UART_PARITY_EVEN:
      winParity = EVENPARITY;
      break;
    case SLA_UART_PARITY_ODD:
      winParity = ODDPARITY;
      break;
    default:
      CloseHandle(hPort);
      return -1;
  }
  u32 winStopBits;
  // TODO: 1.5 stop bits not supported (ONE5STOPBITS)
  switch (stopBits) {
    case 1:
      winStopBits = ONESTOPBIT;
      break;
    case 2:
      winStopBits = TWOSTOPBITS;
      break;
  }

  u32 winRTS = RTS_CONTROL_DISABLE;

  if ((flags & SLA_UART_ENABLE_CTS) && (flags & SLA_UART_ENABLE_RTS)) {
    winRTS = RTS_CONTROL_ENABLE;
  }

  u32 winDTR = DTR_CONTROL_DISABLE;
  if (flags & SLA_UART_ENABLE_DTR) {
    winDTR = DTR_CONTROL_ENABLE;
  }

  DCB PortDCB;
  // Initialize the DCBlength member. 
  PortDCB.DCBlength = sizeof(DCB);
  // Get the default port setting information.
  GetCommState(hPort, &PortDCB);
  // Change the DCB structure settings.
  PortDCB.BaudRate = baud;              // Current baud
  PortDCB.fBinary = TRUE;               // Binary mode; no EOF check 
  PortDCB.fParity = TRUE;               // Enable parity checking 
  PortDCB.fOutxCtsFlow = FALSE;         // No CTS output flow control 
  PortDCB.fOutxDsrFlow = FALSE;         // No DSR output flow control 
//  PortDCB.fDtrControl = winDTR;         // DTR flow control type 
  PortDCB.fDtrControl = DTR_CONTROL_DISABLE;// DTR flow control type 
  PortDCB.fDsrSensitivity = FALSE;      // DSR sensitivity 
  PortDCB.fTXContinueOnXoff = TRUE;     // XOFF continues Tx 
  PortDCB.fOutX = FALSE;                // No XON/XOFF out flow control 
  PortDCB.fInX = FALSE;                 // No XON/XOFF in flow control 
  PortDCB.fErrorChar = FALSE;           // Disable error replacement 
  PortDCB.fNull = FALSE;                // Disable null stripping 
//  PortDCB.fRtsControl = winRTS;         // RTS flow control 
  PortDCB.fRtsControl = RTS_CONTROL_DISABLE;// RTS flow control 
  PortDCB.fAbortOnError = FALSE;        // Do not abort reads/writes on error
  PortDCB.ByteSize = dataBits;          // Number of bits/byte, 4-8 
  PortDCB.Parity = winParity;           // 0-4=no,odd,even,mark,space 
  PortDCB.StopBits = winStopBits;       // 0,1,2 = 1, 1.5, 2 
  // Configure the port according to the specifications of the DCB structure.
  if (!SetCommState(hPort, &PortDCB)) {
    DWORD dw = GetLastError();
    printf("failed to configure serial port\n");
    CloseHandle(hPort);
    return -1;
  }

  cntx->osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  cntx->osWriter.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
  cntx->hPort = hPort;
  // Retrieve the timeout parameters for all read and write operations
  COMMTIMEOUTS CommTimeouts;
  GetCommTimeouts(hPort, &CommTimeouts);
  CommTimeouts.ReadIntervalTimeout        = 5; // Char-to-char interval timeout is 5ms.
  CommTimeouts.ReadTotalTimeoutMultiplier = 0; // Don't use the total timeout.
  CommTimeouts.ReadTotalTimeoutConstant   = 0; // Don't use the total timeout.
  CommTimeouts.WriteTotalTimeoutMultiplier = 0;
  CommTimeouts.WriteTotalTimeoutConstant = 100;

  // Set the timeout parameters for all read and write operations
  if (!SetCommTimeouts(hPort, &CommTimeouts)) {
    printf("Could not set the timeout parameters\n");
    CloseHandle(hPort);
    hPort = INVALID_HANDLE_VALUE;
    return -1;
  }

  FlushFileBuffers(hPort);
  return 0;
}

s32 SLARs232::Open(u32 portNumber, u32 baud, u32 dataBits, u32 stopBits, u32 parity, u32 flags)
{
  char port[10] = { 0 };
  sprintf(port, "\\\\.\\COM%d", portNumber);
  return this->Open(port, baud, dataBits, stopBits, parity, flags);
}

s32 SLARs232::Read(void *_data, u32 dataLen, s32 timeout)
{
  DWORD bytesRead;
  u8 *data = (u8*)_data;
  u32 total = 0;

  SLRs232Context *ctxt = (SLRs232Context*)handle;
  if (!ctxt || !ctxt->hPort)
    return -1;

  // Based on: https://msdn.microsoft.com/en-us/library/ff802693.aspx
  if (!ctxt->readWaiting) {
    DWORD *pBytesToRead = isAsyncronous ? 0 : &bytesRead;
    if (ReadFile(ctxt->hPort, data, dataLen, pBytesToRead, &ctxt->osReader)) {
      if (isAsyncronous)
        return dataLen; // In Asynchronous mode, it must have read the number of bytes specified.
      return bytesRead; // In Synchronous mode, the number of bytes read is returned.
    }
    if (GetLastError() == ERROR_IO_PENDING)
      if (enableReadWaiting)
        ctxt->readWaiting = true;
  }
  if (ctxt->readWaiting) { // I/O pending.
    DWORD dwRes = WaitForSingleObject(ctxt->osReader.hEvent, timeout);
    switch (dwRes){
    case WAIT_OBJECT_0:
      if (!GetOverlappedResult(ctxt->hPort, &ctxt->osReader, &bytesRead, FALSE)) {
        bytesRead = -1; // error.
      }
      ctxt->readWaiting = false;
      return bytesRead;
      break;

    case WAIT_TIMEOUT:
      return 0;  // no bytes available, readWaiting remains true
      break;

    default: // This indicates a problem with the OVERLAPPED structure's event handle.
      ctxt->readWaiting = false;
      break;
    }
  }
  return -1;
}

// clear pending reads
s32 SLARs232::ClearPending()
{
  SLRs232Context *ctxt = (SLRs232Context*)handle;
  if(!ctxt || !ctxt->hPort)
    return -1;
 ctxt->readWaiting = false;
 return 0;
}

s32 SLARs232::EnableReadWaiting(bool enable)
{
  enableReadWaiting = enable;
  return 0;
}

s32 SLARs232::Write(const void *data, u32 dataLen)
{
  SLRs232Context *ctxt = (SLRs232Context*)handle;
  if(!ctxt || !ctxt->hPort)
    return -1;

  DWORD totalBytes = 0;
  DWORD bytesWritten = 0;
  while(1){
    if(WriteFile(ctxt->hPort,(u8*)data+totalBytes,dataLen-totalBytes,&bytesWritten,&ctxt->osWriter)){
      return bytesWritten;
    } else {
      if(GetLastError() != ERROR_IO_PENDING)
        return -1;
      DWORD dwres = WaitForSingleObject(ctxt->osWriter.hEvent, INFINITE);
      switch(dwres){
        case WAIT_OBJECT_0:
          if(GetOverlappedResult(ctxt->hPort, &ctxt->osWriter, &bytesWritten, FALSE)){
            totalBytes += bytesWritten;
            if(totalBytes == dataLen)
              return bytesWritten;
          }
        default:
          return -1;
      }
    }
  }
}

s32 SLARs232::Close()
{
  s32 rv = 0;
  if (handle != NULL) {

    SLRs232Context *ctxt = (SLRs232Context*)handle;
    if(ctxt->hPort != NULL) {
      HANDLE hPort = ctxt->hPort;
      ctxt->hPort = NULL;

      rv = -1;
      s32 shouldWait;
      s32 nHandles = 0;
      HANDLE handles[2];

      if(ctxt->osReader.Internal == STATUS_PENDING)
        handles[nHandles++] = ctxt->osReader.hEvent;
      if(ctxt->osWriter.Internal == STATUS_PENDING)
        handles[nHandles++] = ctxt->osWriter.hEvent;

      shouldWait = CancelIo(hPort);

      if(shouldWait)
        WaitForMultipleObjects(nHandles, handles, TRUE, 1000);

      CloseHandle(ctxt->osReader.hEvent);
      CloseHandle(ctxt->osWriter.hEvent);
      if(CloseHandle(hPort))
        rv = 0;
    }
    free(handle);
    handle = NULL;
  }
  return rv;
}

