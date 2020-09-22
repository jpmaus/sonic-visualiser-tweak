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

#include "System.h"

#include <QStringList>
#include <QString>

#include <stdint.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/statvfs.h>
#include <locale.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include <limits.h>
#include <cstdlib>

#include <iostream>

#ifdef __APPLE__
extern "C" {
    void *
    rpl_realloc (void *p, size_t n)
    {
        p = realloc(p, n);
        if (p == 0 && n == 0)
        {
            p = malloc(0);
        }
        return p;
    }
}
#endif

#ifdef _WIN32

extern "C" {

#ifdef _MSC_VER
    void usleep(unsigned long usec)
    {
        ::Sleep(usec / 1000);
    }
#endif

    int gettimeofday(struct timeval *tv, void * /* tz */)
    {
        union { 
            long long ns100;  
            FILETIME ft; 
        } now; 
    
        ::GetSystemTimeAsFileTime(&now.ft); 
        tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL); 
        tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL); 
        return 0;
    }

}

#endif

ProcessStatus
GetProcessStatus(int pid)
{
#ifdef _WIN32
    HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!handle) {
        return ProcessNotRunning;
    } else {
        CloseHandle(handle);
        return ProcessRunning;
    }
#else
    if (kill(getpid(), 0) == 0) {
        if (kill(pid, 0) == 0) {
            return ProcessRunning;
        } else {
            return ProcessNotRunning;
        }
    } else {
        return UnknownProcessStatus;
    }
#endif
}

#ifdef _WIN32
/*  MEMORYSTATUSEX is missing from older Windows headers, so define a
    local replacement.  This trick from MinGW source code.  Ugh */
typedef struct
{
    DWORD dwLength;
    DWORD dwMemoryLoad;
    DWORDLONG ullTotalPhys;
    DWORDLONG ullAvailPhys;
    DWORDLONG ullTotalPageFile;
    DWORDLONG ullAvailPageFile;
    DWORDLONG ullTotalVirtual;
    DWORDLONG ullAvailVirtual;
    DWORDLONG ullAvailExtendedVirtual;
} lMEMORYSTATUSEX;
typedef BOOL (WINAPI *PFN_MS_EX) (lMEMORYSTATUSEX*);
#endif

void
GetRealMemoryMBAvailable(ssize_t &available, ssize_t &total)
{
    available = -1;
    total = -1;

#ifdef _WIN32

    static bool checked = false;
    static bool exFound = false;
    static PFN_MS_EX ex;

    if (!checked) {

        HMODULE h = GetModuleHandleA("kernel32.dll");

        if (h) {
            if ((ex = (PFN_MS_EX)GetProcAddress(h, "GlobalMemoryStatusEx"))) {
                exFound = true;
            }
        }
        
        checked = true;
    }

    DWORDLONG wavail = 0;
    DWORDLONG wtotal = 0;

    if (exFound) {

        lMEMORYSTATUSEX lms;
        lms.dwLength = sizeof(lms);
        if (!ex(&lms)) {
            cerr << "WARNING: GlobalMemoryStatusEx failed: error code "
                 << GetLastError() << endl;
            return;
        }
        wavail = lms.ullAvailPhys;
        wtotal = lms.ullTotalPhys;

    } else {

        /* Fall back to GlobalMemoryStatus which is always available.
           but returns wrong results for physical memory > 4GB  */

        MEMORYSTATUS ms;
        GlobalMemoryStatus(&ms);
        wavail = ms.dwAvailPhys;
        wtotal = ms.dwTotalPhys;
    }

    DWORDLONG size = wavail / 1048576;
    if (size > INT_MAX) size = INT_MAX;
    available = ssize_t(size);

    size = wtotal / 1048576;
    if (size > INT_MAX) size = INT_MAX;
    total = ssize_t(size);

    return;

#else
#ifdef __APPLE__

    unsigned int val32;
    int64_t val64;
    int mib[2];
    size_t size_sys;
    
    mib[0] = CTL_HW;

    mib[1] = HW_MEMSIZE;
    size_sys = sizeof(val64);
    sysctl(mib, 2, &val64, &size_sys, NULL, 0);
    if (val64) total = val64 / 1048576;

    mib[1] = HW_USERMEM;
    size_sys = sizeof(val32);
    sysctl(mib, 2, &val32, &size_sys, NULL, 0);
    if (val32) available = val32 / 1048576;

    // The newer memsize sysctl returns a 64-bit value, but usermem is
    // an old 32-bit value that doesn't seem to have an updated
    // alternative (?) - so it can't return more than 2G. In practice
    // it seems to return values far lower than that, even where more
    // than 2G of real memory is free. So we can't actually tell when
    // we're getting low on memory at all. Most of the time I think we
    // just need to use an arbitrary value like this one.
    if (available < total/4) {
        available = total/4;
    }

    return;

#else

    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (!meminfo) return;

    char buf[256];
    while (!feof(meminfo)) {
        if (!fgets(buf, 256, meminfo)) {
            fclose(meminfo);
            return;
        }
        bool isMemFree = (strncmp(buf, "MemFree:", 8) == 0);
        bool isMemTotal = (!isMemFree && (strncmp(buf, "MemTotal:", 9) == 0));
        if (isMemFree || isMemTotal) {
            QString line = QString(buf).trimmed();
            QStringList elements = line.split(' ', QString::SkipEmptyParts);
            QString unit = "kB";
            if (elements.size() > 2) unit = elements[2];
            int size = elements[1].toInt();
//            cerr << "have size \"" << size << "\", unit \""
//                      << unit << "\"" << endl;
            if (unit.toLower() == "gb") size = size * 1024;
            else if (unit.toLower() == "mb") size = size;
            else if (unit.toLower() == "kb") size = size / 1024;
            else size = size / 1048576;

            if (isMemFree) available = size;
            else total = size;
        }
        if (available != -1 && total != -1) {
            fclose(meminfo);
            return;
        }
    }
    fclose(meminfo);

    return;

#endif
#endif
}

ssize_t
GetDiscSpaceMBAvailable(const char *path)
{
#ifdef _WIN32
    ULARGE_INTEGER available, total, totalFree;
    if (GetDiskFreeSpaceExA(path, &available, &total, &totalFree)) {
        __int64 a = available.QuadPart;
        a /= 1048576;
        if (a > INT_MAX) a = INT_MAX;
        return ssize_t(a);
    } else {
        cerr << "WARNING: GetDiskFreeSpaceEx failed: error code "
             << GetLastError() << endl;
        return -1;
    }
#else
    struct statvfs buf;
    if (!statvfs(path, &buf)) {
        // do the multiplies and divides in this order to reduce the
        // likelihood of arithmetic overflow
//        cerr << "statvfs(" << path << ") says available: " << buf.f_bavail << ", block size: " << buf.f_bsize << endl;
        uint64_t available = ((buf.f_bavail / 1024) * buf.f_bsize) / 1024;
        if (available > INT_MAX) available = INT_MAX;
        return ssize_t(available);
    } else {
        perror("statvfs failed");
        return -1;
    }
#endif
}

#ifdef _WIN32
extern void SystemMemoryBarrier()
{
#ifdef _MSC_VER
    MemoryBarrier();
#else /* mingw */
    LONG Barrier = 0;
    __asm__ __volatile__("xchgl %%eax,%0 "
                         : "=r" (Barrier));
#endif
}
#else /* !_WIN32 */
#if !defined(__APPLE__) && defined(__GNUC__) && ((__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ == 0))
void
SystemMemoryBarrier()
{
    pthread_mutex_t dummy = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&dummy);
    pthread_mutex_unlock(&dummy);
}
#endif /* !defined(__APPLE__) etc */
#endif /* !_WIN32 */


static char *startupLocale = nullptr;

void
StoreStartupLocale()
{
    char *loc = setlocale(LC_ALL, nullptr);
    if (!loc) return;
    if (startupLocale) free(startupLocale);
    startupLocale = strdup(loc);
}

void
RestoreStartupLocale()
{
    if (!startupLocale) {
        setlocale(LC_ALL, "");
    } else {
        setlocale(LC_ALL, startupLocale);
    }
}

double mod(double x, double y) { return x - (y * floor(x / y)); }
float modf(float x, float y) { return x - (y * floorf(x / y)); }

double princarg(double a) { return mod(a + M_PI, -2 * M_PI) + M_PI; }
float princargf(float a) { return float(princarg(a)); }

bool
getEnvUtf8(std::string variable, std::string &value)
{
    value = "";
    
#ifdef _WIN32
    int wvarlen = MultiByteToWideChar(CP_UTF8, 0,
                                      variable.c_str(), int(variable.length()),
                                      0, 0);
    if (wvarlen < 0) {
        SVCERR << "WARNING: Unable to convert environment variable name "
               << variable << " to wide characters" << endl;
        return false;
    }
    
    wchar_t *wvarbuf = new wchar_t[wvarlen + 1];
    (void)MultiByteToWideChar(CP_UTF8, 0,
                              variable.c_str(), int(variable.length()),
                              wvarbuf, wvarlen);
    wvarbuf[wvarlen] = L'\0';
    
    wchar_t *wvalue = _wgetenv(wvarbuf);

    delete[] wvarbuf;

    if (!wvalue) {
        return false;
    }

    int wvallen = int(wcslen(wvalue));
    int vallen = WideCharToMultiByte(CP_UTF8, 0,
                                     wvalue, wvallen,
                                     0, 0, 0, 0);
    if (vallen < 0) {
        SVCERR << "WARNING: Unable to convert environment value to UTF-8"
               << endl;
        return false;
    }

    char *val = new char[vallen + 1];
    (void)WideCharToMultiByte(CP_UTF8, 0,
                              wvalue, wvallen,
                              val, vallen, 0, 0);
    val[vallen] = '\0';

    value = val;

    delete[] val;
    return true;

#else

    char *val = getenv(variable.c_str());
    if (!val) {
        return false;
    }

    value = val;
    return true;
    
#endif
}

bool
putEnvUtf8(std::string variable, std::string value)
{
#ifdef _WIN32
    std::string entry = variable + "=" + value;
    
    int wentlen = MultiByteToWideChar(CP_UTF8, 0,
                                      entry.c_str(), int(entry.length()),
                                      0, 0);
    if (wentlen < 0) {
        SVCERR << "WARNING: Unable to convert environment entry to "
               << "wide characters" << endl;
        return false;
    }
    
    wchar_t *wentbuf = new wchar_t[wentlen + 1];
    (void)MultiByteToWideChar(CP_UTF8, 0,
                              entry.c_str(), int(entry.length()),
                              wentbuf, wentlen);
    wentbuf[wentlen] = L'\0';

    int rv = _wputenv(wentbuf);

    delete[] wentbuf;

    if (rv != 0) {
        SVCERR << "WARNING: Failed to set environment entry" << endl;
        return false;
    }
    return true;

#else

    int rv = setenv(variable.c_str(), value.c_str(), 1);
    if (rv != 0) {
        SVCERR << "WARNING: Failed to set environment entry" << endl;
        return false;
    }
    return true;
    
#endif
}

