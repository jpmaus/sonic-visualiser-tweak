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

#ifndef CHECK_CODE_H
#define CHECK_CODE_H

enum class PluginCheckCode {

    SUCCESS = 0,

    /** Plugin library file is not found
     */
    FAIL_LIBRARY_NOT_FOUND = 1,

    /** Plugin library does appear to be a library, but its
     *  architecture differs from that of the checker program, in
     *  a way that can be distinguished from other loader
     *  failures. On Windows this may arise from system error 193,
     *  ERROR_BAD_EXE_FORMAT
     */
    FAIL_WRONG_ARCHITECTURE = 2,

    /** Plugin library depends on some other library that cannot be
     *  loaded. On Windows this may arise from system error 126,
     *  ERROR_MOD_NOT_FOUND, provided that the library file itself
     *  exists
     */
    FAIL_DEPENDENCY_MISSING = 3,

    /** Plugin library loading was refused for some security-related
     *  reason
     */
    FAIL_FORBIDDEN = 4,

    /** Plugin library cannot be loaded for some other reason
     */
    FAIL_NOT_LOADABLE = 5,

    /** Plugin library can be loaded, but the expected plugin
     *  descriptor symbol is missing
     */
    FAIL_DESCRIPTOR_MISSING = 6,

    /** Plugin library can be loaded and descriptor called, but no
     *  plugins are found in it
     */
    FAIL_NO_PLUGINS = 7,

    /** Failure but no meaningful error code provided, or failure
     *  read from an older helper version that did not support
     *  error codes
     */
    FAIL_OTHER = 999
};

#endif
