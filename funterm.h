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
  $Id: funterm.h,v 1.3 2010/04/01 15:19:45 blakelev Exp $
  $Revision: 1.3 $
  $Date: 2010/04/01 15:19:45 $
 */
#ifndef _TFUNTERM_H_ID_
#define _TFUNTERM_H_ID_

#include <windows.h>
/**
   @file funterm.h This file contains definitions used by FUNterm.
    @addtogroup term
    @{
 */

// Defines
/** Contains the state of the display.  This structure describes the current
    state of the display - which characters are displayed, and the cursor
    position and state.
*/
typedef struct {
  char **Lines;		///< Pointer to the array of char pointers (lines of text)
  int Count;		///< Number of lines in display.
  int Capacity;		///< Allocated number of lines.
  int *LineLen;		///< Pointer to array of line lengths.
  int CursX,CursY;	///< Current cursor position.
  BOOL Cursor;		///< On/off state of cursor.
} TLines;
/**
   Contains the items stored in the system registry.  These are stored under 
the registry key HKEY_CURRENT_USER\\Software\\FUNterm.
 */
typedef struct {
  int ComPort;		    ///< Comport last used.  1 = COM1, 2 = COM2, etc.
  int Baud;		    ///< Baud rate of serial port the last time it was opened.
  BOOL OpenOnStart;         ///< Should the port be opened on program startup?  1 = YES, 0 = NO.
  BOOL HdwFlow;		    ///< Should hardware flow control used?  1 = YES, 0 = NO.
  BOOL CrLf;                ///< CR/LF flag, true for unix behavior
} TRegContents;

// Variables
// flags used to signal lighting of rx/tx LEDs
extern BOOL RxFlag;
extern BOOL TxFlag;

// Functions
void UpdateStatusBar (LPSTR lpszStatusString ,WORD partNumber ,WORD displayFlags );
static BOOL CreateSBar (HWND hwndParent ,char * initialText ,int nrOfParts );
static BOOL InitApplication (void);
HWND CreatefuntermWndClassWnd (void);
BOOL _stdcall DlgWinProc (HWND hwnd ,UINT msg ,WPARAM wParam ,LPARAM lParam );
void MainWndProc_OnCommand (HWND hwnd ,int id ,HWND hwndCtl ,UINT codeNotify );
LRESULT CALLBACK MainWndProc (HWND hwnd ,UINT msg ,WPARAM wParam ,LPARAM lParam );
int WINAPI WinMain (HINSTANCE hInstance ,HINSTANCE hPrevInstance ,LPSTR lpCmdLine ,INT nCmdShow );
void AddChar (char ch );
void InitCommDialog (HWND wnd );
void Paint (HWND wnd );
void DoKey(HWND wnd,int Key);
TLines *CreateLines(int Count);
void DestroyLines(TLines *Lines);
void PushChar(TLines *Lines,char ch);
void SetCursX(TLines *Lines,int x);
void SetCursY(TLines *Lines,int y);
void AddChar(char ch);
void ReadReg(void);
void SaveReg(void);


#endif

/**
   @}
*/
