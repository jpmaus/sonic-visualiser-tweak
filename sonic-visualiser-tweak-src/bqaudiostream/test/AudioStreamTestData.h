/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */
/* Copyright Chris Cannam - All Rights Reserved */

#ifndef AUDIOSTREAM_TEST_DATA_H
#define AUDIOSTREAM_TEST_DATA_H

#include "bqaudiostream/AudioWriteStreamFactory.h"
#include "bqaudiostream/AudioWriteStream.h"

#include <cmath>

#include <iostream>

namespace breakfastquay {

/**
 * Class that generates a single fixed test pattern to a given sample
 * rate and number of channels.
 *
 * The test pattern is two seconds long and consists of:
 *
 * -- in channel 0, a 600Hz sinusoid with peak amplitude 1.0
 *
 * -- in channel 1, four triangular forms with peaks at +1.0, -1.0,
 *    +1.0, -1.0 respectively, of 10ms width, starting at 0.0, 0.5,
 *    1.0 and 1.5 seconds; silence elsewhere
 *
 * -- in subsequent channels, a flat DC offset at +(channelNo / 20.0)
 */
class AudioStreamTestData
{
public:
    AudioStreamTestData(float rate, int channels) :
	m_channelCount(channels),
	m_duration(2.0),
	m_sampleRate(rate),
	m_sinFreq(600.0),
	m_pulseFreq(2)
    {
	m_frameCount = lrint(m_duration * m_sampleRate);
	m_data = new float[m_frameCount * m_channelCount];
	m_pulseWidth = 0.01 * m_sampleRate;
	generate();
    }

    ~AudioStreamTestData() {
	delete[] m_data;
    }

    void generate() {

	float hpw = m_pulseWidth / 2.0;

	for (int i = 0; i < m_frameCount; ++i) {
	    for (int c = 0; c < m_channelCount; ++c) {

		float s = 0.f;

		if (c == 0) {

		    float phase = (i * m_sinFreq * 2.f * M_PI) / m_sampleRate;
		    s = sinf(phase);

		} else if (c == 1) {

		    int pulseNo = int((i * m_pulseFreq) / m_sampleRate);
		    int index = (i * m_pulseFreq) - (m_sampleRate * pulseNo);
		    if (index < m_pulseWidth) {
			s = 1.0 - fabsf(hpw - index) / hpw;
			if (pulseNo % 2) s = -s;
		    }

		} else {

		    s = c / 20.0;
		}

		m_data[i * m_channelCount + c] = s;
	    }
	}
    }

    float *getInterleavedData() const {
	return m_data;
    }

    int getFrameCount() const { 
	return m_frameCount;
    }

    int getChannelCount() const {
	return m_channelCount;
    }

    float getSampleRate () const {
	return m_sampleRate;
    }

    float getDuration() const { // seconds
	return m_duration;
    }

    void writeToFile(std::string filename) {
	AudioWriteStream *ws = AudioWriteStreamFactory::createWriteStream
	    (filename, m_channelCount, lrint(m_sampleRate));
	ws->putInterleavedFrames(m_frameCount, m_data);
	delete ws;
    }

private:
    float *m_data;
    int m_frameCount;
    int m_channelCount;
    float m_duration;
    float m_sampleRate;
    float m_sinFreq;
    float m_pulseFreq;
    float m_pulseWidth;
};

}

#endif

