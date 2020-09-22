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

/*
   This is a modified version of a source file from the 
   Rosegarden MIDI and audio sequencer and notation editor.
   This file copyright 2000-2006 Chris Cannam and Richard Bown.
*/

#ifndef SV_LADSPAPLUGININSTANCE_H
#define SV_LADSPAPLUGININSTANCE_H

#include <vector>
#include <set>
#include <QString>

#include "api/ladspa.h"
#include "RealTimePluginInstance.h"
#include "base/BaseTypes.h"

// LADSPA plugin instance.  LADSPA is a variable block size API, but
// for one reason and another it's more convenient to use a fixed
// block size in this wrapper.
//
class LADSPAPluginInstance : public RealTimePluginInstance
{
public:
    virtual ~LADSPAPluginInstance();

    bool isOK() const override { return m_instanceHandles.size() != 0; }

    int getClientId() const { return m_client; }
    QString getPluginIdentifier() const override { return m_identifier; }
    int getPosition() const { return m_position; }

    std::string getIdentifier() const override;
    std::string getName() const override;
    std::string getDescription() const override;
    std::string getMaker() const override;
    int getPluginVersion() const override;
    std::string getCopyright() const override;

    void run(const RealTime &rt, int count = 0) override;

    int getParameterCount() const override;
    void setParameterValue(int parameter, float value) override;
    float getParameterValue(int parameter) const override;
    float getParameterDefault(int parameter) const override;
    int getParameterDisplayHint(int parameter) const override;
    
    ParameterList getParameterDescriptors() const override;
    float getParameter(std::string) const override;
    void setParameter(std::string, float) override;

    int getBufferSize() const override { return m_blockSize; }
    int getAudioInputCount() const override { return int(m_instanceCount * m_audioPortsIn.size()); }
    int getAudioOutputCount() const override { return int(m_instanceCount * m_audioPortsOut.size()); }
    sample_t **getAudioInputBuffers() override { return m_inputBuffers; }
    sample_t **getAudioOutputBuffers() override { return m_outputBuffers; }

    int getControlOutputCount() const override { return int(m_controlPortsOut.size()); }
    float getControlOutputValue(int n) const override;

    bool isBypassed() const override { return m_bypassed; }
    void setBypassed(bool bypassed) override { m_bypassed = bypassed; }

    sv_frame_t getLatency() override;

    void silence() override;
    void setIdealChannelCount(int channels) override; // may re-instantiate

    std::string getType() const override { return "LADSPA Real-Time Plugin"; }

protected:
    // To be constructed only by LADSPAPluginFactory
    friend class LADSPAPluginFactory;

    // Constructor that creates the buffers internally
    // 
    LADSPAPluginInstance(RealTimePluginFactory *factory,
                         int client,
                         QString identifier,
                         int position,
                         sv_samplerate_t sampleRate,
                         int blockSize,
                         int idealChannelCount,
                         const LADSPA_Descriptor* descriptor);

    void init(int idealChannelCount = 0);
    void instantiate(sv_samplerate_t sampleRate);
    void cleanup();
    void activate();
    void deactivate();

    // Connection of data (and behind the scenes control) ports
    //
    void connectPorts();
    
    int                        m_client;
    int                        m_position;
    std::vector<LADSPA_Handle> m_instanceHandles;
    int                        m_instanceCount;
    const LADSPA_Descriptor   *m_descriptor;

    std::vector<std::pair<int, LADSPA_Data*> > m_controlPortsIn;
    std::vector<std::pair<int, LADSPA_Data*> > m_controlPortsOut;

    std::vector<int>          m_audioPortsIn;
    std::vector<int>          m_audioPortsOut;

    int                       m_blockSize;
    sample_t                **m_inputBuffers;
    sample_t                **m_outputBuffers;
    bool                      m_ownBuffers;
    sv_samplerate_t           m_sampleRate;
    float                    *m_latencyPort;
    bool                      m_run;
    
    bool                      m_bypassed;
};

#endif // _LADSPAPLUGININSTANCE_H_

