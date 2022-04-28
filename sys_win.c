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
// sys_win.c -- Win32 system interface code

#include "quakedef.h"
#include "winquake.h"
#include "resource.h"
#include "conproc.h"
#include <limits.h>
#include <errno.h>
#include <direct.h>		// _mkdir
#include <conio.h>		// _putch

#include <fcntl.h>
#include <sys/stat.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define MINIMUM_WIN_MEMORY	0x8000000	// 128MB //this is allocated for low hunk mem (default)
#define MAXIMUM_WIN_MEMORY	0x10000000	// 256MB // this is allocated for total mem if no command-line parm defined.

#define CONSOLE_ERROR_TIMEOUT	60.0	// # of seconds to wait on Sys_Error running
										// dedicated before exiting
#define PAUSE_SLEEP			50		// sleep time on pause or minimization
#define NOT_FOCUS_SLEEP			20		// sleep time when not focus

int		starttime;
qboolean	ActiveApp, Minimized;
qboolean	WinNT, Win2K, WinXP, Win2K3, WinVISTA, Win7;

static		int	lowshift;
qboolean	isDedicated;
static 		qboolean	sc_return_on_enter = false;
HANDLE		hinput, houtput;

static	char	*tracking_tag = "Clams & Mooses";

static	HANDLE	tevent;
static	HANDLE	hFile;
static	HANDLE	heventParent;
static	HANDLE	heventChild;

static	void *memBasePtr = 0;//Reckless

typedef HRESULT (WINAPI *SetProcessDpiAwarenessFunc)(_In_ DWORD value);

qboolean OnChange_sys_highpriority (cvar_t *, char *, qboolean *);
cvar_t sys_highpriority = {"sys_highpriority", "0", 1, false, OnChange_sys_highpriority};

//void MaskExceptions (void);
void Sys_InitDoubleTime (void);
void Sys_PushFPCW_SetHigh (void);
void Sys_PopFPCW (void);

// mh - handly little buffer for putting any short-term memory requirements into
// avoids the need to malloc an array, then free it after, for a lot of setup and loading code
#ifdef USESHADERS
byte scratchbuf[SCRATCHBUF_SIZE];
#endif


// WinKeys system hook (from ezQuake)

static HHOOK WinKeyHook;
static qboolean WinKeyHook_isActive;

LRESULT CALLBACK LLWinKeyHook(int Code, WPARAM wParam, LPARAM lParam) 
{
	PKBDLLHOOKSTRUCT p;

	p = (PKBDLLHOOKSTRUCT) lParam;

	if (ActiveApp) 
	{
		switch(p->vkCode) 
		{
			case VK_LWIN: Key_Event (K_LWIN, !(p->flags & LLKHF_UP)); return 1;
			case VK_RWIN: Key_Event (K_RWIN, !(p->flags & LLKHF_UP)); return 1;
			case VK_APPS: Key_Event (K_MENU, !(p->flags & LLKHF_UP)); return 1;
			case VK_SNAPSHOT: Key_Event (K_PRINTSCR, !(p->flags & LLKHF_UP)); return 1;
		}
	}
	return CallNextHookEx(NULL, Code, wParam, lParam);
}

qboolean OnChange_sys_disableWinKeys(cvar_t *var, char *string) 
{
	if (Q_atof(string))
	{
		if (!WinKeyHook_isActive) 
		{
			if ((WinKeyHook = SetWindowsHookEx(13, LLWinKeyHook, global_hInstance, 0))) 
			{
				WinKeyHook_isActive = true;
			} 
			else 
			{
				Con_Printf("Failed to install winkey hook.\n");
				return true;
			}
		}
	} 
	else 
	{
		if (WinKeyHook_isActive) 
		{
			UnhookWindowsHookEx(WinKeyHook);
			WinKeyHook_isActive = false;
		}
	}	
	return false;
}

cvar_t	sys_disableWinKeys = {"sys_disableWinKeys", "1", true, false, OnChange_sys_disableWinKeys};



/*
===============================================================================

SYNCHRONIZATION

===============================================================================
*/

int	hlock;
_CRTIMP int __cdecl _open(const char *, int, ...);
_CRTIMP int __cdecl _close(int);

/*
================
Sys_GetLock
================
*/
void Sys_GetLock (void)
{
	int	i;

	for (i=0 ; i<10 ; i++)
	{
		hlock = _open (va("%s/lock.dat", com_gamedir), _O_CREAT | _O_EXCL, _S_IREAD | _S_IWRITE);
		if (hlock != -1)
			return;
		Sleep (1000);
	}

	Sys_Printf ("Warning: could not open lock; using crowbar\n");
}

/*
================
Sys_ReleaseLock
================
*/
void Sys_ReleaseLock (void)
{
	if (hlock != -1)
		_close (hlock);
	_unlink (va("%s/lock.dat", com_gamedir));
}

/*
===============================================================================

FILE IO

===============================================================================
*/

#define	MAX_HANDLES		99

FILE	*sys_handles[MAX_HANDLES];

int findhandle (void)
{
	int		i;
	
	for (i=1 ; i<MAX_HANDLES ; i++)
		if (!sys_handles[i])
			return i;
	Sys_Error ("out of handles");
	return -1;
}

/*
================
filelength
================
*/
int filelength (FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int Sys_FileOpenAppend (char *path) //MH
{
   FILE   *f;
   int      i;

   i = findhandle ();
   f = fopen (path, "ab");

   // change this Sys_Error to something that works in your code
   if (!f)
      Sys_Error ("Error opening %s: %s", path, strerror (errno));

   sys_handles[i] = f;
   return i;
}

int Sys_FileOpenRead (char *path, int *hndl)
{
	FILE	*f;
	int		i, retval;

	i = findhandle ();

	f = fopen(path, "rb");

	if (!f)
	{
		*hndl = -1;
		retval = -1;
	}
	else
	{
		sys_handles[i] = f;
		*hndl = i;
		retval = filelength(f);
	}

	return retval;
}

int Sys_FileOpenWrite (char *path)
{
	FILE	*f;
	int		i;

	i = findhandle ();

	f = fopen(path, "wb");
	if (!f)
		Sys_Error ("Error opening %s: %s", path,strerror(errno));
	sys_handles[i] = f;
	
	return i;
}

void Sys_FileClose (int handle)
{
	fclose (sys_handles[handle]);
	sys_handles[handle] = NULL;
}

void Sys_FileSeek (int handle, int position)
{
	fseek (sys_handles[handle], position, SEEK_SET);
}

int Sys_FileRead (int handle, void *dest, int count)
{
	int	x;
	x = fread (dest, 1, count, sys_handles[handle]);
	return x;
}

int Sys_FileWrite (int handle, void *data, int count)
{
	int	x;
	x = fwrite (data, 1, count, sys_handles[handle]);
	return x;
}

int Sys_FileTime (char *path)
{
	FILE	*f;
	int	retval;
#ifndef GLQUAKE
	int	t;

	t = VID_ForceUnlockedAndReturnState ();
#endif

	if ((f = fopen(path, "rb")))
	{
		fclose (f);
		retval = 1;
	}
	else
	{
		retval = -1;
	}

#ifndef GLQUAKE
	VID_ForceLockState (t);
#endif
	return retval;
}

void Sys_mkdir (char *path)
{
	_mkdir (path);
}

/*
===============================================================================

SYSTEM IO

===============================================================================
*/

/*
================
Sys_MakeCodeWriteable
================
*/
void Sys_MakeCodeWriteable (unsigned long startaddr, unsigned long length)
{
	DWORD  flOldProtect;

	if (!VirtualProtect((LPVOID)startaddr, length, PAGE_READWRITE, &flOldProtect))
		Sys_Error ("Protection change failed\n");
}

/*
================
Sys_Init
================
*/

void Sys_Init (void)
{
 	NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
	OSVERSIONINFOEXW osInfo;

	*(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");

	if (NULL != RtlGetVersion)
	{
		osInfo.dwOSVersionInfoSize = sizeof(osInfo);
		RtlGetVersion(&osInfo);
		if ((osInfo.dwMajorVersion < 4) || (osInfo.dwPlatformId == VER_PLATFORM_WIN32s))
		Sys_Error ("Qrack requires at least Win95 or greater.");

	}
	// Use raw resolutions, not scaled (from ezQuake)
	{
		HMODULE lib = LoadLibrary("Shcore.dll");
		if (lib != NULL) 
		{
			SetProcessDpiAwarenessFunc SetProcessDpiAwareness;

			SetProcessDpiAwareness = (SetProcessDpiAwarenessFunc) GetProcAddress(lib, "SetProcessDpiAwareness");
			if (SetProcessDpiAwareness != NULL) 
			{
				SetProcessDpiAwareness(2);
			}
			FreeLibrary(lib);
		}
	}
}

void Sys_Error (char *error, ...)
{
	va_list		argptr;
	char		text[1024], text2[1024];
	char		*text3 = "Press Enter to exit\n";
	char		*text4 = "***********************************\n";
	char		*text5 = "\n";
	DWORD		dummy;
	double		starttime;
	static	int	in_sys_error0 = 0, in_sys_error1 = 0, in_sys_error2 = 0, in_sys_error3 = 0;

	if (!in_sys_error3)
	{
		in_sys_error3 = 1;
#ifndef GLQUAKE
		VID_ForceUnlockedAndReturnState ();
#endif
	}

	va_start (argptr, error);
	vsnprintf (text, sizeof(text), error, argptr);
	va_end (argptr);

	if (isDedicated)
	{
		va_start (argptr, error);
		vsnprintf (text, sizeof(text), error, argptr);
		va_end (argptr);

		sprintf (text2, "ERROR: %s\n", text);
		WriteFile (houtput, text5, strlen(text5), &dummy, NULL);
		WriteFile (houtput, text4, strlen(text4), &dummy, NULL);
		WriteFile (houtput, text2, strlen(text2), &dummy, NULL);
		WriteFile (houtput, text3, strlen(text3), &dummy, NULL);
		WriteFile (houtput, text4, strlen(text4), &dummy, NULL);

		starttime = Sys_DoubleTime ();
		sc_return_on_enter = true;	// so Enter will get us out of here
	}
	else
	{
	// switch to windowed so the message box is visible, unless we already
	// tried that and failed
		if (!in_sys_error0)
		{
			in_sys_error0 = 1;
			VID_SetDefaultMode ();
			MessageBox (NULL, text, "Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
		else
		{
			MessageBox (NULL, text, "Double Quake Error", MB_OK | MB_SETFOREGROUND | MB_ICONSTOP);
		}
	}

	if (!in_sys_error1)
	{
		in_sys_error1 = 1;
		Host_Shutdown ();
	}

	// shut down QHOST hooks if necessary
	if (!in_sys_error2)
	{
		in_sys_error2 = 1;
		DeinitConProc ();
	}

	if (memBasePtr)//Reckless
	{
		VirtualFree(memBasePtr, 0, MEM_RELEASE);
	}

	exit (1);
}

void Sys_Printf (char *fmt, ...)
{
	va_list	argptr;
	char	text[2048];
	DWORD	dummy;
	
	if (isDedicated)
	{
		va_start (argptr, fmt);
		vsnprintf (text, sizeof(text), fmt, argptr);
		va_end (argptr);

		WriteFile (houtput, text, strlen(text), &dummy, NULL);

		// joe, from ProQuake: rcon (64 doesn't mean anything special,
		// but we need some extra space because NET_MAXMESSAGE == RCON_BUFF_SIZE)
		if (rcon_active  && (rcon_message.cursize < rcon_message.maxsize - strlen(text) - 64))
		{
			rcon_message.cursize--;
			MSG_WriteString (&rcon_message, text);
		}
	}
}

void Sys_Quit (void)
{
#ifndef GLQUAKE
	VID_ForceUnlockedAndReturnState ();
#endif
	
	Host_Shutdown ();

	if (tevent)
		CloseHandle (tevent);

	if (isDedicated)
		FreeConsole ();

// shut down QHOST hooks if necessary
	DeinitConProc ();

	if (memBasePtr)//Reckless
	{
		VirtualFree(memBasePtr, 0, MEM_RELEASE);
	}
	
	exit (0);
}

int Sys_SetPriority(int priority) 
{
    DWORD p;

	switch (priority) 
	{
		case 0:	p = IDLE_PRIORITY_CLASS; break;
		case 1:	p = NORMAL_PRIORITY_CLASS; break;
		case 2:	p = HIGH_PRIORITY_CLASS; break;
		case 3:	p = REALTIME_PRIORITY_CLASS; break;
		default: return 0;
	}

	return SetPriorityClass(GetCurrentProcess(), p);
}

qboolean OnChange_sys_highpriority (cvar_t *var, char *s, qboolean *cancel) 
{
	int ok, q_priority;
	char *desc;
	float priority;

	priority = Q_atof(s);
	if (priority == 1) 
	{
		q_priority = 2;
		desc = "high";
	} 
	else if (priority == -1) 
	{
		q_priority = 0;
		desc = "low";
	} 
	else 
	{
		q_priority = 1;
		desc = "normal";
	}

	if (!(ok = Sys_SetPriority(q_priority))) 
	{
		Con_Printf("Changing process priority failed\n");
		*cancel = true;
		return 0;
	}

	Con_Printf("Process priority set to %s\n", desc);
	return 1;
}

static __int64 qpcfreq = 0;
static qboolean	hwtimer = false;
/*
================
Sys_InitDoubleTime
================
*/
void Sys_InitDoubleTime (void)
{
	__int64	freq;

	if (!COM_CheckParm("-nohwtimer") && QueryPerformanceFrequency ((LARGE_INTEGER *)&freq) && freq > 0)
	{
		// hardware timer available
		qpcfreq = (double)freq;
		hwtimer = true;
	}
	else
	{
		// make sure the timer is high precision, otherwise
		// NT gets 18ms resolution
		timeBeginPeriod (1);
	}
}

/*
================
Sys_DoubleTime	-- MH's high-performance/stable Sys_DoubleTime method. (ie, multi-core clock fix)
================

	We have two main methods of timing on Windows: QueryPerformanceCounter and timeGetTime. 
 QueryPerformanceCounter sucks because it's unstable on many PCs but it does have high resolution. 
 timeGetTime sucks because it's resolution - while quite high - isn't high enough for Quakes 72 FPS but it is stable on all PCs.
 So what I did was use timeGetTime as a solid baseline and QueryPerformanceCounter to fill in the sub-millisecond gaps
*/

double Sys_DoubleTime (void) 
{
	static DWORD starttime;
	static qboolean first = true;
	DWORD now;

	if (hwtimer) 
	{
	   static qboolean firsttime = true;
	   //static __int64 qpcfreq = 0;
	   static __int64 currqpccount = 0;
	   static __int64 lastqpccount = 0;
	   static double qpcfudge = 0;
	   DWORD currtime = 0;
	   static DWORD lasttime = 0;
	   static DWORD starttime = 0;

	   if (firsttime)
	   {
		  timeBeginPeriod (1);
		  starttime = lasttime = timeGetTime ();
		  QueryPerformanceFrequency ((LARGE_INTEGER *) &qpcfreq);
		  QueryPerformanceCounter ((LARGE_INTEGER *) &lastqpccount);
		  firsttime = false;
		  return 0;
	   }

	   // get the current time from both counters
	   currtime = timeGetTime ();
	   QueryPerformanceCounter ((LARGE_INTEGER *) &currqpccount);

	   if (currtime != lasttime)
	   {
		  // requery the frequency in case it changes (which it can on multicore machines)
		  QueryPerformanceFrequency ((LARGE_INTEGER *) &qpcfreq);

		  // store back times and calc a fudge factor as timeGetTime can overshoot on a sub-millisecond scale
		  qpcfudge = ((double) (currqpccount - lastqpccount) / (double) qpcfreq) - ((double) (currtime - lasttime) * 0.001);
		  lastqpccount = currqpccount;
		  lasttime = currtime;
	   }
	   else qpcfudge = 0;

	   // the final time is the base from timeGetTime plus an addition from QPC
	   return ((double) (currtime - starttime) * 0.001) + ((double) (currqpccount - lastqpccount) / (double) qpcfreq) + qpcfudge;
	}

	now = timeGetTime();

	if (first) 
	{
		first = false;
		starttime = now;
		return 0.0;
	}

	if (now < starttime) // Wrapped?
		return (now / 1000.0) + (LONG_MAX - starttime / 1000.0);

	if (now - starttime == 0)
		return 0.0;

	return (now - starttime) / 1000.0;   
}

char *Sys_ConsoleInput (void)
{
	int		dummy, ch, numread, numevents;
	static char	text[256];
	static int	len;
	INPUT_RECORD	recs[1024];

	if (!isDedicated)
		return NULL;

	for ( ; ; )
	{
		if (!GetNumberOfConsoleInputEvents(hinput, &numevents))
			Sys_Error ("Error getting # of console events");

		if (numevents <= 0)
			break;

		if (!ReadConsoleInput(hinput, recs, 1, &numread))
			Sys_Error ("Error reading console input");

		if (numread != 1)
			Sys_Error ("Couldn't read console input");

		if (recs[0].EventType == KEY_EVENT)
		{
			if (!recs[0].Event.KeyEvent.bKeyDown)
			{
				ch = recs[0].Event.KeyEvent.uChar.AsciiChar;

				switch (ch)
				{
				case '\r':
					WriteFile (houtput, "\r\n", 2, &dummy, NULL);

					if (len)
					{
						text[len] = 0;
						len = 0;
						return text;
					}
					else if (sc_return_on_enter)
					{
					// special case to allow exiting from the error handler on Enter
						text[0] = '\r';
						len = 0;
						return text;
					}
					break;

				case '\b':
					WriteFile (houtput, "\b \b", 3, &dummy, NULL);
					if (len)
						len--;
					break;

				default:
					if (ch >= ' ')
					{
						WriteFile (houtput, &ch, 1, &dummy, NULL);
						text[len] = ch;
						len = (len + 1) & 0xff;
					}
					break;
				}
			}
		}
	}

	return NULL;
}

void Sys_Sleep (void)
{
	Sleep (1);
}

void Sys_SendKeyEvents (void)
{
	MSG	msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
	// we always update if there are any event, even if we're paused
		scr_skipupdate = 0;

		if (!GetMessage(&msg, NULL, 0, 0))
			Sys_Quit ();

		TranslateMessage (&msg);
		DispatchMessage (&msg);
	}
}

#define	SYS_CLIPBOARD_SIZE	256

char *Sys_GetClipboardData (void)
{
	HANDLE		th;
	char		*cliptext, *s, *t;
	static	char	clipboard[SYS_CLIPBOARD_SIZE];

	if (!OpenClipboard(NULL))
		return NULL;

	if (!(th = GetClipboardData(CF_TEXT)))
	{
		CloseClipboard ();
		return NULL;
	}

	if (!(cliptext = GlobalLock(th)))
	{
		CloseClipboard ();
		return NULL;
	}

	s = cliptext;
	t = clipboard;
	while (*s && t - clipboard < SYS_CLIPBOARD_SIZE - 1 && *s != '\n' && *s != '\r' && *s != '\b')
		*t++ = *s++;
	*t = 0;

	GlobalUnlock (th);
	CloseClipboard ();

	return clipboard;
}


//Baker: copies given text to clipboard
void Sys_CopyToClipboard(char *text) 
{
	char *clipText;
	HGLOBAL hglbCopy;

	if (!OpenClipboard(NULL))
		return;

	if (!EmptyClipboard()) 
	{
		CloseClipboard();
		return;
	}

	if (!(hglbCopy = GlobalAlloc(GMEM_DDESHARE, strlen(text) + 1))) 
	{
		CloseClipboard();
		return;
	}

	if (!(clipText = GlobalLock(hglbCopy))) 
	{
		CloseClipboard();
		return;
	}

	strcpy((char *) clipText, text);
	GlobalUnlock(hglbCopy);
	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

void SleepUntilInput (int time)
{
	MsgWaitForMultipleObjects (1, &tevent, FALSE, time, QS_ALLINPUT);
}

extern void RestoreHWGamma (void);
LONG WINAPI ABANDONSHIP (LPEXCEPTION_POINTERS escape_pod)
{
	MessageBox (hwnd_dialog, "An error has occured forcing Qrack to shutdown.\n All of your current settings will be saved.","Qrack",MB_OK | MB_ICONSTOP);
	RestoreHWGamma();
	Sys_Quit();

	return EXCEPTION_EXECUTE_HANDLER;
}

HINSTANCE		global_hInstance;
int				global_nCmdShow;
char			*argv[MAX_NUM_ARGVS], *argv0;
static	char	*empty_string = "";
HWND			hwnd_dialog;

double sys_init_start_time, sys_init_end_time;

/*
==================
WinMain
==================
*/
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)

{
	int				t;//, i;
	static	char	cwd[1024];
	double			time, oldtime, newtime;
	quakeparms_t	parms;
	MEMORYSTATUS	lpBuffer;
	RECT			rect;
//	FILE			*fpak0;
//	char			fpaktest[1024];//, exeline[MAX_OSPATH];
//	char			*e;

	/* previous instances do not exist in Win32 */
	if (hPrevInstance)
		return 0;

	Sys_InitDoubleTime ();	

	if (developer.value)
		sys_init_start_time = Sys_DoubleTime ();

	SetUnhandledExceptionFilter(ABANDONSHIP); 

	global_hInstance = hInstance;
	global_nCmdShow = nCmdShow;

	lpBuffer.dwLength = sizeof(MEMORYSTATUS);
	GlobalMemoryStatus (&lpBuffer);

	if (!GetCurrentDirectory(sizeof(cwd), cwd))
		Sys_Error ("Couldn't determine current directory");

	if (cwd[strlen(cwd)-1] == '/' || cwd[strlen(cwd)-1] == '\\')
		cwd[strlen(cwd)-1] = 0;

/*////////////////////////////////////////////////////////////////////////////////////////
// Baker 3.76 - playing demos via file association

	Q_snprintfz (fpaktest, sizeof(fpaktest), "%s/id1/pak0.pak", cwd); // Baker 3.76 - Sure this isn't gfx.wad, but let's be realistic here

	if(!(i = GetModuleFileName(NULL, com_basedir, sizeof(com_basedir)-1)))
		Sys_Error("FS_InitFilesystemEx: GetModuleFileName failed");

	com_basedir[i] = 0; // ensure null terminator

	Q_snprintfz(exeline, sizeof(exeline), "%s \"%%1\"", com_basedir);

	// Strip to the bare path; needed for demos started outside Quake folder
	for (e = com_basedir+strlen(com_basedir)-1; e >= com_basedir; e--)
	{
		if (*e == '/' || *e == '\\')
		{
			*e = 0;
			break;
		}
	}

	Q_snprintfz (cwd, sizeof(cwd), "%s", com_basedir);

	if (fpak0 = fopen(fpaktest, "rb"))  
	{
		fclose (fpak0); // Pak0 found so close it; we have a valid directory
	} 
	else 
	{
		// Failed to find pak0.pak, use the dir the exe is in
		Q_snprintfz (cwd, sizeof(cwd), "%s", com_basedir);
	}
// End Baker 3.76
*/////////////////////////////////////////////////////////////////////////////
	parms.basedir = cwd;
	parms.argc = 1;
	argv[0] = empty_string;

	// joe, from ProQuake: isolate the exe name, eliminate quotes
	argv0 = GetCommandLine ();

	while (*lpCmdLine && (parms.argc < MAX_NUM_ARGVS))
	{
		while (*lpCmdLine && ((*lpCmdLine <= 32) || (*lpCmdLine > 126)))
			lpCmdLine++;

		if (*lpCmdLine)
		{
			argv[parms.argc] = lpCmdLine;
			parms.argc++;

			while (*lpCmdLine && ((*lpCmdLine > 32) && (*lpCmdLine <= 126)))
				lpCmdLine++;

			if (*lpCmdLine)
			{
				*lpCmdLine = 0;
				lpCmdLine++;
			}
		}
	}

	parms.argv = argv;

	COM_InitArgv (parms.argc, parms.argv);

	parms.argc = com_argc;
	parms.argv = com_argv;

	isDedicated = COM_CheckParm("-dedicated");

	if (!(isDedicated))
	{
		hwnd_dialog = CreateDialog (hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, NULL);//start in a window

		if (hwnd_dialog)
		{
			if (GetWindowRect(hwnd_dialog, &rect))
			{
				if (rect.left > (rect.top * 2))
				{
					SetWindowPos (hwnd_dialog, 0,(rect.left / 2) - ((rect.right - rect.left) / 2),rect.top, 0, 0,SWP_NOZORDER | SWP_NOSIZE);
				}
			}

			ShowWindow (hwnd_dialog, SW_SHOWDEFAULT);
			UpdateWindow (hwnd_dialog);
			SetForegroundWindow (hwnd_dialog);
		}
	}

	if (COM_CheckParm("-debugconsole"))
	{//Baker's debug console
		con_debugconsole = true;
		AllocConsole ();
		freopen("CONIN$", "rt", stdin);
		freopen("CONOUT$", "wt", stdout);
		freopen("CONOUT$", "wt", stderr);
		SetConsoleTitle("Debug Console");
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN|FOREGROUND_BLUE);
		fprintf (stderr, "Console initialized.\n");
	}

	// Take the greater of all the available memory or half the total memory,
	// but at least 12 Mb and no more than 32 Mb, unless they explicitly request otherwise
	parms.memsize = lpBuffer.dwAvailPhys;

	if (parms.memsize < MINIMUM_WIN_MEMORY)
		parms.memsize = MINIMUM_WIN_MEMORY;

	if (parms.memsize > MAXIMUM_WIN_MEMORY)
		parms.memsize = MAXIMUM_WIN_MEMORY;

	if ((t = COM_CheckParm("-heapsize")) && t + 1 < com_argc)
		parms.memsize = Q_atoi(com_argv[t+1]) * 1024;

	if ((t = COM_CheckParm("-mem")) && t + 1 < com_argc)
		parms.memsize = Q_atoi(com_argv[t+1]) * 1024 * 1024;

	if (!(parms.membase = VirtualAlloc(NULL, parms.memsize, MEM_COMMIT, PAGE_EXECUTE_READWRITE)))
	{
		Sys_Error ("Not enough memory free; check disk space\n");
	}

	if (!(tevent = CreateEvent(NULL, FALSE, FALSE, NULL)))
		Sys_Error ("Couldn't create event");

	memBasePtr = parms.membase;

	if (isDedicated)
	{
		if (!AllocConsole())
			Sys_Error ("Couldn't create dedicated server console");

		hinput = GetStdHandle (STD_INPUT_HANDLE);
		houtput = GetStdHandle (STD_OUTPUT_HANDLE);

	// give QHOST a chance to hook into the console
		if ((t = COM_CheckParm("-HFILE")) > 0)
		{
			if (t < com_argc)
				hFile = (HANDLE)Q_atoi(com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm("-HPARENT")) > 0)
		{
			if (t < com_argc)
				heventParent = (HANDLE)Q_atoi(com_argv[t+1]);
		}
			
		if ((t = COM_CheckParm("-HCHILD")) > 0)
		{
			if (t < com_argc)
				heventChild = (HANDLE)Q_atoi(com_argv[t+1]);
		}

		InitConProc (hFile, heventParent, heventChild);
	}

	Sys_Init ();

// because sound is off until we become active
	S_BlockSound ();
	
	Host_Init (&parms);

	oldtime = Sys_DoubleTime ();

	if (developer.value)
	{
		sys_init_end_time = oldtime;

		Con_Printf ("…");
		Con_Printf("System initialized in %2.3f seconds",(sys_init_end_time - sys_init_start_time));
		Con_Printf("…\n");
	}

	/* main window message loop */
	while (1)
	{
		if (isDedicated)
		{
			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;

			while (time < sys_ticrate.value)
			{				
				Sys_Sleep ();
				newtime = Sys_DoubleTime ();
				time = newtime - oldtime;
			}
		}
		else
		{ 
		// yield the CPU for a little while when paused, minimized, or not the focus
			if ((cl.paused && (!ActiveApp && !DDActive)) || Minimized || block_drawing)
			{
				SleepUntilInput (PAUSE_SLEEP);
				scr_skipupdate = 1;		// no point in bothering to draw
			}
			else if (!ActiveApp && !DDActive)
			{
				SleepUntilInput (NOT_FOCUS_SLEEP);
			}

			newtime = Sys_DoubleTime ();
			time = newtime - oldtime;
		}
	
		Host_Frame (time);
		oldtime = newtime;
	}

	if (memBasePtr)
	{
		VirtualFree(memBasePtr, 0, MEM_RELEASE);
	}
	// return success of application
	return TRUE;
}

