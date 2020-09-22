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

#ifdef HAVE_OPUS

#include <opus/opusfile.h>

#include "OpusReadStream.h"

#include <sstream>

namespace breakfastquay
{

static vector<string>
getOpusExtensions()
{
    vector<string> extensions;
    extensions.push_back("opus");
    return extensions;
}

static
AudioReadStreamBuilder<OpusReadStream>
opusbuilder(
    string("http://breakfastquay.com/rdf/turbot/audiostream/OpusReadStream"),
    getOpusExtensions()
    );

class OpusReadStream::D
{
public:
    D() : file(0) { }

    OggOpusFile *file;
};

OpusReadStream::OpusReadStream(string path) :
    m_path(path),
    m_d(new D)
{
    ostringstream os;
    
    m_channelCount = 0;
    m_sampleRate = 0;

    int err = 0;
    m_d->file = op_open_file(path.c_str(), &err);

    if (err || !m_d->file) {
        os << "OpusReadStream: Unable to open file (error code " << err << ")";
        m_error = os.str();
        m_d->file = 0;
        if (err == OP_EFAULT) {
            throw FileNotFound(m_path);
        } else {
            throw InvalidFileFormat(m_path, "failed to open audio file");
        }
    }

    const OpusTags *tags = op_tags(m_d->file, -1); //!!! who owns this?
    if (tags) {
        for (int i = 0; i < tags->comments; ++i) {
            string comment = tags->user_comments[i];
            for (size_t c = 0; c < comment.size(); ++c) {
                if (comment[c] == '=') {
                    string key = comment.substr(0, c);
                    string value = comment.substr(c + 1, std::string::npos);
                    if (key == "title") {
                        m_track = value;
                    } else if (key == "artist") {
                        m_artist = value;
                    }
                    break;
                }
            }
        }
    }

    m_channelCount = op_channel_count(m_d->file, -1);
    m_sampleRate = 48000; // libopusfile always decodes to 48kHz! I like that
}

size_t
OpusReadStream::getFrames(size_t count, float *frames)
{
//    cerr << "getFrames: count = " << count << endl;
    
    if (!m_d->file) return 0;
    if (count == 0) return 0;

//    cerr << "getFrames: working" << endl;

    int totalRequired = int(count);
    int totalObtained = 0;
    
    int channelsRequired = int(m_channelCount);

    float *fptr = frames;

    while (totalObtained < totalRequired) {

        int required = totalRequired - totalObtained;

        int likelyChannelCount = channelsRequired;
        const OpusHead *linkHead = op_head(m_d->file, -1);
        if (linkHead) {
            likelyChannelCount = linkHead->channel_count;
        }
        if (likelyChannelCount < channelsRequired) {
            // need to avoid overrun/truncation when reconfiguring later -
            // as our target contains enough space for more frames at a
            // lower channel count, so opusfile will return more than we
            // can then accommodate after reconfiguration
            required = (required / channelsRequired) * likelyChannelCount;
        }
        
        int li = -1;
        int obtained = op_read_float
            (m_d->file, fptr, required * channelsRequired, &li);

//        cerr << "required = " << required << ", obtained = " << obtained << endl;
        
        if (obtained == OP_HOLE) {
            continue;
        }

        if (obtained == 0) {
            break;
        }

        if (obtained < 0) {
            ostringstream os;
            os << "OpusReadStream: Failed to read from file (error code "
               << obtained << ")";
            m_error = os.str();
            throw InvalidFileFormat(m_path, "error in decoder");
        }

        // obtained > 0
        
        int channelsRead = channelsRequired;
        linkHead = op_head(m_d->file, li);
        if (linkHead) {
            channelsRead = linkHead->channel_count;
        }

        if (channelsRead != channelsRequired) {

            if (obtained > (totalRequired - totalObtained)) {
                // could happen if channel count is unexpected?
                // despite precaution earlier - truncate if so
                obtained = (totalRequired - totalObtained);
            }
            
            float **read =
                allocate_channels<float>(channelsRead, obtained);
            float **toWrite =
                allocate_channels<float>(channelsRequired, obtained);
            
            v_deinterleave(read, fptr, channelsRead, obtained);
            v_reconfigure_channels(toWrite, channelsRequired,
                                   read, channelsRead,
                                   obtained);

            v_interleave(fptr, toWrite, channelsRequired, obtained);
        
            deallocate_channels(read, channelsRead);
            deallocate_channels(toWrite, channelsRequired);
        }
        
        totalObtained += obtained;
        fptr += obtained * channelsRequired;
    }

    return totalObtained;
}

OpusReadStream::~OpusReadStream()
{
    if (m_d->file) {
        op_free(m_d->file);
    }

    delete m_d;
}

}

#endif

