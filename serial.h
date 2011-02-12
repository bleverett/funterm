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
#ifndef SERIAL_H
#define SERIAL_H
/*
  CVS info:
  $Id: serial.h,v 1.2 2010/04/01 15:19:45 blakelev Exp $
  $Revision: 1.2 $
  $Date: 2010/04/01 15:19:45 $
 */

/**
   @file serial.h Includes defines for serial port operation.
   @addtogroup serial
 */


#include <stdio.h>
#include <windows.h>


#define MESS_SERIAL (WM_USER+1)  ///< Custom windows message ID for serial messages.

BOOL OpenPort(int port,int baud,int HwFc, HWND handle);
void CloseSerialPort(void);
void PutSerialChar(int c);
int SerialPortIsOpen(void);
BOOL SerialIsChar(void);
int SerialGetChar(void);

#endif
