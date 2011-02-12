/***************************************************************************
 *   Copyright (C) 2008 by Blake Leverett                                  *
 *   bleverett@gmail.com
 *                                                                         *
 *   FUNterm is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/
/*
  CVS info:
  $Id: serial.c,v 1.2 2010/04/01 15:19:45 blakelev Exp $
  $Revision: 1.2 $
  $Date: 2010/04/01 15:19:45 $
 */
/**
  @file serial.c This file implements a serial interface to Win32.
  @defgroup serial Serial Interface

  @section intro Introduction
  
  This file implements a serial port connection in Win32.  It uses direct calls to the OS,
  which makes it quite simple to understand and maintain.
  
  Beware that the Win32 serial interface is somewhat broken, and that some of the code may seem
  unnecessary.  In particular, GetCommState() does not initialize the DCB structure correctly.
  So be careful to not delete any "unnecessary" code.
  
  @section using Using serial.c as a General-Purpose Serial Port
  
  You can use this file without the terminal application as a serial port driver under windows.
  The interface is simple to use, involving just a few basic functions, in this order:
  - OpenPort()
  - PutSerialChar()
  - SerialIsChar()
  - SerialGetChar()
  - CloseSerialPort()
  
  If you create a Win32 program using this interface, it will be easier to simply pass the handle
  of your main window to OpenPort, and then your main window will receive MESS_SERIAL messages
  every time incoming characters are availble.  All you have to do is handle the MESS_SERIAL
  message.  See the FUNterm application for details.  As a Win32 app, you would not need to call
  SerialGetChar() directly.

  Note that this implementation only allows one open serial port at a time.  Having multiple serial
  ports open at once is left as an excerise to the reader.
  @{
 */
#include "serial.h"

// Functions:
HANDLE StartCommThread(void);
DWORD WINAPI ThreadProc(void *p);

// Variables:
HANDLE SerialPort=NULL;  ///< Handle of SerialPort itself.
HANDLE Thread;           ///< Handle to the Rx thread.
HWND handle=NULL;        ///< Handle to window that receives MESS_SERIAL messages.
int StopThread=0;        ///< Flag: set to non-zero to stop the Rx thread.
int FlowControl=0;       ///< Flag: is hardware flow-control active?


/**
   Displays any error messages. Includes descriptive text, not just an error number.
 */
void ShowLastError(void)
{
  LPVOID lpMsgBuf;
  int err = GetLastError();
  FormatMessage(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                err,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                (LPTSTR) &lpMsgBuf,
                0,
                NULL
                );
  
  // Display the string.
  MessageBox( NULL, lpMsgBuf, "Error", MB_OK|MB_ICONINFORMATION );
  
  // Free the buffer.
  LocalFree( lpMsgBuf );
}


/**
   Opens the COMM Port.
   @param port Port number.  COMM1 = 1, COMM2 = 2, etc.
   @param baud Baud rate, in BPS.  Commonly 9600, 38400, etc.
   @param HwFc Set to non-zero to use hardware flow-control, or zero
   for no flow control.
   @param hwnd Window handle to recieve MESS_SERIAL messages.  If set to NULL,
   you can still get characters using SerialGetChar().
   @return TRUE if port was opened, FALSE if opening failed.
 */
BOOL OpenPort(int port,int baud,int HwFc, HWND hwnd)
{
  HANDLE Comport;
  DCB myDCB;
  COMMTIMEOUTS CTout;
  char str[100];
  
  FlowControl = HwFc;
  
  // Open the serial port
  if (port > 9)
    sprintf(str,"\\\\.\\COM%d",port);
  else
    sprintf(str,"COM%d",port);
  Comport = CreateFile(str,GENERIC_READ|GENERIC_WRITE,0,
                       NULL,OPEN_EXISTING,0,NULL);
  if (Comport == INVALID_HANDLE_VALUE)
    return FALSE;
  // Configure Serial port (Setup Comm)
  if (!SetupComm(Comport,350,20)) // Buffer sizes
    return FALSE;
  
  // setup DCB using current values
  if (!GetCommState(Comport,&myDCB))
    return FALSE;
  myDCB.fInX = FALSE;     // Turn off xon/xoff handler
  myDCB.fOutX = FALSE;
  myDCB.fOutxDsrFlow = FALSE;
  if (HwFc)       // no flow control
    {
      myDCB.fOutxCtsFlow = TRUE;     // no hardware flow control.
      myDCB.fRtsControl = RTS_CONTROL_HANDSHAKE;
    }
  else
    {
      myDCB.fOutxCtsFlow = FALSE;    // no hardware flow control.
    }
  
  myDCB.BaudRate = baud;
  myDCB.DCBlength = sizeof(DCB);
  myDCB.fBinary = 1;
  myDCB.fParity = 0;
  myDCB.fDtrControl = DTR_CONTROL_DISABLE;
  myDCB.fDsrSensitivity = 0;
  myDCB.fTXContinueOnXoff = 1;
  myDCB.fNull = 0;
  myDCB.fRtsControl = RTS_CONTROL_DISABLE;
  myDCB.fDummy2 = 0;
  myDCB.wReserved = 0;
  myDCB.Parity = NOPARITY;
  myDCB.StopBits = ONESTOPBIT;
  myDCB.wReserved1 = 0;
  myDCB.ByteSize = 8;
  
  
  if (!SetCommState(Comport,&myDCB))
    {
      ShowLastError();
      return FALSE;
    }
  
        // Set timeouts
  CTout.ReadIntervalTimeout = 0xffffffff;
  CTout.ReadTotalTimeoutMultiplier = 0;
  CTout.ReadTotalTimeoutConstant = 0;
  CTout.WriteTotalTimeoutMultiplier = 0;
  CTout.WriteTotalTimeoutConstant = 5000;         // don't hang if CTS is locked, for example
  
  SetCommTimeouts(Comport,&CTout);
  
  EscapeCommFunction(Comport,SETDTR);
  PurgeComm(Comport,PURGE_TXCLEAR | PURGE_RXCLEAR);

  handle = hwnd;
  SerialPort = Comport;
  StartCommThread();

  return TRUE;
}

/**
   Closes the serial port.  Stops the Rx thread.
*/
void CloseSerialPort(void)
{
  if (!SerialPort) return;
  
  if (Thread)
    {
      StopThread = TRUE;
      WaitForSingleObject(Thread,2000);
      CloseHandle(Thread);
      Thread = NULL;
      StopThread = FALSE;             // reset for next time
    }
  
  PurgeComm(SerialPort,PURGE_TXCLEAR | PURGE_RXCLEAR);
  CloseHandle(SerialPort);
  SerialPort = NULL;
}

/**
   Puts a serial character out the serial port.  This assumes that the port
   is already opened.
   @param c Character to send.
 */
void PutSerialChar(int c)
{
  int Cnt;
  DWORD ModemStat;
  DWORD ticks;
  int Cts=1;
  
  ticks = GetTickCount();
  
  // check for flow control
  if (FlowControl)
    {
      while (Cts)
        {
          if (!GetCommModemStatus(SerialPort, &ModemStat))
            {
              ShowLastError();
              return;
            }
          Cts = !(ModemStat & MS_CTS_ON);
          // timeout after 3 seconds
          if (GetTickCount() > ticks + 1000)
            break;
        }
    }
  
  
  WriteFile(SerialPort,&c,1,(LPDWORD)&Cnt,NULL);
}


/**
   Internal function to start the Rx thread.
   @return Handle to new thread.
 */
HANDLE StartCommThread(void)
{
  int ThreadID;
  
  StopThread = FALSE;
  Thread = CreateThread(NULL,4096,ThreadProc,SerialPort,0,(LPDWORD)&ThreadID);
  return Thread;
}

/**
   Internal thread procedure function.  Polls the serial port for received characters,
   and sends a MESS_SERIAL message when characters are received.
 */
DWORD WINAPI ThreadProc(void *p)
{
  int Cnt;
  char buf[256];
  // read serial port, signal any chars found
  
  if (!handle)
    return;

  for(;;)
    {
      // check for chars from port or kbd:
      if (ReadFile(SerialPort,&buf,255,(LPDWORD)&Cnt,NULL) && Cnt)
        {
          // signal main thread
          SendMessage(handle,MESS_SERIAL,(unsigned int)Cnt,(unsigned long)buf);
        }
      else
        Sleep(50);
      
      if (StopThread)
        break;
    }
  return 0;
}

/**
   Query function used to determine if serial port is open.
   @return TRUE if serial port has been successfully opened, or FALSE otherwise.
 */
int SerialPortIsOpen(void)
{
  if (SerialPort)
    return TRUE;
  return FALSE;
}

/**
   Query function used to determine if there are serial characters available.
   This function is only needed if the handle parameter to OpenPort() was NULL.
   @return TRUE if there are one or more characters in the serial input buffer,
   FALSE if no characters are available.
 */
BOOL SerialIsChar(void)
{
  int Cnt,Avail,Remain;
  int buf;
  char str[100];

  PeekNamedPipe(SerialPort, (LPVOID)&buf, (DWORD) 1, (LPDWORD)&Cnt,(LPDWORD)&Avail, (LPDWORD)&Remain);

  if (Avail)
    return TRUE;

  return FALSE;
}

/**
   Returns a character from the serial port buffer.  This function assumes that the port has
   been already opened, and that characters have been detected with SerialIsChar().
   @return Character from serial port, or EOF on error.
 */
int SerialGetChar(void)
{
  char ch;
  int Cnt;

  if (!SerialPort)
    return EOF;

  ReadFile(SerialPort,&ch,1,(LPDWORD)&Cnt,NULL);
  if (!Cnt)
    return EOF;

  return (int) ch;
}

/**
   @}
*/

