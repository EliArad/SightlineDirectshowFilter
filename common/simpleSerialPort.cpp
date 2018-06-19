/*
* Copyright (C)2007-2015 SightLine Applications Inc.
* SightLine Applications Library of signal, vision, and speech processing
*               http://www.sightlineapplications.com
*------------------------------------------------------------------------*/
#if USE_SERIAL_PORT // set this in the project proprties preprocessor tab in vidual studio.

#include "simpleSerialPort.h"

#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

///////////////////////////////////////////////////////////////////////////////
void CloseSerialPort(void * handle)
{
  CloseHandle(handle);
}
///////////////////////////////////////////////////////////////////////////////
void * OpenSerialPort(const char * portNumberStr)
{
  HANDLE hPort = INVALID_HANDLE_VALUE;
  u32 baud = 57600;

  hPort = CreateFile (portNumberStr, // Pointer to the name of the port
    GENERIC_READ | GENERIC_WRITE,
    0,            // Share mode
    NULL,         // Pointer to the security attribute
    OPEN_EXISTING,// How to open the serial port
    0,            // Port attributes
    NULL);        // Handle to port with attribute to copy

  if(hPort == INVALID_HANDLE_VALUE) {
    printf("failed to open serial %s, bail\n", portNumberStr);
    hPort = INVALID_HANDLE_VALUE;
    return hPort;
  }


  DCB PortDCB;
  // Initialize the DCBlength member. 
  PortDCB.DCBlength = sizeof (DCB); 
  // Get the default port setting information.
  GetCommState (hPort, &PortDCB);

  // Change the DCB structure settings.
  PortDCB.BaudRate = baud;                  // Current baud
  PortDCB.fBinary = TRUE;                   // Binary mode; no EOF check 
  PortDCB.fParity = TRUE;                   // Enable parity checking 
  PortDCB.fOutxCtsFlow = FALSE;             // No CTS output flow control 
  PortDCB.fOutxDsrFlow = FALSE;             // No DSR output flow control 
  PortDCB.fDtrControl = DTR_CONTROL_DISABLE;// DTR flow control type 
  PortDCB.fDsrSensitivity = FALSE;          // DSR sensitivity 
  PortDCB.fTXContinueOnXoff = TRUE;         // XOFF continues Tx 
  PortDCB.fOutX = FALSE;                    // No XON/XOFF out flow control 
  PortDCB.fInX = FALSE;                     // No XON/XOFF in flow control 
  PortDCB.fErrorChar = FALSE;               // Disable error replacement 
  PortDCB.fNull = FALSE;                    // Disable null stripping 
  PortDCB.fRtsControl = RTS_CONTROL_DISABLE;// RTS flow control 
  PortDCB.fAbortOnError = FALSE;            // Do not abort reads/writes on error
  PortDCB.ByteSize = 8;                     // Number of bits/byte, 4-8 
  PortDCB.Parity = NOPARITY;                // 0-4=no,odd,even,mark,space 
  PortDCB.StopBits = ONESTOPBIT;            // 0,1,2 = 1, 1.5, 2 

  if (!SetCommState (hPort, &PortDCB)) {
    DWORD dw = GetLastError();
    printf("failed to configure serial port\n");
    CloseSerialPort(hPort);
    hPort = INVALID_HANDLE_VALUE;
    return hPort;
  }

  // Retrieve the timeout parameters for all read and write operations
  COMMTIMEOUTS CommTimeouts;
  GetCommTimeouts(hPort, &CommTimeouts);
  CommTimeouts.ReadIntervalTimeout = MAXDWORD;  
  CommTimeouts.ReadTotalTimeoutMultiplier = 0;  
  CommTimeouts.ReadTotalTimeoutConstant = 2;    
  CommTimeouts.WriteTotalTimeoutMultiplier = 0;  
  CommTimeouts.WriteTotalTimeoutConstant = 100;    

  // Set the timeout parameters for all read and write operations
  if (!SetCommTimeouts (hPort, &CommTimeouts)) {
    printf("Could not set the timeout parameters\n");
    CloseSerialPort(hPort);
    hPort = INVALID_HANDLE_VALUE;
    return hPort;
  }

  FlushFileBuffers(hPort);
  return (void *)hPort;
}
///////////////////////////////////////////////////////////////////////////////
int WriteFIP(HANDLE handle, SLPacketType buffer, s32 dataLen) 
{
  DWORD bytesWritten = 0;
  if(WriteFile((HANDLE)handle, buffer, dataLen, &bytesWritten, NULL)){
    if( FlushFileBuffers(handle) == FALSE ) {
      printf("FlushFileBuffers fails.\n");
    }
    return bytesWritten;
  }
  return -1;
}
///////////////////////////////////////////////////////////////////////////////
int Read(HANDLE handle, SLPacketType buffer, u32 dataLen, s32 timeout) 
{
  DWORD bytesRead;
  u8 *data = (u8*)buffer;
  u32 total = 0;

  if(timeout == -1) {
    while(total < dataLen) {
      if(!ReadFile((HANDLE)handle, data+total, dataLen-total, &bytesRead, NULL))
        return -1;
      total+=bytesRead;
    }
    return total;
  } else {
    if(ReadFile((HANDLE)handle, data, dataLen, &bytesRead, NULL)){
      return bytesRead;
    }
    return -1;
  }
}
///////////////////////////////////////////////////////////////////////////////
s32 ReadFIP(HANDLE handle, SLPacketType data, u32 dataLen, s32 timeout)
{
  if(!data || !handle || (dataLen == 0)) 
    return -1;

  s32 n = 0;
  data[0] = data[1] = 0;

  //
  // Look for header 0x51 0xAC
  //
  n = Read(handle, data, 2, timeout);
  if(n!=2) {
    return -1;
  }

  if( (data[0] != HeaderByte1) || (data[1] != HeaderByte2) ) 
  {
    //  
    // two bytes don't match, look for a single byte matching (correct framing issues)
    //
    if( data[1] == HeaderByte1 )
    {
      data[0] = HeaderByte1;
      n = Read(handle, data+1, 1, timeout);
      if(n<0)
        return -1;
      if(data[1] != HeaderByte2)
        return 0;
    } else {
      return 0;
    }
  }

  //
  // Header matches, read length field
  //
  n = Read(handle, &data[2], 1, timeout);
  if(n<0)
    return -1;

  //
  // And based on length, read the rest of the data
  //
  n = Read(handle, &data[3], min(data[2],MAX_SLFIP_PAYLOAD), timeout);
  if( (n < 0) || (n != data[2]) )
    return -1;

  return n+3;
}

#endif // not compiled if we're not using serial port (using Ethernet instead)