/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
  Copyright (c) 2016-2018 Queen Mary, University of London

  Permission is hereby granted, free of charge, to any person
  obtaining a copy of this software and associated documentation
  files (the "Software"), to deal in the Software without
  restriction, including without limitation the rights to use, copy,
  modify, merge, publish, distribute, sublicense, and/or sell copies
  of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be
  included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

  Except as contained in this notice, the names of the Centre for
  Digital Music and Queen Mary, University of London shall not be
  used in advertising or otherwise to promote the sale, use or other
  dealings in this Software without prior written authorization.
*/

#include "knownplugins.h"

#include <iostream>

using namespace std;

#if defined(_WIN32)
#include <windows.h>
#define PATH_SEPARATOR ';'
#else
#define PATH_SEPARATOR ':'
#endif

static bool
getEnvUtf8(std::string variable, std::string &value)
{
    value = "";
    
#ifdef _WIN32
    int wvarlen = MultiByteToWideChar(CP_UTF8, 0,
                                      variable.c_str(), int(variable.length()),
                                      0, 0);
    if (wvarlen < 0) {
        cerr << "WARNING: Unable to convert environment variable name "
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
        cerr << "WARNING: Unable to convert environment value to UTF-8" << endl;
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

KnownPlugins::KnownPlugins(BinaryFormat format) :
    m_format(format)
{
    string variableSuffix = "";
    if (m_format == FormatNonNative32Bit) {
        variableSuffix = "_32";
    }
    
    m_known[VampPlugin] = {
        "vamp",
        "VAMP_PATH" + variableSuffix,
        {}, {},
        "vampGetPluginDescriptor"
    };
    
    m_known[LADSPAPlugin] = {
        "ladspa",
        "LADSPA_PATH" + variableSuffix,
        {}, {},
        "ladspa_descriptor"
    };

    m_known[DSSIPlugin] = {
        "dssi",
        "DSSI_PATH" + variableSuffix,
        {}, {},
        "dssi_descriptor"
    };

    for (auto &k: m_known) {
        k.second.defaultPath = expandPathString(getDefaultPathString(k.first));
        k.second.path = expandConventionalPath(k.first, k.second.variable);
    }
}

vector<KnownPlugins::PluginType>
KnownPlugins::getKnownPluginTypes() const
{
    vector<PluginType> kt;

    for (const auto &k: m_known) {
        kt.push_back(k.first);
    }

    return kt;
}

string
KnownPlugins::getUnexpandedDefaultPathString(PluginType type)
{
    switch (type) {

#if defined(_WIN32)

    case VampPlugin:
        return "%ProgramFiles%\\Vamp Plugins";
    case LADSPAPlugin:
        return "%ProgramFiles%\\LADSPA Plugins;%ProgramFiles%\\Audacity\\Plug-Ins";
    case DSSIPlugin:
        return "%ProgramFiles%\\DSSI Plugins";
        
#elif defined(__APPLE__)
        
    case VampPlugin:
        return "$HOME/Library/Audio/Plug-Ins/Vamp:/Library/Audio/Plug-Ins/Vamp";
    case LADSPAPlugin:
        return "$HOME/Library/Audio/Plug-Ins/LADSPA:/Library/Audio/Plug-Ins/LADSPA";
    case DSSIPlugin:
        return "$HOME/Library/Audio/Plug-Ins/DSSI:/Library/Audio/Plug-Ins/DSSI";
        
#else /* Linux, BSDs, etc */
        
    case VampPlugin:
        return "$HOME/vamp:$HOME/.vamp:/usr/local/lib/vamp:/usr/lib/vamp";
    case LADSPAPlugin:
        return "$HOME/ladspa:$HOME/.ladspa:/usr/local/lib/ladspa:/usr/lib/ladspa";
    case DSSIPlugin:
        return "$HOME/dssi:$HOME/.dssi:/usr/local/lib/dssi:/usr/lib/dssi";
#endif
    }

    throw logic_error("unknown or unhandled plugin type");
}

string
KnownPlugins::getDefaultPathString(PluginType type)
{
    string path = getUnexpandedDefaultPathString(type);

    if (path == "") {
        return path;
    }

    string home;
    if (getEnvUtf8("HOME", home)) {
        string::size_type f;
        while ((f = path.find("$HOME")) != string::npos &&
               f < path.length()) {
            path.replace(f, 5, home);
        }
    }

#ifdef _WIN32
    string pfiles, pfiles32;
    if (!getEnvUtf8("ProgramFiles", pfiles)) {
        pfiles = "C:\\Program Files";
    }
    if (!getEnvUtf8("ProgramFiles(x86)", pfiles32)) {
        pfiles32 = "C:\\Program Files (x86)";
    }
    
    string::size_type f;
    while ((f = path.find("%ProgramFiles%")) != string::npos &&
           f < path.length()) {
        if (m_format == FormatNonNative32Bit) {
            path.replace(f, 14, pfiles32);
        } else {
            path.replace(f, 14, pfiles);
        }
    }
#endif

    return path;
}

vector<string>
KnownPlugins::expandPathString(string path)
{
    vector<string> pathList;

    string::size_type index = 0, newindex = 0;

    while ((newindex = path.find(PATH_SEPARATOR, index)) < path.size()) {
        pathList.push_back(path.substr(index, newindex - index).c_str());
        index = newindex + 1;
    }
    
    pathList.push_back(path.substr(index));

    return pathList;
}

vector<string>
KnownPlugins::expandConventionalPath(PluginType type, string var)
{
    string path;

    if (!getEnvUtf8(var, path)) {
        path = getDefaultPathString(type);
    }

    return expandPathString(path);
}
