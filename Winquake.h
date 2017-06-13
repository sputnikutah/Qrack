/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// winquake.h: Win32-specific Quake header file

#ifdef _WIN32

#pragma warning (disable : 4229)  // mgraph gets this

#include <windows.h>

#if (_WIN32_WINNT >= 0x0400) || (_WIN32_WINDOWS > 0x0400)
#define WM_MOUSEWHEEL                   0x020A
#endif
#if (_WIN32_WINNT >= 0x0600)
#define WM_MOUSEHWHEEL                  0x020E
#endif

#ifndef MK_XBUTTON1
#define MK_XBUTTON1         0x0020
#define MK_XBUTTON2         0x0040
#endif

#ifndef MK_XBUTTON3
#define MK_XBUTTON3         0x0080
#define MK_XBUTTON4         0x0100
#endif

#ifndef MK_XBUTTON5
#define MK_XBUTTON5         0x0200
#endif

#ifndef WM_XBUTTONUP
#define WM_XBUTTONUP        0x020C
#define WM_XBUTTONDOWN      0x020B
#endif

#if ( _WIN32_WINNT < 0x0400 )
#define KF_UP		    0x8000
#define LLKHF_UP	    ( KF_UP >> 8 )
#endif

extern int mouse_buttons;

#ifndef SERVERONLY
#include <ddraw.h>
#include <dsound.h>
#ifndef GLQUAKE
#include "mgraph.h"
#endif
#endif

extern	HINSTANCE	global_hInstance;
extern	int		global_nCmdShow;

#ifndef SERVERONLY

extern LPDIRECTDRAW		lpDD;
extern qboolean			DDActive;
extern LPDIRECTDRAWSURFACE	lpPrimary;
extern LPDIRECTDRAWSURFACE	lpFrontBuffer;
extern LPDIRECTDRAWSURFACE	lpBackBuffer;
extern LPDIRECTDRAWPALETTE	lpDDPal;
extern LPDIRECTSOUND pDS;
extern LPDIRECTSOUNDBUFFER pDSBuf;

extern DWORD gSndBufSize;
//#define SNDBUFSIZE 65536

void	VID_LockBuffer (void);
void	VID_UnlockBuffer (void);

#endif

typedef enum {MS_WINDOWED, MS_FULLSCREEN, MS_FULLDIB, MS_UNINIT} modestate_t;

extern modestate_t	modestate;

extern HWND		mainwindow;
extern qboolean		ActiveApp, Minimized;

extern qboolean	WinNT, Win2K, WinXP, Win2K3, WinVISTA;

int VID_ForceUnlockedAndReturnState (void);
void VID_ForceLockState (int lk);

void IN_ShowMouse (void);
void IN_DeactivateMouse (void);
void IN_HideMouse (void);
void IN_ActivateMouse (void);
void IN_RestoreOriginalMouseState (void);
void IN_SetQuakeMouseState (void);
void IN_MouseEvent (int mstate);

extern qboolean	winsock_lib_initialized;

extern int	window_center_x, window_center_y;
extern RECT	window_rect;

extern qboolean	mouseinitialized;
extern HWND	hwnd_dialog;

extern HANDLE	hinput, houtput;

void IN_UpdateClipCursor (void);
void CenterWindow (HWND hWndCenter, int width, int height, BOOL lefttopjustify);

void S_BlockSound (void);
void S_UnblockSound (void);

void VID_SetDefaultMode (void);

#endif	//_WIN32
