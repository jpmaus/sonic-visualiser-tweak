/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/*
    bqaudioio

    Copyright 2007-2016 Particular Programs Ltd.

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
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    Except as contained in this notice, the names of Chris Cannam and
    Particular Programs Ltd shall not be used in advertising or
    otherwise to promote the sale, use or other dealings in this
    Software without prior written authorization.
*/

#ifndef BQAUDIOIO_DYNAMIC_JACK_H
#define BQAUDIOIO_DYNAMIC_JACK_H

#if ( defined BUILD_STATIC ) || ( defined DYNAMIC_JACK )
#if (! defined _WIN32) && (! defined __APPLE__)

// Some lunacy to enable JACK support in static builds.  JACK isn't
// supposed to be linked statically, because it depends on a
// consistent shared memory layout between client library and daemon,
// so it's very fragile in the face of version mismatches.
//
// Therefore for static builds on Linux we avoid linking against JACK
// at all during the build, instead using dlopen and runtime symbol
// lookup to switch on JACK support at runtime.  The following big
// mess (down to the #endifs) is the code that implements this.

#ifdef HAVE_JACK

#include <jack/jack.h>
#include <dlfcn.h>

#include <map>
#include <iostream>

namespace breakfastquay {

//#define DEBUG_AUDIO_JACK_TARGET 1

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

static void *symbol(const char *name)
{
    static bool attempted = false;
    static void *library = 0;
    static std::map<const char *, void *> symbols;
    if (symbols.find(name) != symbols.end()) return symbols[name];
    if (!library) {
        if (!attempted) {
            library = ::dlopen("libjack.so.1", RTLD_NOW);
            if (!library) library = ::dlopen("libjack.so.0", RTLD_NOW);
            if (!library) library = ::dlopen("libjack.so", RTLD_NOW);
            if (!library) {
                std::cerr << "WARNING: Failed to load JACK library: "
                          << ::dlerror() << " (tried .so, .so.0, .so.1)"
                          << std::endl;
            }
            attempted = true;
        }
        if (!library) return 0;
    }
    void *symbol = ::dlsym(library, name);
    if (!symbol) {
        std::cerr << "WARNING: JACKPlaybackTarget: Failed to locate symbol "
                  << name << ": " << ::dlerror() << std::endl;
    }
    symbols[name] = symbol;
    return symbol;
}

static jack_client_t *dynamic_jack_client_open(const char *client_name,
                                               jack_options_t options,
                                               jack_status_t *status, ...)
{
    typedef jack_client_t *(*func)(const char *client_name,
                                   jack_options_t options,
                                   jack_status_t *status, ...);
    void *s = symbol("jack_client_open");
    if (!s) return 0;
    func f = (func)s;
    return f(client_name, options, status); // varargs not supported here
}

static int dynamic_jack_set_process_callback(jack_client_t *client,
                                             JackProcessCallback process_callback,
                                             void *arg)
{
    typedef int (*func)(jack_client_t *client,
                        JackProcessCallback process_callback,
                        void *arg);
    void *s = symbol("jack_set_process_callback");
    if (!s) return 1;
    func f = (func)s;
    return f(client, process_callback, arg);
}

static int dynamic_jack_set_xrun_callback(jack_client_t *client,
                                          JackXRunCallback xrun_callback,
                                          void *arg)
{
    typedef int (*func)(jack_client_t *client,
                        JackXRunCallback xrun_callback,
                        void *arg);
    void *s = symbol("jack_set_xrun_callback");
    if (!s) return 1;
    func f = (func)s;
    return f(client, xrun_callback, arg);
}

static const char **dynamic_jack_get_ports(jack_client_t *client, 
                                           const char *port_name_pattern, 
                                           const char *type_name_pattern, 
                                           unsigned long flags)
{
    typedef const char **(*func)(jack_client_t *client, 
                                 const char *port_name_pattern, 
                                 const char *type_name_pattern, 
                                 unsigned long flags);
    void *s = symbol("jack_get_ports");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port_name_pattern, type_name_pattern, flags);
}

static jack_port_t *dynamic_jack_port_register(jack_client_t *client,
                                               const char *port_name,
                                               const char *port_type,
                                               unsigned long flags,
                                               unsigned long buffer_size)
{
    typedef jack_port_t *(*func)(jack_client_t *client,
                                 const char *port_name,
                                 const char *port_type,
                                 unsigned long flags,
                                 unsigned long buffer_size);
    void *s = symbol("jack_port_register");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port_name, port_type, flags, buffer_size);
}

static int dynamic_jack_connect(jack_client_t *client,
                                const char *source,
                                const char *dest)
{
    typedef int (*func)(jack_client_t *client,
                        const char *source,
                        const char *dest);
    void *s = symbol("jack_connect");
    if (!s) return 1;
    func f = (func)s;
    return f(client, source, dest);
}

static void *dynamic_jack_port_get_buffer(jack_port_t *port,
                                          jack_nframes_t sz)
{
    typedef void *(*func)(jack_port_t *, jack_nframes_t);
    void *s = symbol("jack_port_get_buffer");
    if (!s) return 0;
    func f = (func)s;
    return f(port, sz);
}

static void dynamic_jack_port_get_latency_range(jack_port_t *port,
                                                jack_latency_callback_mode_t mode,
                                                jack_latency_range_t *range)
{
    typedef void (*func)(jack_port_t *, jack_latency_callback_mode_t, jack_latency_range_t *);
    void *s = symbol("jack_port_get_latency_range");
    if (!s) {
        range->min = range->max = 0;
        return;
    }
    func f = (func)s;
    f(port, mode, range);
}

static int dynamic_jack_port_unregister(jack_client_t *client,
                                        jack_port_t *port)
{
    typedef int(*func)(jack_client_t *, jack_port_t *);
    void *s = symbol("jack_port_unregister");
    if (!s) return 0;
    func f = (func)s;
    return f(client, port);
}

#define dynamic1(rv, name, argtype, failval) \
    static rv dynamic_##name(argtype arg) { \
        typedef rv (*func) (argtype); \
        void *s = symbol(#name); \
        if (!s) return failval; \
        func f = (func) s; \
        return f(arg); \
    }

dynamic1(jack_client_t *, jack_client_new, const char *, 0);
dynamic1(jack_nframes_t, jack_get_buffer_size, jack_client_t *, 0);
dynamic1(jack_nframes_t, jack_get_sample_rate, jack_client_t *, 0);
dynamic1(int, jack_activate, jack_client_t *, 1);
dynamic1(int, jack_deactivate, jack_client_t *, 1);
dynamic1(int, jack_client_close, jack_client_t *, 1);
dynamic1(jack_nframes_t, jack_port_get_latency, jack_port_t *, 0);
dynamic1(const char *, jack_port_name, const jack_port_t *, 0);
dynamic1(jack_nframes_t, jack_frame_time, jack_client_t *, 0);

#define jack_client_new dynamic_jack_client_new
#define jack_client_open dynamic_jack_client_open
#define jack_get_buffer_size dynamic_jack_get_buffer_size
#define jack_get_sample_rate dynamic_jack_get_sample_rate
#define jack_set_process_callback dynamic_jack_set_process_callback
#define jack_set_xrun_callback dynamic_jack_set_xrun_callback
#define jack_activate dynamic_jack_activate
#define jack_deactivate dynamic_jack_deactivate
#define jack_client_close dynamic_jack_client_close
#define jack_get_ports dynamic_jack_get_ports
#define jack_port_register dynamic_jack_port_register
#define jack_port_unregister dynamic_jack_port_unregister
#define jack_port_get_latency dynamic_jack_port_get_latency
#define jack_port_name dynamic_jack_port_name
#define jack_connect dynamic_jack_connect
#define jack_port_get_buffer dynamic_jack_port_get_buffer
#define jack_port_get_latency_range dynamic_jack_port_get_latency_range
#define jack_frame_time dynamic_jack_frame_time

}

#endif // HAVE_JACK

#endif // (! defined _WIN32) && (! defined __APPLE__)

#endif // BUILD_STATIC || DYNAMIC_JACK

#endif // BQAUDIOIO_DYNAMIC_JACK_H
