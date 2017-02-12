
/*
Copyright (C) 2000	LordHavoc, Ender, R00k

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
#include "fmod.h"
#include "fmod_errors.h"

#include "quakedef.h"

#ifdef _WIN32
static	HINSTANCE fmod_handle = NULL;
#else
static	void	*fmod_handle = NULL;
#endif

static qboolean modplaying = false;
static qboolean sampleplaying = false;

FMUSIC_MODULE	*mod = NULL;
FSOUND_STREAM	*stream = NULL;

static signed char (F_API *qFSOUND_Init)(int, int, unsigned int);
static signed char (F_API *qFSOUND_SetBufferSize)(int);

static int (F_API *qFSOUND_GetMixer)(void);
static signed char (F_API *qFSOUND_SetMixer)(int);

static int (F_API *qFSOUND_GetError)(void);
static void (F_API *qFSOUND_Close)(void);

static FMUSIC_MODULE * (F_API *qFMUSIC_LoadSongEx)(const char *, int, int, unsigned int, const int *, int);
static signed char (F_API *qFMUSIC_FreeSong)(FMUSIC_MODULE *);
static signed char (F_API *qFMUSIC_PlaySong)(FMUSIC_MODULE *);

static signed char (F_API *qFSOUND_Stream_SetBufferSize)(int);      /* call this before opening streams, not after */
static FSOUND_STREAM * (F_API *qFSOUND_Stream_Open)(const char *, unsigned int, int, int);
static signed char (F_API *qFSOUND_Stream_Close)(FSOUND_STREAM *);                           
static int (F_API *qFSOUND_Stream_Play)(int channel, FSOUND_STREAM *);
static signed char (F_API *qFSOUND_Stream_Stop)(FSOUND_STREAM *);

qboolean fmod_loaded;

#ifdef _WIN32
#define FSOUND_GETFUNC(f, g) (qFSOUND_##f = (void *)GetProcAddress(fmod_handle, "_FSOUND_" #f #g))
#define FMUSIC_GETFUNC(f, g) (qFMUSIC_##f = (void *)GetProcAddress(fmod_handle, "_FMUSIC_" #f #g))
#else
#define FSOUND_GETFUNC(f, g) (qFSOUND_##f = (void *)dlsym(fmod_handle, "FSOUND_" #f))
#define FMUSIC_GETFUNC(f, g) (qFMUSIC_##f = (void *)dlsym(fmod_handle, "FMUSIC_" #f))
#endif

qboolean FMOD_LoadLibrary (void)
{
	fmod_loaded = false;

#ifdef _WIN32
	if (!(fmod_handle = LoadLibrary("qrack/fmod.dll")))
#else
	if (!(fmod_handle = dlopen("qrack/libfmod-3.73.so", RTLD_NOW)))
#endif
	{
		Con_Printf ("\x02" "FMOD module not found\n");
		goto fail;
	}

	FSOUND_GETFUNC(Init, @12);
	FSOUND_GETFUNC(SetBufferSize, @4);
	FSOUND_GETFUNC(Stream_SetBufferSize, @4);      /* call this before opening streams, not after */
	FSOUND_GETFUNC(GetMixer, @0);
	FSOUND_GETFUNC(Stream_Open, @16);
	FSOUND_GETFUNC(Stream_Close, @4);
	FSOUND_GETFUNC(Stream_Play, @4);
	FSOUND_GETFUNC(Stream_Stop, @4);
	FSOUND_GETFUNC(SetMixer, @4);
	FSOUND_GETFUNC(GetError, @0);
	FSOUND_GETFUNC(Close, @0);
	FMUSIC_GETFUNC(LoadSongEx, @24);
	FMUSIC_GETFUNC(FreeSong, @4);
	FMUSIC_GETFUNC(PlaySong, @4);
	
	fmod_loaded = qFSOUND_Init && qFSOUND_SetBufferSize && qFSOUND_GetMixer && qFSOUND_SetMixer && qFSOUND_GetError && qFSOUND_Close && qFMUSIC_LoadSongEx && qFMUSIC_FreeSong && qFMUSIC_PlaySong;

	if (!fmod_loaded)
	{
		Con_Printf ("\x02" "FMOD module not initialized\n");
		goto fail;
	}

	Con_Printf ("FMOD module initialized\n");
	return true;

fail:
	if (fmod_handle)
	{
#ifdef _WIN32
		FreeLibrary (fmod_handle);
#else
		dlclose (fmod_handle);
#endif
		fmod_handle = NULL;
	}
	return false;
}

void FMOD_Stop_f (void)
{
	if (modplaying)
	{
		qFMUSIC_FreeSong (mod);
		mod = NULL;
	}
	modplaying = false;
}

void FMOD_Play_f (void)
{
	char	modname[1024], *buffer;
	int	mark;

	Q_strncpyz (modname, Cmd_Argv(1), sizeof(modname));

	if (modplaying)
		FMOD_Stop_f ();

	if (strlen(modname) < 3)
	{
		Con_Print ("Usage: playmod <filename.ext>");
		return;
	}

	mark = Hunk_LowMark ();

	if (!(buffer = (char *)COM_LoadHunkFile(modname)))
	{
		Con_Printf ("ERROR: Couldn't open %s\n", modname);
		return;
	}

	mod = qFMUSIC_LoadSongEx (buffer, 0, com_filesize, FSOUND_LOADMEMORY, NULL, 0);

	Hunk_FreeToLowMark (mark);

	if (!mod)
	{
		Con_Printf ("%s\n", FMOD_ErrorString(qFSOUND_GetError()));
		return;
	}

	modplaying = true;
	qFMUSIC_PlaySong (mod);
}

void FMOD_Stop_Sample_f (void)
{
	if (modplaying)
	{
	   qFSOUND_Stream_Stop(stream);
	   qFSOUND_Stream_Close(stream);
	   stream = NULL;
	}
	modplaying = false;
}

//FIXME!!! I CANT SEEM TO GET FMOD TO READ MP3/OGG FILES!!!!
void FMOD_Play_Sample_f (void)
{
	char modname[1024], *buffer;
	int	 mark;

	Q_strncpyz (modname, Cmd_Argv(1), sizeof(modname));

	if (modplaying)
		FMOD_Stop_Sample_f ();

	if (strlen(modname) < 5)
	{
		Con_Print ("Usage: playsample <filename.ext>");
		return;
	}

	mark = Hunk_LowMark ();

	if (!(buffer = (char *)COM_LoadHunkFile(modname)))
	{
		Con_Printf ("ERROR: Couldn't open %s\n", modname);
		return;
	}
	
	stream = NULL;
	stream = FSOUND_Stream_Open(modname, FSOUND_NORMAL | FSOUND_2D | FSOUND_MPEGACCURATE | FSOUND_NONBLOCKING, 0, 0);
	
	Hunk_FreeToLowMark (mark);

	if (!stream)
	{
		Con_Printf ("%s: %s\n", FMOD_ErrorString(qFSOUND_GetError()), modname);
		return;
	}

	modplaying = true;

	qFSOUND_Stream_Play (FSOUND_FREE, stream);
}
void FMOD_Init (void)
{
	qFSOUND_SetBufferSize (300);
	qFSOUND_Stream_SetBufferSize(1000);

	if (!qFSOUND_Init(44100, 32, 0))
	{
		Con_Printf ("(!)%s\n", FMOD_ErrorString(qFSOUND_GetError()));
		return;
	}

	Cmd_AddCommand ("stopmod", FMOD_Stop_f);
	Cmd_AddCommand ("playmod", FMOD_Play_f);
	Cmd_AddCommand ("stopsample", FMOD_Stop_Sample_f);
	Cmd_AddCommand ("playsample", FMOD_Play_Sample_f);
}

void FMOD_Close (void)
{
	if (fmod_loaded)
		qFSOUND_Close ();
}

