/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    bqaudiostream

    A small library wrapping various audio file read/write
    implementations in C++.

    Copyright 2007-2015 Particular Programs Ltd.

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

#ifndef BQAUDIOSTREAM_EXCEPTIONS_H
#define BQAUDIOSTREAM_EXCEPTIONS_H

#include <exception>
#include <string>

namespace breakfastquay {

/**
 * Failed to open a file for reading, because the file did not exist.
 */
class FileNotFound : virtual public std::exception
{
public:
    FileNotFound(std::string file) throw();
    virtual ~FileNotFound() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }
    
protected:
    std::string m_file;
    std::string m_what;
};

/**
 * Failed to read a file, although the file existed. May mean we did
 * not have read permission.
 */
class FileReadFailed : virtual public std::exception
{
public:
    FileReadFailed(std::string file) throw();
    virtual ~FileReadFailed() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }

protected:
    std::string m_file;
    std::string m_what;
};

/**
 * Failed to read a file because it did not seem to have the expected
 * format or contained errors.
 */
class InvalidFileFormat : virtual public std::exception
{
public:
    InvalidFileFormat(std::string file, std::string how) throw();
    virtual ~InvalidFileFormat() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }
    
protected:
    std::string m_what;
};

/**
 * Failed to read or write a file because we do not have a reader,
 * writer, decoder, or encoder for the requested file type.
 */
class UnknownFileType : virtual public std::exception
{
public:
    UnknownFileType(std::string file) throw();
    virtual ~UnknownFileType() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }
    
protected:
    std::string m_what;
};

/**
 * Failed to open a file for writing. Possibly the containing
 * directory did not exist, or lacked write permission.
 */
class FailedToWriteFile : virtual public std::exception
{
public:
    FailedToWriteFile(std::string file) throw();
    virtual ~FailedToWriteFile() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }
    
protected:
    std::string m_file;
    std::string m_what;
};

/**
 * Failed to read, write, or manipulate a file in some way not
 * adequately described by any of the other exception types. This may
 * also indicate an internal error in an encoder or decoder library.
 */
class FileOperationFailed : virtual public std::exception
{
public:
    FileOperationFailed(std::string file, std::string operation) throw();
    FileOperationFailed(std::string file, std::string operation,
                        std::string explanation) throw();
    virtual ~FileOperationFailed() throw() { }
    virtual const char *what() const throw() { return m_what.c_str(); }

protected:
    std::string m_file;
    std::string m_operation;
    std::string m_explanation;
    std::string m_what;
};

}
#endif
