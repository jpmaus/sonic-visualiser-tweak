/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "UnsupportedFormat.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include <iostream>

bool
UnsupportedFormat::isLegitimatelyUnsupported(QString format)
{
#ifdef Q_OS_WIN

    if (sizeof(void *) == 4) {
        // Our 32-bit MinGW build lacks MediaFoundation support
        return (format == "aac" ||
                format == "apple_lossless" ||
                format == "m4a" ||
                format == "wma");
    }

    // Our CI tests run on Windows Server, which annoyingly seems to
    // come without codecs for WMA and AAC
    
    NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
    *(FARPROC*)&RtlGetVersion = GetProcAddress
        (GetModuleHandleA("ntdll"), "RtlGetVersion");

    if (RtlGetVersion) {

        OSVERSIONINFOEXW osInfo;
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        RtlGetVersion(&osInfo);

        if (osInfo.wProductType != VER_NT_WORKSTATION) {
            std::cerr << "NOTE: We appear to be running on Windows Server (wProductType = " << osInfo.wProductType << ") - assuming encumbered media codecs might not be installed and being lenient about them" << std::endl;
            return (format == "aac" ||
                    format == "apple_lossless" ||
                    format == "m4a" ||
                    format == "wma");
        }
        
    } else {
        std::cerr << "WARNING: Failed to find RtlGetVersion in NTDLL"
                  << std::endl;
    }

    // If none of the above applies, then we should have everything
    // except this:
    
    return (format == "apple_lossless");
    
#else
#ifdef Q_OS_MAC
    return (format == "wma");
#else
    return (format == "aac" ||
            format == "apple_lossless" ||
            format == "m4a" ||
            format == "wma");
#endif
#endif
}
