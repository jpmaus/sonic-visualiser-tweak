/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2018 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_SYSTEM_H
#define SV_SYSTEM_H

#include "base/Debug.h"

#ifdef _WIN32

#include <windows.h>
#include <malloc.h>
#include <process.h>
#include <math.h>

#define MLOCK(a,b)   1
#define MUNLOCK(a,b) 1
#define MUNLOCK_SAMPLEBLOCK(a) 1
#define MUNLOCKALL() 1

extern void SystemMemoryBarrier();
#define MBARRIER()   SystemMemoryBarrier()

#define DLOPEN(a,b)  LoadLibrary((a).toStdWString().c_str())
#define DLSYM(a,b)   GetProcAddress((HINSTANCE)(a),(b))
#define DLCLOSE(a)   (!FreeLibrary((HINSTANCE)(a)))
#define DLERROR()    ""

#define PLUGIN_GLOB  "*.dll"
#define PATH_SEPARATOR ';'

// The default Vamp plugin path is obtained from a function in the
// Vamp SDK (Vamp::PluginHostAdapter::getPluginPath).

// At the time of writing, at least, the vast majority of LADSPA
// plugins on Windows hosts will have been put there for use in
// Audacity.  It's a bit of a shame that Audacity uses its own Program
// Files directory for plugins that any host may want to use... maybe
// they were just following the example of VSTs, which are usually
// found in Steinberg's Program Files directory.  Anyway, we can
// greatly increase our chances of picking up some LADSPA plugins by
// default if we include the Audacity plugin location as well as an
// (imho) more sensible place.

#define DEFAULT_LADSPA_PATH "%ProgramFiles%\\LADSPA Plugins;%ProgramFiles%\\Audacity\\Plug-Ins"
#define DEFAULT_DSSI_PATH   "%ProgramFiles%\\DSSI Plugins"

#define getpid _getpid

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#ifdef _MSC_VER
extern "C" {
void usleep(unsigned long usec);
}
#else
#include <unistd.h>
#endif

extern "C" {
int gettimeofday(struct timeval *p, void *tz);
}

#define ISNAN std::isnan
#define ISINF std::isinf

#else

#include <sys/mman.h>
#include <dlfcn.h>
#include <stdio.h> // for perror
#include <cmath>
#include <unistd.h> // sleep + usleep primarily

#define MLOCK(a,b)   ::mlock((a),(b))
#define MUNLOCK(a,b) (::munlock((a),(b)) ? (::perror("munlock failed"), 0) : 0)
#define MUNLOCK_SAMPLEBLOCK(a) do { if (!(a).empty()) { const float &b = *(a).begin(); MUNLOCK(&b, (a).capacity() * sizeof(float)); } } while(0);
//#define MLOCK(a,b)   1
//#define MUNLOCK(a,b) 1
//#define MUNLOCK_SAMPLEBLOCK(a) 1

#define DLOPEN(a,b)  dlopen((a).toStdString().c_str(),(b))
#define DLSYM(a,b)   dlsym((a),(b))
#define DLCLOSE(a)   dlclose((a))
#define DLERROR()    dlerror()

#include <cmath>
#define ISNAN std::isnan
#define ISINF std::isinf

#ifdef __APPLE__

#define PLUGIN_GLOB  "*.dylib *.so"
#define PATH_SEPARATOR ':'

#define DEFAULT_LADSPA_PATH "$HOME/Library/Audio/Plug-Ins/LADSPA:/Library/Audio/Plug-Ins/LADSPA"
#define DEFAULT_DSSI_PATH   "$HOME/Library/Audio/Plug-Ins/DSSI:/Library/Audio/Plug-Ins/DSSI"

#define MUNLOCKALL() 1

#include <libkern/OSAtomic.h>
#define MBARRIER() OSMemoryBarrier()

#else 

#ifdef sun
#undef MLOCK
#undef MUNLOCK
#define MLOCK(a,b) ::mlock((char *)a,b)
#define MUNLOCK(a,b) ::munlock((char *)a,b)
#ifdef __SUNPRO_CC
#undef ISNAN
#undef ISINF
#define ISNAN(x) ((x)!=(x))
#define ISINF(x) 0
#endif
#endif

#define PLUGIN_GLOB  "*.so"
#define PATH_SEPARATOR ':'

#define DEFAULT_LADSPA_PATH "$HOME/ladspa:$HOME/.ladspa:/usr/local/lib/ladspa:/usr/lib/ladspa"
#define DEFAULT_DSSI_PATH "$HOME/dssi:$HOME/.dssi:/usr/local/lib/dssi:/usr/lib/dssi"

#define MUNLOCKALL() ::munlockall()

#if (__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define MBARRIER() __sync_synchronize()
#else
extern void SystemMemoryBarrier();
#define MBARRIER() SystemMemoryBarrier()
#endif

#endif /* ! __APPLE__ */

#endif /* ! _WIN32 */

enum ProcessStatus { ProcessRunning, ProcessNotRunning, UnknownProcessStatus };
extern ProcessStatus GetProcessStatus(int pid);

// Return a vague approximation to the number of free megabytes of real memory.
// Return -1 if unknown. (Hence signed args.) Note that this could be more than
// is actually addressable, e.g. for a 32-bit process on a 64-bit system.
extern void GetRealMemoryMBAvailable(ssize_t &available, ssize_t &total);

// Return a vague approximation to the number of free megabytes of
// disc space on the partition containing the given path.  Return -1
// if unknown. (Hence signed return type)
extern ssize_t GetDiscSpaceMBAvailable(const char *path);

extern void StoreStartupLocale();
extern void RestoreStartupLocale();

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern double mod(double x, double y);
extern float modf(float x, float y);

extern double princarg(double a);
extern float princargf(float a);

#ifdef USE_POW_NO_F
#define powf pow
#endif

/** Return the value of the given environment variable by reference.
    Return true if successfully retrieved, false if unset or on error.
    Both the variable name and the returned value are UTF-8 encoded.
*/
extern bool getEnvUtf8(std::string variable, std::string &value);

/** Set the value of the given environment variable.
    Return true if successfully set, false on error.
    Both the variable name and the value must be UTF-8 encoded.
*/
extern bool putEnvUtf8(std::string variable, std::string value);

#endif /* ! _SYSTEM_H_ */


