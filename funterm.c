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
  $Id: funterm.c,v 1.11 2010/04/01 15:19:45 blakelev Exp $
  $Revision: 1.11 $
  $Date: 2010/04/01 15:19:45 $
 */
#include "funterm.h"
#include <windowsx.h>
#include <commctrl.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "funtermres.h"
#include "serial.h"

/** @file
    This file is the main module of the project. It contains the code 
    for the user interface and handling the strings that get displayed,
    and every major function of the program.


    @mainpage FUNterm

    @section intro Introduction
    This program is a basic terminal program, written for Windows, that communicates
    with remote devices over a serial port.

    Though this is a Win32 program, it was developed under GNU/Linux using the MinGW
    compiler and run with Wine.  As it only uses basic Win32 calls, with no high-level
    libraries, it runs stably under Wine.

    Also, the serial.c file contains a working Win32 serial port interface, which may
    be used in any application that requires serial interface with Windows.

    @section compiling Compiling
    If you have MinGW loaded, the included Makefile should work.  If you are compiling from
    Windows, you may need to modify the makefile to work with Windows path conventions.
    
    @section features Features
    - Supports 64 COM ports.
    - Supports escape sequences.  All escape sequences start with the escape character, 0x1B.
      - <b><ESC> T</b> - Clear to end of line.
      - <b><ESC> Y</b> - Clear to end of screen.
      - <b><ESC> . <i>x</i></b> - Turn cursor on/off.  If <i>x</i> == '0', cursor is off.  
                                 Otherwise, cursor is turned on.
      - <b><ESC> = <i>row col</i></b> - Set cursor position to <i>row, col</i>.  Parameters
               are biased by 0x20 (space character).  For example, to set the column to 40,
               <i>col</i> would be 0x20 + 40 = 32 + 40 = 72 = 0x48 = 'H'.
    - Supports control-character commands.  Note that these are characters received over the
      serial port, not entered via the keyboard.
      - <b>Control-X</b> - Move to home position.
      - <b>Control-Z</b> - Clear screen and home.

    @section reg Registry Usage
    The registry is used so store program settings.  Settings are stored in 
    HKEY_CURRENT_USER\\Software\\FUNterm.  These parameters are saved:
    - Comm Port
    - Baud rate.
    - OpenOnStart.  If true the comport is opened on startup.
    - Hardware flow control setting.

    @defgroup term Terminal
    @{

    See Main Page for features.  This file implements all of the terminal functions, except
    for serial connectivity.
               
 */

// Defines:
#define Margin 5           ///< Margin in pixels between edge of main control's edge and text.
#define FIXED_CONFIG_1 0   ///< Special build flag to create a fixed config version, should be zero for most users

// Enumerations:
/// State variable for processing escape codes (VT100).
enum TEscProg {
  Idle=-1,      ///< Not processing escape sequence yet.
  Ready,        ///< Got the Escape character, waiting for command.
  CursorOnOff,  ///< Waiting for cursor on/off parameter character.
  CursPosX,     ///< Waiting for cursor X position.
  CursPosY      ///< Waiting for cursor Y position.
} EscProg=Idle;
/// Current state of serial port, used for updating the status bar.
enum TStatus {
  stOff,     ///< Serial port is off.
  stError,   ///< Serial port cannot be opened.
  stRunning, ///< Serial port opened successfully.
  stResize   ///< Status bar is being resized.
};

// Functions:
void OnConfigComm(HWND wnd);
void DrawLEDs(DRAWITEMSTRUCT *dis);
void FillInStatus(int Status);
void CenterWindow(HWND wnd);
void CopyToClipboard(HWND wnd);
void PasteFromClipboard(HWND wnd);
void ShowMenu(HWND wnd);
void DrawChar(int x,int y,char ch);
void SetMinMaxInfo(MINMAXINFO *p);
void ClearScreen(void);
void SendFile(void);
void SaveFile(void);
void StartLog(void);
void EndLog(void);
void AddBinaryChar(char ch);
LRESULT CALLBACK BinWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam);

// Variables:
HINSTANCE hInst;                ///< Handle to this instance of the program.
HWND hwndMain;                  ///< Handle to the main window of the program.
HWND hwndBin;                   ///< Handle to the binary view window.
HWND hwndBinEdit;               ///< Handle to the edit control in the bin view window.

TLines *Lines=NULL;             ///< Pointer to the global TLines structure.
int TopLine=0;                  ///< Index of first line on screen.
int ScrnLineCount=1;            ///< Number of lines on the screen.
TRegContents RegContents;       ///< Global registry stuff.
/// Supported baud rates (in BPS).
int BaudRates[8] = {300,2400,4800,9600,19200,38400,57600,115200};
char FileName[300];             ///< Filename string used anywhere a filename is needed.
HMENU PopupMenu=NULL;           ///< Pointer to popup menu.
int CharWd=5,CharHt=5;          /**< Size of a single character in pixels.
                                This is determined by calling GetTextExtentPoint32()
                                on an 'A'. */
int LineLength=80;              ///< Current width of window in characters.
HFONT font;                     ///< Font used for drawing characters.
FILE *LogFile=0;                ///< File pointer to the Logging file.
HWND  hWndStatusbar;            ///< Windows handle to the Status Bar
BOOL RxFlag=FALSE;              ///< Flag used to signal the Rx "LED" to flash
BOOL TxFlag=FALSE;              ///< Flag used to signal the Tx "LED" to flash

/**
   Updates the statusbar control with the input text.

   @param lpszStatusString Charactar string to be displayed.
   @param partNumber Index of the status bar part number to be
          used for display.
   @param displayFlags Value passed to the statusbar.  See the SB_SETTEXT
          win32 message for details.
*/
void UpdateStatusBar(LPSTR lpszStatusString, WORD partNumber, WORD displayFlags)
{
  SendMessage(hWndStatusbar,
              SB_SETTEXT,
              partNumber | displayFlags,
              (LPARAM)lpszStatusString);
}

/**
   Initializes the status bar.
   @param  hwndParent Handle to the parent window.
   @param  nrOfParts  The status bar can contain more than one
   pane, and this parameter specifies how many panes the status
   bar will have.
*/
void InitializeStatusBar(HWND hwndParent,int nrOfParts)
{
  const int cSpaceInBetween = 8;
  int   ptArray[6];   // Array defining the number of parts/sections
  RECT  rect;
  HDC   hDC;
  
  /* Fill in the ptArray...  */
  
  hDC = GetDC(hwndParent);
  GetClientRect(hwndParent, &rect);
  LineLength = (rect.right - rect.left)/CharWd;
  
  ptArray[0] = 46;
  ptArray[1] = 100;
  ptArray[2] = 151;
  ptArray[3] = 194;
  ptArray[4] = 326;
  ptArray[nrOfParts-1] = -1;  // Last part extends to right side of window
  
  ReleaseDC(hwndParent, hDC);
  SendMessage(hWndStatusbar,
              SB_SETPARTS,
              nrOfParts,
              (LPARAM)(LPINT)ptArray);
}


/**
   Calls CreateStatusWindow to create the status bar.
   @param  hwndParent Handle to the parent window that owns the new status bar.
   @param  initialText The initial contents of the status bar.
   @param  nrOfParts The number of panes in the status bar.
   @return TRUE on success, or FALSE on failure.
*/
static BOOL CreateSBar(HWND hwndParent,char *initialText,int nrOfParts)
{
  hWndStatusbar = CreateStatusWindow(WS_CHILD | WS_VISIBLE | WS_BORDER|SBARS_SIZEGRIP,
                                     initialText,
                                     hwndParent,
                                     IDM_STATUSBAR);
  if (hWndStatusbar)
    {
      InitializeStatusBar(hwndParent,nrOfParts);
      return TRUE;
    }
  
  return FALSE;
}

/**
   Initializes the application. This is the first function called from WinMain.
   @return TRUE on success, or FALSE on failure.
*/
static BOOL InitApplication(void)
{
  WNDCLASSEX wc;
  
  memset(&wc,0,sizeof(WNDCLASSEX));
  wc.cbSize = sizeof(WNDCLASSEX);
  /// Sets main window with parameters CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS
  wc.style = CS_HREDRAW|CS_VREDRAW |CS_DBLCLKS;
  wc.lpfnWndProc = (WNDPROC)MainWndProc;
  wc.hInstance = hInst;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
  /// Main window has class name "funtermWndClass"
  wc.lpszClassName = "funtermWndClass";
  wc.lpszMenuName = MAKEINTRESOURCE(IDMAINMENU);
  wc.hCursor = LoadCursor(NULL,IDC_ARROW);
  wc.hIcon = LoadIcon(hInst,MAKEINTRESOURCE(IDAPPLICON));
  if (!RegisterClassEx(&wc))
    return 0;

  /// Binary window has class name "binWndClass"
  wc.lpszClassName = "binWndClass";
  wc.lpszMenuName = NULL;
  wc.hCursor = LoadCursor(NULL,IDC_ARROW);
  wc.hIcon = 0;//LoadIcon(hInst,MAKEINTRESOURCE(IDAPPLICON));
  wc.lpfnWndProc = (WNDPROC)BinWndProc;
  if (!RegisterClassEx(&wc))
    return 0;
  
  return 1;
}

/**
   Creates the Window Class for application.
   @return Handle to main window created.
*/
HWND CreatefuntermWndClassWnd(void)
{
  /** Window parameters include WS_MINIMIZEBOX, WS_VISIBLE, WS_CLIPSIBLINGS,
      WS_CLIPCHILDREN, WS_MAXIMIZEBOX, WS_CAPTION, WS_BORDER, WS_SYSMENU,
      and WS_THICKFRAME.
  */
  
  return CreateWindowEx(0,"funtermWndClass","FUNterm",
                        WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,
                        CW_USEDEFAULT,0,CW_USEDEFAULT,0,
                        NULL,
                        NULL,
                        hInst,
                        NULL);
}

/**
   Win32 callback function for the config and help/about dialogs.
   @param hwnd Handle to dialog box sending message.
   @param msg Windows message to handle.
   @param wParam First message parameter.
   @param lParam Second message parameter.
   @return Non-zero if message is processed, zero if not processed.
*/
BOOL _stdcall DlgWinProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
    {
    case WM_CLOSE:
      EndDialog(hwnd,0);
      return 1;
    case WM_COMMAND:
      switch (LOWORD(wParam))
        {
        case IDOK:
          OnConfigComm(hwnd);
        case IDCANCEL:
          EndDialog(hwnd,1);
          return 1;
        }
      break;
    case WM_INITDIALOG:
      CenterWindow(hwnd);
      if (lParam)             // only for comm setup dlg.
        {
          InitCommDialog(hwnd);
          return 1;
        }
      break;
    }
  return 0;
}


/**
   Main windows command handler.  See Win32 HANDLE_WM_COMMAND macro for details.
   @param hwnd Handle to main window.
   @param id Window message ID.
   @param hwndCtl Handle to something.
   @param codeNotify Code (unused).
*/
void MainWndProc_OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
  switch(id)
    {
    case IDM_ABOUT:
      DialogBox(hInst,MAKEINTRESOURCE(IDD_ABOUT),
                hwndMain,DlgWinProc);
      break;
    case IDM_CONFIG:
      DialogBoxParam(hInst,MAKEINTRESOURCE(IDD_CONFIG),
                     hwndMain,DlgWinProc,1);
      break;
    case IDM_STARTCOMM:
      // start or stop serial port
      if (SerialPortIsOpen()) // close it
        {
          CloseSerialPort();
          FillInStatus(stOff);
        }
        else if (!OpenPort(RegContents.ComPort,
                           BaudRates[RegContents.Baud],
                           RegContents.HdwFlow, 
                           hwndMain))
          {
            MessageBox(hwndMain,"Cannot open serial port!\n","Error",MB_OK|MB_ICONSTOP);
            CloseSerialPort();
            FillInStatus(stError);
          }
        else
          FillInStatus(stRunning);
      break;
    case IDM_EXIT:
      PostMessage(hwnd,WM_CLOSE,0,0);
      break;
    case IDM_COPY:
      // copy contents to clipboard
      CopyToClipboard(hwnd);
      break;
    case IDM_PASTE:
      // paste from clipboard
      PasteFromClipboard(hwnd);
      break;
    case IDM_CLEAR:
      // clear screen
      ClearScreen();
      break;
    case IDM_SEND:
      // send file to port
      SendFile();
      break;
    case IDM_CRLF:
      // Toggle CR/LF usage
      RegContents.CrLf = !RegContents.CrLf;
      if (SerialPortIsOpen())
        FillInStatus(stRunning);
      break;
    case IDM_LOG_START:
      StartLog();
      break;
    case IDM_LOG_END:
      EndLog();
      break;
    case IDM_SAVE:
      // save screen data to file
      SaveFile();
      break; 
    case IDM_BINARY:
        // Show binary in new window.
        if (!hwndBin)
        {
            LOGFONT LogFont;
            HFONT fnt;
            hwndBin = CreateWindowEx(0,"binWndClass","Binary View",
                                     WS_MINIMIZEBOX|WS_VISIBLE|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|WS_MAXIMIZEBOX|WS_CAPTION|WS_BORDER|WS_SYSMENU|WS_THICKFRAME,
                                     CW_USEDEFAULT,0,300,500,
                                     NULL,
                                     NULL,
                                     hInst,
                                     NULL);
            hwndBinEdit = CreateWindowEx(0,"EDIT","",
                                  WS_VISIBLE|WS_CHILD|ES_MULTILINE|ES_AUTOHSCROLL|ES_AUTOVSCROLL|WS_HSCROLL|WS_VSCROLL,
                                  //CW_USEDEFAULT,0,CW_USEDEFAULT,0,
                                  0, 0, 100, 100,
                                  hwndBin,
                                  NULL,
                                  hInst,
                                  NULL);

            fnt = GetStockObject(ANSI_FIXED_FONT);
            GetObject(fnt,sizeof(LOGFONT),&LogFont);
            LogFont.lfHeight = LogFont.lfHeight * 4/3; //-MulDiv(10, GetDeviceCaps(DC, LOGPIXELSY), 72);
            fnt = CreateFontIndirect(&LogFont);
            SendMessage(hwndBinEdit, WM_SETFONT, (WPARAM)fnt, 0);
        }
        if (hwndBin)
            ShowWindow(hwndBin,SW_SHOW);
        break;
    default:
        break;
    }
}

/**
   The window procedure (callback) for the main window.
   @param hwnd Handle to main window.
   @param msg Windows message to handle.
   @param wParam First message parameter.
   @param lParam Second message parameter.
   @return Depends on the specific message handled.
 */
LRESULT CALLBACK MainWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
  switch (msg) 
    {
    case WM_SIZE:
      SendMessage(hWndStatusbar,msg,wParam,lParam);
      FillInStatus(stResize);
      break;
    case WM_GETMINMAXINFO:
      // set minimum size of window
      SetMinMaxInfo((MINMAXINFO*)lParam);
      break;
    case WM_COMMAND:
      HANDLE_WM_COMMAND(hwnd,wParam,lParam,MainWndProc_OnCommand);
      break;
    case WM_CREATE:
      Lines = CreateLines(4);
      break;
    case WM_DESTROY:
      EndLog();
      SaveReg();
      // close serial port
      CloseSerialPort();
      DestroyLines(Lines);
      DestroyMenu(PopupMenu);
      PostQuitMessage(0);
      break;
    case WM_CHAR:
      DoKey(hwnd,wParam);
      break;
    case WM_PAINT:
      Paint(hwnd);
      break;
      /// This application includes a custom message type: MESS_SERIAL.
    case MESS_SERIAL:       // custom message: buf and count sent
      {
        int i;
        for (i=0;i<wParam;i++)
          {
            AddChar(((char*)lParam)[i]);
            RxFlag = TRUE;          // signal LED to go on.
            // send char to log file
            if (LogFile)
              fputc(((char*)lParam)[i],LogFile);
            // Add to binary window
            AddBinaryChar(((char*)lParam)[i]);
          }
      }
      break;
    case WM_DRAWITEM:
      if (wParam == IDM_STATUSBAR)
        DrawLEDs((DRAWITEMSTRUCT *)lParam);
      break;
    case WM_RBUTTONUP:
      if (!wParam)    // only do context menu if no other buttons/keys are down
        ShowMenu(hwnd);
      break;
    default:
      return DefWindowProc(hwnd,msg,wParam,lParam);
    }
  return 0;
}

/**
   The window procedure (callback) for the binary window.
   @param hwnd Handle to main window.
   @param msg Windows message to handle.
   @param wParam First message parameter.
   @param lParam Second message parameter.
   @return Depends on the specific message handled.
 */
LRESULT CALLBACK BinWndProc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
  switch (msg)
    {
    case WM_SIZE:
        MoveWindow(hwndBinEdit, 0, 0, LOWORD(lParam), HIWORD(lParam), 1);
      break;
    case WM_DESTROY:
        // Destroy edit window
        DestroyWindow(hwndBinEdit);
        hwndBin = 0;
      break;
    default:
      return DefWindowProc(hwnd,msg,wParam,lParam);
    }
  return 0;
}

/**
   Sets a MINMAXINFO structure for the OS.  Tells the OS the minimum size
   for the main window, in response to a WM_GETMINMAXINFO message.
   @param mmi Pointer to MINMAXINFO structure to be set.
 */
void SetMinMaxInfo(MINMAXINFO *mmi)
{
  POINT P;
  P.x = 325;
  P.y = 150;
  mmi->ptMinTrackSize = P;
  
}

/**
   This is the main entry into the application.  WinMain initializes everything,
   and then executes a message loop until the application exits.
   @param hInstance Handle to application instance.
   @param hPrevInstance Always NULL.
   @param lpCmdLine String containing the command line used to launch the application.
   @param nCmdShow Specifies how the window is to be shown.  See WinMain in the
   Win32 help file.
   @return Zero on error, nonzero otherwise.
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, INT nCmdShow)
{
  MSG msg;
  HANDLE hAccelTable;
  HDC DC;
  LOGFONT LogFont;
  SIZE Size;
  
  InitCommonControls();
  hInst = hInstance;
  if (!InitApplication())
    return 0;
  hAccelTable = LoadAccelerators(hInst,MAKEINTRESOURCE(IDACCEL));
  if ((hwndMain = CreatefuntermWndClassWnd()) == (HWND)0)
    return 0;
  
  CreateSBar(hwndMain,"",2);
  
  // Load popup menu from resource
  PopupMenu = LoadMenu(hInst,MAKEINTRESOURCE(IDPOPUPMENU));
  
  // read registry contents, config if no reg info found.
  ReadReg();
  
  // Open serial port, fill in status bar
  if (RegContents.OpenOnStart && !OpenPort(RegContents.ComPort,
                                           BaudRates[RegContents.Baud],
                                           RegContents.HdwFlow,
                                           hwndMain))
    {
      MessageBox(NULL,"Cannot open serial port!\n","Error",MB_OK|MB_ICONSTOP);
      CloseSerialPort();
      PostMessage(hwndMain,WM_COMMAND,IDM_CONFIG,0);
      FillInStatus(stError);
    }
  else
    {
      if (SerialPortIsOpen())
        FillInStatus(stRunning);
      else
        FillInStatus(stOff);
    }
  
  // draw LEDs
  UpdateStatusBar(NULL, 0, SBT_OWNERDRAW);
  
  // init char drawing stuff
  
  DC = GetDC(hwndMain);
  font = GetStockObject(ANSI_FIXED_FONT);
  GetObject(font,sizeof(LOGFONT),&LogFont);
  LogFont.lfHeight = LogFont.lfHeight * 4/3; //-MulDiv(10, GetDeviceCaps(DC, LOGPIXELSY), 72);
  font = CreateFontIndirect(&LogFont);
  SelectObject(DC,font);
  GetTextExtentPoint32(DC,"A",1,&Size);
  ReleaseDC(hwndMain,DC);
  
  CharWd = Size.cx;
  CharHt = Size.cy;
  
  ShowWindow(hwndMain,SW_SHOW);
  while (GetMessage(&msg,NULL,0,0)) 
    {
      if (!TranslateAccelerator(msg.hwnd,hAccelTable,&msg)) 
        {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
        }
    }
  return msg.wParam;
}

/**
   Adds a character to the terminal display.  Called every time a character
   is received from the serial port.  This function implements escape sequences.
   See the main page for a description of escape sequences supported.
   @param ch The character to add to the display.
*/
void AddChar(char ch)
{
  unsigned int x;
  
  
  if (EscProg != Idle)    // We're parsing escape sequence
    {
      switch (EscProg)
        {
        case Ready:
          switch (toupper(ch))
            {
            case 'T':
              // Clear to EOL
              Lines->Lines[Lines->CursY][Lines->CursX] = 0;
              EscProg = Idle;
              return;
            case 'Y':
              // Clear to EOF
              Lines->Lines[Lines->CursY][Lines->CursX] = 0;
              for(x=Lines->CursY;x<Lines->Count;x++)
                Lines->Lines[x][0] = 0;
              EscProg = Idle;
              return;
            case '.':
              EscProg = CursorOnOff;
              return;
            case '=':
              EscProg = CursPosX;
              return;
            default:
              EscProg = Idle;         // kill the command sequence.
              return;
            }
        case (CursorOnOff):
          if (ch == '0')
            Lines->Cursor = FALSE;
          else
            {
              Lines->Cursor = TRUE;
              InvalidateRect(hwndMain,NULL,FALSE);
            }
          EscProg = Idle;
          return;
        case (CursPosX):
          SetCursY(Lines,ch - 0x20);
          EscProg = CursPosY;
          return;
        case (CursPosY):
          SetCursX(Lines,ch - 0x20);
          EscProg = Idle;
          return;
        default:
          EscProg = Idle;         // kill the command sequence.
          return;
        }
    }
  
  switch (ch)
    {
    case (-1):
      return;
    case (0x1e):
    case (0x18):
      // Home position (??)
      SetCursX(Lines,0);
      SetCursY(Lines,0);
      break;
    case (0x1a):  // Control-z
    case 12:      // Form-feed (cntrl-l)
      // Clear Screen and home (cntrl-z)
      ClearScreen();
      break;
    case (0x1b):
      // Start escape sequence
      EscProg = Ready;
      break;
    case 13:
      SetCursX(Lines,0);
      break;
    case 10:
      if (RegContents.CrLf)
        // implied CR
        SetCursX(Lines,0);
      SetCursY(Lines,Lines->CursY+1);
      break;
    case 8:         // backspace
      SetCursX(Lines,Lines->CursX - 1);
      break;
    case 9:         // tab
      // Must add 8 spaces for tab
      for (x=0;x<8;x++)
        AddChar(' ');
      SetCursX(Lines,(Lines->CursX + 8) % 8);
      break;
    default:
      // check for word wrap
      if (Lines->CursX >= LineLength -1)
        {
          // fake cr/lf
          SetCursX(Lines,0);
          SetCursY(Lines,Lines->CursY+1);
        }
      DrawChar(Lines->CursX,Lines->CursY,ch);
      PushChar(Lines,ch);
      break;
    }
}

/**
   Initializes the comm settings dialog.  Called before displaying the dialog.
   @param wnd Handle to dialog window.
*/
void InitCommDialog(HWND wnd)
{
  HWND Control;
  int i;
  char str[40];
  
  // init com port listbox
  Control = GetDlgItem(wnd, ID_COMPORT);
  for (i=0;i<64;i++)
    {
      sprintf(str,"COM%d",i+1);
      SendMessage(Control,LB_ADDSTRING, 0, (long)str);
    }
  SendMessage(Control,LB_SETCURSEL, RegContents.ComPort-1, 0);
  
  
  // init baud rate button
  Control = GetDlgItem(wnd,ID_BAUD+RegContents.Baud);
  SendMessage(Control,BM_SETCHECK,BST_CHECKED,0);
  
  // init auto-open button
  Control = GetDlgItem(wnd,ID_CBOPEN);
  SendMessage(Control,BM_SETCHECK,RegContents.OpenOnStart ? BST_CHECKED : BST_UNCHECKED,0);
  
  // init flow control button
  Control = GetDlgItem(wnd,ID_CBFLOW);
  SendMessage(Control,BM_SETCHECK,RegContents.HdwFlow ? BST_CHECKED : BST_UNCHECKED,0);
  
}

/**
   Execute comport parameter settings.  Called after user presses "OK" in the
   comm config dialog.
   @param wnd Handle to dialog window.
*/
void OnConfigComm(HWND wnd)
{
  // get stuff from dialogs
  HWND Control;
  int i;

  /// Closes and re-opens serial port if it is already open.
  CloseSerialPort();
  
  // get com port number
  Control = GetDlgItem(wnd,ID_COMPORT);
  RegContents.ComPort = SendMessage(Control, LB_GETCURSEL, 0, 0) + 1;
  
  // read baud rate button
  for (i=0;i<8;i++)
    {
      Control = GetDlgItem(wnd,ID_BAUD+i);
      if (SendMessage(Control,BM_GETCHECK,0,0) == BST_CHECKED)
        break;
    }
  RegContents.Baud = i;
  
  /// Saves config settings to registry.
  // auto-open button
  Control = GetDlgItem(wnd,ID_CBOPEN);
  RegContents.OpenOnStart = SendMessage(Control,BM_GETCHECK,0,0);
  
  // init flow control button
  Control = GetDlgItem(wnd,ID_CBFLOW);
  RegContents.HdwFlow = SendMessage(Control,BM_GETCHECK,0,0);
  
  /// Opens the serial port with the new settings.
  PostMessage(hwndMain,WM_COMMAND,IDM_STARTCOMM,0);
}

/**
   Paints the main window's terminal control area.  Updates the status bar
   if necessary.  If the window is minimized, the painting is suspended to save
   CPU time. The cursor is also drawn by this function.  This function is called
   in response to a WM_PAINT message.
   @param wnd Handle to dialog window.
*/
void Paint(HWND wnd)
{
  PAINTSTRUCT ps;
  RECT R,T;
  HBITMAP Bmp;                    // off-screen bitmap
  HDC DC;
  SIZE Size;
  int i;
  
  // tell statusbar to redraw if nec.
  if (RxFlag || TxFlag)
    UpdateStatusBar(NULL, 0, SBT_OWNERDRAW);
  
  if (IsIconic(wnd))
    {
      // must do begin/end paint or WM_PAINTs will be
      // continuously sent while window is minimized.
      BeginPaint(wnd,&ps);
      EndPaint(wnd,&ps);
      // force the lines to scroll off screen if nec. (on resize shorter)
      SetCursY(Lines,Lines->CursY);
      return; // don't draw on minimized window
    }
  
  BeginPaint(wnd,&ps);
  
  /** Painting is done by first drawing the screen to an off-screen bitmap, and 
      then copying the bitmap to the terminal control with a BitBlt() command.
      Without this copying, the program flashes horribly.
  */
  // draw to off-screen bitmap
  GetClientRect(wnd,&R);
  Bmp = CreateCompatibleBitmap(ps.hdc,R.right,R.bottom);
  DC = CreateCompatibleDC(ps.hdc);
  SelectObject(DC,Bmp);
  SelectObject(DC,font);
  
  // clear bitmap
  FillRect(DC,&R,GetStockObject(WHITE_BRUSH));
  
  // draw border around term window.
  GetWindowRect(hWndStatusbar,&T);
  ScreenToClient(wnd,(POINT *)&T);
  R.bottom = T.top;
  DrawEdge(DC,&R,EDGE_SUNKEN,BF_RECT);
  
  // draw lines
  if (Lines->CursX)
    GetTextExtentPoint32(DC,Lines->Lines[Lines->CursY],Lines->CursX,&Size);
  else
    {GetTextExtentPoint32(DC,"ABC",3,&Size);
      Size.cx = 0;
    }
  if (Size.cy)
    ScrnLineCount = R.bottom/Size.cy;       // number of lines on screen
  for(i=TopLine;i<Lines->Count;i++)
    TextOut(DC,Margin,Margin+(i-TopLine)*Size.cy,Lines->Lines[i],strlen(Lines->Lines[i]));
  
  // draw cursor
  if (Lines->Cursor)
    {
      int y;                 // y dim.
      int x;                  // x dim of cursor
      y = (Lines->CursY - TopLine + 1) * Size.cy + Margin - 2;
      x = Size.cx + Margin;
      MoveToEx(DC,x,y,NULL);
      LineTo(DC,x+CharWd,y);
    }
  
  // draw bitmap to screen
  BitBlt(ps.hdc,0,0,R.right,R.bottom,DC,0,0,SRCCOPY);
  
  DeleteDC(DC);
  DeleteObject(Bmp);
  EndPaint(wnd,&ps);
  
  // force the lines to scroll off screen if nec. (on resize shorter)
  SetCursY(Lines,Lines->CursY);
}


/**
   Creates and initializes a TLines structure.  Includes the allocation of space for 
   the array of strings to be displayed.
   @param Count Number of lines to be allocated.
   @return Pointer to new TLines structure.
*/
TLines *CreateLines(int Count)
{
  int i;
  // create the struct.
  TLines *Lines = malloc(sizeof(TLines));
  memset(Lines,0,sizeof(TLines));
  Lines->Count = 0;
  Lines->Capacity = Count;
  Lines->Cursor = TRUE;
  
  // allocate ptr space
  Lines->Lines = malloc(Count*sizeof(void*));
  Lines->LineLen = malloc(Count*sizeof(int *));
  
  // allocate string space
  for (i=0;i<Count;i++)
    {
      Lines->Lines[i] = malloc(8);
      Lines->Lines[i][0] = 0;         // asciiz terminate
      Lines->LineLen[i] = 8;
    }
  
  return Lines;
}

/**
   De-allocates TLines structure.  Also de-allocates the lines of text allocated
   by CreateLines().
   @param Lines Pointer to TLines structure to destroy.
*/
void DestroyLines(TLines *Lines)
{
  int i;
  
  // free strings
  for (i=0;i<Lines->Capacity;i++)
    free(Lines->Lines[i]);
  
  // free pointers
  free(Lines->Lines);
  free(Lines->LineLen);
}

/**
   Sets the current cursor's column position.  Manages the TLines structure
   to move the cursor.
   @param Lines Pointer to TLines structure.
   @param x New column position, zero-based.
*/
void SetCursX(TLines *Lines,int x)
{
  int y = Lines->CursY;
  char *p = Lines->Lines[y];
  int z;
  
  if (x < 0) return;
  if (x > 300) return;
  
  // expand line if nec.
  if (x+1 >= Lines->LineLen[y])
    {
      p = realloc(p,x+10);
      Lines->Lines[y] = p;
      Lines->LineLen[y] = x+10;
    }
  
  // add spaces if nec.
  z = strlen(p);
  while (z < x)
    {
      p[z++] = ' ';
      p[z] = 0;
    }
  
  // redraw if cursor is visible and has moved.
  if ((x != Lines->CursX) && Lines->Cursor)
    InvalidateRect(hwndMain,NULL,FALSE);
  Lines->CursX = x;
}


/**
   Sets the current cursor's row position.  Manages the TLines structure
   to move the cursor.  Allocates and de-allocates lines as necessary.
   @param Lines Pointer to TLines structure.
   @param y New column position, zero-based.
*/
void SetCursY(TLines *Lines,int y)
{
  int NewCap,i;
  
  if (y > 300) return;
  if (y < 0) return;
  
  if (y >= Lines->Capacity)
    {
      // must re-alloc Line list
      NewCap = y + 10;        // add a little more
      // alloc new space
      Lines->Lines = realloc(Lines->Lines,NewCap*sizeof(void*));
      Lines->LineLen = realloc(Lines->LineLen,NewCap*sizeof(int*));
      // create new strings
      for (i=Lines->Capacity;i<NewCap;i++)
        {
          Lines->Lines[i] = malloc(8);
          Lines->Lines[i][0] = 0;         // asciiz terminate
          Lines->LineLen[i] = 8;
        }
      Lines->Capacity = NewCap;
      Lines->Count = y + 1;
    }
  
  // redraw if cursor has moved.
  if ((y != Lines->CursY) && Lines->Cursor)
    InvalidateRect(hwndMain,NULL,FALSE);
  Lines->CursY = y;
  
  // adjust line count
  if (y >= Lines->Count)
    Lines->Count = y + 1;
  SetCursX(Lines,Lines->CursX);   // force Y line to be padded if necessary.
  
  // Remove lines if too many
  if (Lines->Count > ScrnLineCount)
    {
      // move lines down
      int x = Lines->Count+1 - ScrnLineCount;
      // delete strings from top of list
      for(i=0;i<x;i++)
        free(Lines->Lines[i]);
      // move pointers to bottom;
      memmove(&(Lines->Lines[0]),&(Lines->Lines[x]),sizeof(void *)*(Lines->Capacity-x));
      memmove(&(Lines->LineLen[0]),&(Lines->LineLen[x]),sizeof(int *)*(Lines->Capacity-x));
      // create new lines at end
      for(i=Lines->Capacity - x;i<Lines->Capacity;i++)
        {
          Lines->Lines[i] = malloc(8);
          Lines->Lines[i][0] = 0;         // asciiz terminate
          Lines->LineLen[i] = 8;
        }
      Lines->Count -= x;
      SetCursY(Lines,Lines->CursY-x);
      InvalidateRect(hwndMain,NULL,FALSE);
    }
}

/**
   Adds a new character to the TLines structure.
   @param Lines Pointer to TLines structure.
   @param ch Character to add to display.
*/
void PushChar(TLines *Lines,char ch)
{
  // add a char to list of lines
  char *p;
  int x = Lines->CursX;
  
  // add the char
  p = Lines->Lines[Lines->CursY];
  if (!p[x])
    p[x+1] = 0;                             // asciiz terminate
  p[x] = ch;
  
  SetCursX(Lines,Lines->CursX+1);
  if (Lines->CursY >= Lines->Count)
    Lines->Count = Lines->CursY+1;
}

/**
   Process an input key from OS.  Called in response to WM_CHAR message.
   @param wnd Handle to window.
   @param Key The character corresponding to the key pressed.
*/
void DoKey(HWND wnd,int Key)
{
  PutSerialChar(Key);
  TxFlag = TRUE;          // signal LED to go on.
}

/**
   Read config settings from registry.
*/
void ReadReg(void)
{
  HKEY Key;
  int Size;
  
  // default values
  RegContents.ComPort = 1;
  RegContents.Baud = 3;   // 9600 Baud
  RegContents.OpenOnStart = FIXED_CONFIG_1 ? TRUE : FALSE;
  RegContents.HdwFlow = FALSE;
  RegContents.CrLf = FALSE;
  
  // read params from registry
  if (RegOpenKeyEx(HKEY_CURRENT_USER,"Software\\FUNterm",
                   0,KEY_ALL_ACCESS,&Key) != ERROR_SUCCESS)
  {
      if (!FIXED_CONFIG_1)
          // Pop up config dialog on first usage
          PostMessage(hwndMain,WM_COMMAND,IDM_CONFIG,0);
      return; // key doesn't exist, use defaults
  }
  
  Size = sizeof(int);
  RegQueryValueEx(Key,"ComPort",0,NULL,(LPBYTE)&RegContents.ComPort,(LPDWORD)&Size);
  RegQueryValueEx(Key,"Baud",0,NULL,(LPBYTE)&RegContents.Baud,(LPDWORD)&Size);
  RegQueryValueEx(Key,"OpenOnStart",0,NULL,(LPBYTE)&RegContents.OpenOnStart,(LPDWORD)&Size);
  RegQueryValueEx(Key,"HdwFlow",0,NULL,(LPBYTE)&RegContents.HdwFlow,(LPDWORD)&Size);
  RegQueryValueEx(Key,"CrLf",0,NULL,(LPBYTE)&RegContents.CrLf,(LPDWORD)&Size);
  
  RegCloseKey(Key);
}

/**
   Saves config settings to registry.
*/
void SaveReg(void)
{
  HKEY Key;
  int x;
  
  // store all of this in registry
  if (RegCreateKeyEx(HKEY_CURRENT_USER,"Software\\FUNterm",
                     0,0,0,KEY_ALL_ACCESS,NULL,&Key,(LPDWORD)&x) != ERROR_SUCCESS)
    return; // bad

  RegSetValueEx(Key,"ComPort",0,REG_DWORD,(BYTE *)&RegContents.ComPort,sizeof(RegContents.ComPort));
  RegSetValueEx(Key,"Baud",0,REG_DWORD,(BYTE *)&RegContents.Baud,sizeof(RegContents.Baud));
  RegSetValueEx(Key,"OpenOnStart",0,REG_DWORD,(BYTE *)&RegContents.OpenOnStart,sizeof(RegContents.OpenOnStart));
  RegSetValueEx(Key,"HdwFlow",0,REG_DWORD,(BYTE *)&RegContents.HdwFlow,sizeof(RegContents.HdwFlow));
  RegSetValueEx(Key,"CrLf",0,REG_DWORD,(BYTE *)&RegContents.CrLf,sizeof(RegContents.CrLf));
  
  RegCloseKey(Key);
}

/**
   Draws Rx and Tx LEDs on statusbar.  Selects on/off LED's depending on RxFlag and TxFlag.
   Called in response to WM_DRAWITEM message to statusbar.
   @param dis Pointer to DRAWITEMSTRUCT, sent by OS with WM_DRAWITEM message.
*/
void DrawLEDs(DRAWITEMSTRUCT *dis)
{
  RECT *R;
  HDC DC;
  HBITMAP RxLed,TxLed;
  BOOL RedrawFlag = RxFlag || TxFlag;
  
  if (dis->hwndItem != hWndStatusbar)
    return;

  R = &dis->rcItem;
  
  // draw LEDs on
  RxLed = LoadBitmap(hInst,MAKEINTRESOURCE(RxFlag ? IDB_REDLEDON : IDB_REDLEDOFF));
  TxLed = LoadBitmap(hInst,MAKEINTRESOURCE(TxFlag ? IDB_GRNLEDON : IDB_GRNLEDOFF));
  
  DC = CreateCompatibleDC(dis->hDC);
  SelectObject(DC,RxLed);
  BitBlt(dis->hDC,R->left+23,R->top,15,15,DC,0,0,SRCCOPY);
  SelectObject(DC,TxLed);
  BitBlt(dis->hDC,R->left+5,R->top,15,15,DC,0,0,SRCCOPY);
  
  DeleteDC(DC);
  DeleteObject(TxLed);
  DeleteObject(RxLed);
  
  RxFlag = FALSE;
  TxFlag = FALSE;

  if (RedrawFlag)
    // using PostMessage allows the LED to stay on for a bit before it's cleared.
    PostMessage(hWndStatusbar,SB_SETTEXT,SBT_OWNERDRAW,0);
}


/**
   Print serial params to statusbar.
   @param Status The new status for the statusbar.  See TStatus for valid values.
*/
void FillInStatus(int Status)
{
  char s[100];

  //enum TStatus {stOff,stError,stRunning,stResize};
  
  switch(Status)
    {
    case stOff:
      InitializeStatusBar(hWndStatusbar,2);
      UpdateStatusBar("Serial port closed", 1, 0);
      break;
    case stError:
      InitializeStatusBar(hWndStatusbar,2);
      UpdateStatusBar("Unable to open serial port - check comm setup", 1, 0);
      break;
    case stRunning:
      InitializeStatusBar(hWndStatusbar,6);
      sprintf(s," COM%d",RegContents.ComPort);
      UpdateStatusBar(s, 1, 0);
      sprintf(s," %d",BaudRates[RegContents.Baud]);
      UpdateStatusBar(s, 2, 0);
      sprintf(s," N-8-1");
      UpdateStatusBar(s, 3, 0);
      sprintf(s," %s Flow Control",RegContents.HdwFlow ? "Hardware" : "No");
      UpdateStatusBar(s, 4, 0);
      sprintf(s," %s CR/LF",RegContents.CrLf ? "UNIX" : "DOS");
      UpdateStatusBar(s, 5, 0);
      break;
    case stResize:  // resize the bar only - 5 panes for serial on, 2 panes for serial off
      InitializeStatusBar(hWndStatusbar,SerialPortIsOpen() ? 6 : 2);
      break;
    }
}

/**
   Draws a single character onto the main window's device context.  Uses the application's
   font as initialized by WinMain.
   @param x X location of character.
   @param y Y location of character.
   @param ch The character to draw.
*/
void DrawChar(int x,int y,char ch)
{
  // draw one char at x and y
  HDC DC;
  
  x = x * CharWd + Margin;
  y = y * CharHt + Margin;
  
  DC = GetDC(hwndMain);
  SelectObject(DC,font);
  
  TextOut(DC,x,y,&ch,1);
  
  ReleaseDC(hwndMain,DC);
}

/**
   Centers a window on screen.  Used for all dialogs.
   @param hwnd Handle to window to be centered.
*/
void CenterWindow(HWND hwnd)
{
  RECT WinRect,ScrnRect;
  
  // get window dimensions
  GetWindowRect(hwnd,&WinRect);
  // get screen dimensions
  SystemParametersInfo(SPI_GETWORKAREA,0,&ScrnRect,0);
  
  // calculate window in center of screen
  SetWindowPos(hwnd,0,
               (ScrnRect.right - (WinRect.right - WinRect.left))/2,
               (ScrnRect.bottom - (WinRect.bottom - WinRect.top))/2,
               0,0,SWP_NOSIZE|SWP_NOZORDER);
}

void debugtext(char *text)
{
/*   while (*text) */
/*     AddChar(*text++); */

  MessageBox(NULL,text,"Debug",MB_OK|MB_ICONSTOP);
}

/**
   Copies contents of terminal display to clipboard.  Uses the Win32 GlobalAlloc/GlobalLock
   functions because these are required by SetClipboardData.
   @param wnd Handle to display window.
*/
void CopyToClipboard(HWND wnd)
{
  int Count=0,i;                                  // byte count
  HGLOBAL mem;
  char *p;
  // copies lines to clipboard
  
  // count btyes need to alloc
  for(i=0;i<Lines->Count;i++)
    Count += strlen(Lines->Lines[i]) + 2;   // cr/lf added.
  Count += 10;    // just in case

  // alloc mem and copy data to mem
  mem = GlobalAlloc(GMEM_FIXED,Count);
  p = GlobalLock(mem);
  if (!p)
    {
      MessageBox(NULL,"Cannot allocate memory!\n","Error",MB_OK|MB_ICONSTOP);
      return;
    }
  
  for(i=0;i<Lines->Count;i++)
    {
      strcpy(p,Lines->Lines[i]);
      p += strlen(Lines->Lines[i]);
      *(p++) = '\r';
      *(p++) = '\n';
    }
  *p = 0; // asciiz terminate

  if (!OpenClipboard(wnd))
    {
      MessageBox(NULL,"Cannot open clipboard.","Error",MB_OK|MB_ICONSTOP);
      return;
    }
  EmptyClipboard();

  SetClipboardData(CF_TEXT,mem);
  CloseClipboard();
  GlobalUnlock(mem);
}

/**
   Pastes data from clipboard to serial port.
   @param wnd Handle to display window.   
*/
void PasteFromClipboard(HWND wnd)
{
  HGLOBAL Mem;
  char *buf,*clip,*p;
  int Count;
  
  if (!OpenClipboard(wnd)) return;
  Mem = GetClipboardData(CF_TEXT);
  
  if (!Mem)
    {
      CloseClipboard();
      return;
    }
  
  // copy memory to local buffer
  Count = GlobalSize(Mem);
  if (!Count)
    {
      CloseClipboard();
      return;
    }
  clip = GlobalLock(Mem);
  buf = malloc(Count);
  CopyMemory(buf,clip,Count);
  GlobalUnlock(Mem);
  CloseClipboard();
  
  // send chars to serial port
  p = buf;
  while(*p)
    {
      PutSerialChar(*p++);
      TxFlag = TRUE;          // signal LED to go on.
    }
  free(buf);
}

/**
   Show the popup menu for display window.
   @param wnd Handle to display window.
*/
void ShowMenu(HWND wnd)
{
  POINT P;
  
  GetCursorPos(&P);
  TrackPopupMenuEx(GetSubMenu(PopupMenu,0),TPM_LEFTBUTTON|TPM_RIGHTBUTTON,P.x,P.y,wnd,NULL);
}


/**
   Clears all data from display.
*/
void ClearScreen(void)
{
  int x;
  
  SetCursX(Lines,0);
  SetCursY(Lines,0);
  for(x=0;x<Lines->Count;x++)
    Lines->Lines[x][0] = 0;
  InvalidateRect(hwndMain,NULL,FALSE);
}

/**
   Saves data displayed to file.  Does not scrollback.
*/
void SaveFile(void)
{
  // save screen lines to file
  OPENFILENAME OpenStruct;
  FILE *file;
  int i;
  
  // open file
  memset(&OpenStruct,0,sizeof(OPENFILENAME));
  OpenStruct.lStructSize = sizeof(OPENFILENAME);
  OpenStruct.hwndOwner = hwndMain;
  OpenStruct.hInstance = NULL;
  OpenStruct.lpstrFilter = "";
  OpenStruct.lpstrCustomFilter = NULL;
  OpenStruct.nMaxCustFilter = 0;
  OpenStruct.nFilterIndex = 0;
  OpenStruct.lpstrFile = FileName;
  OpenStruct.nMaxFile = 300;
  OpenStruct.lpstrFileTitle = NULL;
  OpenStruct.nMaxFileTitle = 0;
  OpenStruct.lpstrInitialDir = NULL;
  OpenStruct.lpstrTitle = NULL;
  OpenStruct.Flags = 0;
  OpenStruct.nFileOffset = 0;
  OpenStruct.nFileExtension = 0;
  OpenStruct.lpstrDefExt = "";//".cfg";
  OpenStruct.lCustData = 0;
  OpenStruct.lpfnHook = NULL;
  OpenStruct.lpTemplateName = NULL;
  
  if (!GetSaveFileName(&OpenStruct))
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }
  
  // load from file
  file = fopen(FileName,"wb");
  if (!file)
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }
  
  // write the screen contents to file
  for (i=0;i<Lines->Count;i++)
    {
      fputs(Lines->Lines[i],file);
      fputs("\n",file);
    }
    
  fclose(file);

}

/**
   Start logging input data to file.  Gets the filename from user first.
*/
void StartLog(void)
{
  OPENFILENAME OpenStruct;
  
  if (LogFile)
    return;
  
  // open file
  memset(&OpenStruct,0,sizeof(OPENFILENAME));
  OpenStruct.lStructSize = sizeof(OPENFILENAME);
  OpenStruct.hwndOwner = hwndMain;
  OpenStruct.hInstance = NULL;
  OpenStruct.lpstrFilter = "";
  OpenStruct.lpstrCustomFilter = NULL;
  OpenStruct.nMaxCustFilter = 0;
  OpenStruct.nFilterIndex = 0;
  OpenStruct.lpstrFile = FileName;
  OpenStruct.nMaxFile = 300;
  OpenStruct.lpstrFileTitle = NULL;
  OpenStruct.nMaxFileTitle = 0;
  OpenStruct.lpstrInitialDir = NULL;
  OpenStruct.lpstrTitle = NULL;
  OpenStruct.Flags = 0;
  OpenStruct.nFileOffset = 0;
  OpenStruct.nFileExtension = 0;
  OpenStruct.lpstrDefExt = "";
  OpenStruct.lCustData = 0;
  OpenStruct.lpfnHook = NULL;
  OpenStruct.lpTemplateName = NULL;
    
  if (!GetSaveFileName(&OpenStruct))
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }
    
  // open file
  LogFile = fopen(FileName,"wb");
  if (!LogFile)
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }

}

/**
   Closes the log file.
*/
void EndLog(void)
{

  if (LogFile)
    fclose(LogFile);
  LogFile = NULL;
}

/**
   Sends an input file to serial port.  Gets filename from user.
*/
void SendFile(void)
{

  OPENFILENAME OpenStruct;
  FILE *file;
  int ch;

  // open file
  memset(&OpenStruct,0,sizeof(OPENFILENAME));
  OpenStruct.lStructSize = sizeof(OPENFILENAME);
  OpenStruct.hwndOwner = hwndMain;
  OpenStruct.hInstance = NULL;
  OpenStruct.lpstrFilter = "";// "Cfg files\0" "*.cfg\0" "\0" "\0";
  OpenStruct.lpstrCustomFilter = NULL;
  OpenStruct.nMaxCustFilter = 0;
  OpenStruct.nFilterIndex = 0;
  OpenStruct.lpstrFile = FileName;
  OpenStruct.nMaxFile = 300;
  OpenStruct.lpstrFileTitle = NULL;
  OpenStruct.nMaxFileTitle = 0;
  OpenStruct.lpstrInitialDir = NULL;
  OpenStruct.lpstrTitle = NULL;
  OpenStruct.Flags = 0;
  OpenStruct.nFileOffset = 0;
  OpenStruct.nFileExtension = 0;
  OpenStruct.lpstrDefExt = "";//".cfg";
  OpenStruct.lCustData = 0;
  OpenStruct.lpfnHook = NULL;
  OpenStruct.lpTemplateName = NULL;

  if (!GetOpenFileName(&OpenStruct))
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }

  // load from file
  file = fopen(FileName,"rb");
  if (!file)
    {
      MessageBox(NULL,"Can't open file","Error",MB_OK|MB_ICONSTOP);
      return;
    }

  // read the chars, send to port
  while ((ch = fgetc(file)) >= 0)
    {
      PutSerialChar(ch);
      TxFlag = TRUE;          // signal LED to go on.
    }

  fclose(file);
}


/**
   Add a character to the binary viewer window.
*/
void AddBinaryChar(char ch)
{
    static int ndx;
    char str[5];

    if (!hwndBin)
        return;

    // Print 16 chars wide
    if (++ndx == 16)
    {
        SendMessage(hwndBinEdit, WM_CHAR, '\r', 0);
        ndx = 0;
    }
    // Convert to ch-ch-space
    sprintf(str, "%02X ", ch);
    SendMessage(hwndBinEdit, WM_CHAR, str[0], 0);
    SendMessage(hwndBinEdit, WM_CHAR, str[1], 0);
    SendMessage(hwndBinEdit, WM_CHAR, str[2], 0);

}

/**
   @}
*/
