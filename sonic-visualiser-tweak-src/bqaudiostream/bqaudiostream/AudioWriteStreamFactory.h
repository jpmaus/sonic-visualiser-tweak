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

#ifndef BQ_AUDIO_WRITE_STREAM_FACTORY_H
#define BQ_AUDIO_WRITE_STREAM_FACTORY_H

#include <string>
#include <vector>

namespace breakfastquay {

class AudioWriteStream;

class AudioWriteStreamFactory
{
public:
    /**
     * Create and return a write stream object for the given audio
     * file name, if possible. The audio file format will be deduced
     * from the file extension. If the file already exists, it will be
     * silently overwritten.
     *
     * May throw FailedToWriteFile, FileOperationFailed, or
     * UnknownFileType.
     *
     * This function never returns NULL; it will always throw an
     * exception instead. (If there is simply no write stream
     * registered for the file extension, it will throw
     * UnknownFileType.)
     */
    static AudioWriteStream *createWriteStream(std::string fileName,
                                               size_t channelCount,
                                               size_t sampleRate);

    static std::vector<std::string> getSupportedFileExtensions();

    static bool isExtensionSupportedFor(std::string fileName);

    /**
     * Return a "preferred" and definitely supported file extension
     * for writing uncompressed audio files.
     *
     * Returns an empty string if no sufficiently mainstream
     * uncompressed format is supported.
     */
    static std::string getDefaultUncompressedFileExtension();

    /**
     * Return a "preferred" and definitely supported file extension
     * for writing lossily compressed audio files.
     *
     * Returns an empty string if no sufficiently mainstream lossy
     * format is supported.
     */
    static std::string getDefaultLossyFileExtension();

    /**
     * Return the extension of a given filename (e.g. "wav" for "A.WAV").
     */
    static std::string extensionOf(std::string fileName);
};

}

#endif
