/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/**
 * [Vamp] Plugin Load Checker
 *
 * This program accepts the name of a descriptor symbol as its only
 * command-line argument. It then reads a list of plugin library paths
 * from stdin, one per line. For each path read, it attempts to load
 * that library and retrieve the named descriptor symbol, printing a
 * line to stdout reporting whether this was successful or not and
 * then flushing stdout. The output line format is described
 * below. The program exits with code 0 if all libraries were loaded
 * successfully and non-zero otherwise.
 *
 * Note that library paths must be ready to pass to dlopen() or
 * equivalent; this usually means they should be absolute paths.
 *
 * Output line for successful load of library libname.so:
 * SUCCESS|/path/to/libname.so|
 * 
 * Output line for failed load of library libname.so:
 * FAILURE|/path/to/libname.so|Error message [failureCode]
 *
 * or:
 * FAILURE|/path/to/libname.so|[failureCode]
 *
 * where the error message is an optional system-level message, such
 * as may be returned from strerror or similar (which should be in the
 * native language for the system ready to show the user), and the
 * failureCode in square brackets is a mandatory number corresponding
 * to one of the PluginCandidates::FailureCode values (requiring
 * conversion to a translated string by the client).
 *
 * Although this program was written for use with Vamp audio analysis
 * plugins, it also works with other plugin formats. The program has
 * some hardcoded knowledge of Vamp, LADSPA, and DSSI plugins, but it
 * can be used with any plugins that involve loading DLLs and looking
 * up descriptor functions from them.
 *
 * Sometimes plugins will crash completely on load, bringing down this
 * program with them. If the program exits before all listed plugins
 * have been checked, this means that the plugin following the last
 * reported one has crashed. Typically the caller may want to run it
 * again, omitting that plugin.
 */

/*
    Copyright (c) 2016-2017 Queen Mary, University of London

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

#include "../version.h"

#include "../checker/checkcode.h"

static const char programName[] = "vamp-plugin-load-checker";

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include <fcntl.h>

#include <string>
#include <iostream>
#include <stdexcept>

#ifdef _WIN32
#ifndef UNICODE
#error "This must be compiled with UNICODE defined"
#endif

static std::string lastLibraryName = "";

static HMODULE loadLibraryUTF8(std::string name) {
    lastLibraryName = name;
    int n = name.size();
    int wn = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), n, 0, 0);
    wchar_t *wname = new wchar_t[wn+1];
    wn = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), n, wname, wn);
    wname[wn] = L'\0';
    HMODULE h = LoadLibraryW(wname);
    delete[] wname;
    return h;
}

static std::string getErrorText() {
    DWORD err = GetLastError();
    wchar_t *buffer = 0;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        // the correct way to specify the user's default language,
        // according to all resources I could find:
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &buffer,
        0, NULL );
    if (!buffer) {
        return "Unable to format error string (internal error)";
    }
    int wn = wcslen(buffer);
    int n = WideCharToMultiByte(CP_UTF8, 0, buffer, wn, 0, 0, 0, 0);
    if (n < 0) {
        LocalFree(buffer);
        return "Unable to convert error string (internal error)";
    }
    char *text = new char[n+1];
    (void)WideCharToMultiByte(CP_UTF8, 0, buffer, wn, text, n, 0, 0);
    text[n] = '\0';
    std::string s(text);
    LocalFree(buffer);
    delete[] text;
    if (s == "") {
        return s;
    }
    for (int i = s.size(); i > 0; ) {
        --i;
        if (s[i] == '\n' || s[i] == '\r') {
            s.erase(i, 1);
        }
    }
    std::size_t pos = s.find("%1");
    if (pos != std::string::npos && lastLibraryName != "") {
        s.replace(pos, 2, lastLibraryName);
    }
    return s;
}

#define DLOPEN(a,b)  loadLibraryUTF8(a)
#define DLSYM(a,b)   (void *)GetProcAddress((HINSTANCE)(a),(b).c_str())
#define DLCLOSE(a)   (!FreeLibrary((HINSTANCE)(a)))
#define DLERROR()    (getErrorText())

static bool libraryExists(std::string name) {
    if (name == "") return false;
    int n = name.size();
    int wn = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), n, 0, 0);
    wchar_t *wname = new wchar_t[wn+1];
    wn = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), n, wname, wn);
    wname[wn] = L'\0';
    FILE *f = _wfopen(wname, L"rb");
    delete[] wname;
    if (f) {
        fclose(f);
        return true;
    } else {
        return false;
    }
}

#else

#include <dlfcn.h>
#define DLOPEN(a,b)  dlopen((a).c_str(),(b))
#define DLSYM(a,b)   dlsym((a),(b).c_str())
#define DLCLOSE(a)   dlclose((a))
#define DLERROR()    dlerror()

static bool libraryExists(std::string name) {
    if (name == "") return false;
    FILE *f = fopen(name.c_str(), "r");
    if (f) {
        fclose(f);
        return true;
    } else {
        return false;
    }
}

#endif

//#include <unistd.h>

using namespace std;

string error()
{
    return DLERROR();
}

struct Result {
    PluginCheckCode code;
    string message;
};

Result checkLADSPAStyleDescriptorFn(void *f)
{
    typedef const void *(*DFn)(unsigned long);
    DFn fn = DFn(f);
    unsigned long index = 0;
    while (fn(index)) ++index;
    if (index == 0) return { PluginCheckCode::FAIL_NO_PLUGINS, "" };
    return { PluginCheckCode::SUCCESS, "" };
}

Result checkVampDescriptorFn(void *f)
{
    typedef const void *(*DFn)(unsigned int, unsigned int);
    DFn fn = DFn(f);
    unsigned int index = 0;
    while (fn(2, index)) ++index;
    if (index == 0) return { PluginCheckCode::FAIL_NO_PLUGINS, "" };
    return { PluginCheckCode::SUCCESS, "" };
}

Result check(string soname, string descriptor)
{
    void *handle = DLOPEN(soname, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        PluginCheckCode code = PluginCheckCode::FAIL_NOT_LOADABLE;
        string message = error();
#ifdef _WIN32
        DWORD err = GetLastError();
        if (err == ERROR_BAD_EXE_FORMAT) {
            code = PluginCheckCode::FAIL_WRONG_ARCHITECTURE;
        } else if (err == ERROR_MOD_NOT_FOUND) {
            if (libraryExists(soname)) {
                code = PluginCheckCode::FAIL_DEPENDENCY_MISSING;
            } else {
                code = PluginCheckCode::FAIL_LIBRARY_NOT_FOUND;
            }
        }
#else  // !_WIN32
#ifdef __APPLE__
        if (errno == EPERM) {
            // This may be unreliable, but it seems to be set by
            // something dlopen() calls in the case where a library
            // can't be loaded for code-signing-related reasons on
            // macOS
            code = PluginCheckCode::FAIL_FORBIDDEN;
        } else if (!libraryExists(soname)) {
            code = PluginCheckCode::FAIL_LIBRARY_NOT_FOUND;
        }
#else  // !__APPLE__
        if (!libraryExists(soname)) {
            code = PluginCheckCode::FAIL_LIBRARY_NOT_FOUND;
        }
#endif // !__APPLE__
#endif // !_WIN32

        return { code, message };
    }

    Result result { PluginCheckCode::SUCCESS, "" };

    void *fn = DLSYM(handle, descriptor);
    if (!fn) {
        result = { PluginCheckCode::FAIL_DESCRIPTOR_MISSING, error() };
    } else if (descriptor == "ladspa_descriptor") {
        result = checkLADSPAStyleDescriptorFn(fn);
    } else if (descriptor == "dssi_descriptor") {
        result = checkLADSPAStyleDescriptorFn(fn);
    } else if (descriptor == "vampGetPluginDescriptor") {
        result = checkVampDescriptorFn(fn);
    } else {
        cerr << "Note: no descriptor logic known for descriptor function \""
             << descriptor << "\"; not actually calling it" << endl;
    }

    DLCLOSE(handle);
    
    return result;
}

// We write our output to stdout, but want to ensure that the plugin
// doesn't write anything itself. To do this we open a null file
// descriptor and dup2() it into place of stdout in the gaps between
// our own output activity.

static int normalFd = -1;
static int suspendedFd = -1;

static void initFds()
{
#ifdef _WIN32
    normalFd = _dup(1);
    suspendedFd = _open("NUL", _O_WRONLY);
#else
    normalFd = dup(1);
    suspendedFd = open("/dev/null", O_WRONLY);
#endif
    
    if (normalFd < 0 || suspendedFd < 0) {
        throw std::runtime_error
            ("Failed to initialise fds for stdio suspend/resume");
    }
}

static void suspendOutput()
{
#ifdef _WIN32
    _dup2(suspendedFd, 1);
#else
    dup2(suspendedFd, 1);
#endif
}

static void resumeOutput()
{
    fflush(stdout);
#ifdef _WIN32
    _dup2(normalFd, 1);
#else
    dup2(normalFd, 1);
#endif
}

int main(int argc, char **argv)
{
    bool allGood = true;
    string soname;

    bool showUsage = false;
    
    if (argc > 1) {
        string opt = argv[1];
        if (opt == "-?" || opt == "-h" || opt == "--help") {
            showUsage = true;
        } else if (opt == "-v" || opt == "--version") {
            cout << CHECKER_COMPATIBILITY_VERSION << endl;
            return 0;
        }
    } 
    
    if (argc != 2 || showUsage) {
        cerr << endl;
        cerr << programName << ": Test shared library objects for plugins to be" << endl;
        cerr << "loaded via descriptor functions." << endl;
        cerr << "\n    Usage: " << programName << " <descriptorname>\n"
            "\nwhere descriptorname is the name of a plugin descriptor symbol to be sought\n"
            "in each library (e.g. vampGetPluginDescriptor for Vamp plugins). The list of\n"
            "candidate plugin library filenames is read from stdin.\n" << endl;
        return 2;
    }

    string descriptor = argv[1];
    
#ifdef _WIN32
    // Avoid showing the error-handler dialog for missing DLLs,
    // failing quietly instead. It's permissible for this program
    // to simply fail when a DLL can't be loaded -- showing the
    // error dialog wouldn't change this anyway, it would just
    // block the program until the user clicked it away and then
    // fail anyway.
    SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

    initFds();
    suspendOutput();
    
    while (getline(cin, soname)) {
        Result result = check(soname, descriptor);
        resumeOutput();
        if (result.code == PluginCheckCode::SUCCESS) {
            cout << "SUCCESS|" << soname << "|" << endl;
        } else {
            if (result.message == "") {
                cout << "FAILURE|" << soname
                     << "|[" << int(result.code) << "]" << endl;
            } else {
                for (size_t i = 0; i < result.message.size(); ++i) {
                    if (result.message[i] == '\n' ||
                        result.message[i] == '\r') {
                        result.message[i] = ' ';
                    }
                }
                cout << "FAILURE|" << soname
                     << "|" << result.message << " ["
                     << int(result.code) << "]" << endl;
            }
            allGood = false;
        }
        suspendOutput();
    }
    
    return allGood ? 0 : 1;
}
