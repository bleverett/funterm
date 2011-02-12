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
  $Id: funtermres.h,v 1.4 2010/04/01 15:19:45 blakelev Exp $
  $Revision: 1.4 $
  $Date: 2010/04/01 15:19:45 $
 */
/**
   @file funtermres.h This file contains ID codes used for dispatching
   windows events.  Each of these codes corresponds to an event in the
   application, and is passed from dialogs to event handlers.
   @addtogroup term
 */

#define	IDACCEL	100
#define	IDD_ABOUT	101
#define	IDM_CONFIG	200
#define	IDM_STARTCOMM	210
#define IDM_COPY 	211
#define IDM_PASTE	212
#define IDM_CLEAR       213
#define IDM_BINARY      214
#define IDM_SEND        220
#define IDM_SAVE        230
#define IDM_CRLF        235
#define IDM_LOG_START   240
#define IDM_LOG_END     250
#define	IDM_EXIT	300
#define	IDD_CONFIG	400
#define IDD_BINARY      410
#define ID_COMPORT      420
#define	ID_SPEEDGB	406
#define	ID_BAUD         407
#define	ID_CBOPEN	415
#define	ID_CBFLOW	416
#define	IDM_ABOUT	500
#define	IDMAINMENU	600
#define IDPOPUPMENU	601
#define	IDAPPLICON	710
#define	IDB_GRNLEDON	720
#define	IDB_GRNLEDOFF	721
#define	IDB_REDLEDON	722
#define	IDB_REDLEDOFF	723
#define	IDAPPLCURSOR	810
#define	IDS_FILEMENU	2000
#define	IDS_HELPMENU	2010
#define	IDS_SYSMENU	2030
#define	IDM_STATUSBAR	3000
