// version.h
//R00k modified from Fuh NQ = NetQuake

#define	NQ_VERSION	1.09
#ifdef _WIN32
#define NQ_PLATFORM	"Win32"
#endif
#ifdef __APPLE__
#define NQ_PLATFORM "MacOSX"
#endif
#ifndef NQ_PLATFORM
#define NQ_PLATFORM	"Linux"
#endif

#ifdef GLQUAKE
#define NQ_RENDERER	"GL"
#else
#define NQ_RENDERER "Soft"
#endif

int build_number (void);
void Host_Version_f (void);
char *VersionString (void);
