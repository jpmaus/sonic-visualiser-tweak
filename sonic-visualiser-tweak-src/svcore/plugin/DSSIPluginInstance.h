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
   This file copyright 2000-2006 Chris Cannam.
*/

#ifndef SV_DSSIPLUGININSTANCE_H
#define SV_DSSIPLUGININSTANCE_H

#define DSSI_API_LEVEL 2

#include <vector>
#include <set>
#include <map>
#include <QString>
#include <QMutex>

#include "api/dssi.h"

#include "base/RingBuffer.h"
#include "base/Thread.h"
#include "RealTimePluginInstance.h"
#include "base/Scavenger.h"

class DSSIPluginInstance : public RealTimePluginInstance
{
public:
    virtual ~DSSIPluginInstance();

    bool isOK() const override { return m_instanceHandle != 0; }

    int getClientId() const { return m_client; }
    QString getPluginIdentifier() const override { return m_identifier; }
    int getPosition() const { return m_position; }

    std::string getIdentifier() const override;
    std::string getName() const override;
    std::string getDescription() const override;
    std::string getMaker() const override;
    int getPluginVersion() const override;
    std::string getCopyright() const override;

    void run(const RealTime &, int count = 0) override;

    int getParameterCount() const override;
    void setParameterValue(int parameter, float value) override;
    float getParameterValue(int parameter) const override;
    float getParameterDefault(int parameter) const override;
    int getParameterDisplayHint(int parameter) const override;

    ParameterList getParameterDescriptors() const override;
    float getParameter(std::string) const override;
    void setParameter(std::string, float) override;

    std::string configure(std::string key, std::string value) override;
    void sendEvent(const RealTime &eventTime,
                           const void *event) override;
    void clearEvents() override;

    int getBufferSize() const override { return m_blockSize; }
    int getAudioInputCount() const override { return (int)m_audioPortsIn.size(); }
    int getAudioOutputCount() const override { return m_idealChannelCount; }
    sample_t **getAudioInputBuffers() override { return m_inputBuffers; }
    sample_t **getAudioOutputBuffers() override { return m_outputBuffers; }

    int getControlOutputCount() const override { return (int)m_controlPortsOut.size(); }
    float getControlOutputValue(int n) const override;

    ProgramList getPrograms() const override;
    std::string getCurrentProgram() const override;
    std::string getProgram(int bank, int program) const override;
    virtual int getProgram(std::string name) const;
    void selectProgram(std::string program) override;

    bool isBypassed() const override { return m_bypassed; }
    void setBypassed(bool bypassed) override { m_bypassed = bypassed; }

    sv_frame_t getLatency() override;

    void silence() override;
    void discardEvents() override;
    void setIdealChannelCount(int channels) override; // may re-instantiate

    virtual bool isInGroup() const { return m_grouped; }
    virtual void detachFromGroup();

    std::string getType() const override { return "DSSI Real-Time Plugin"; }

protected:
    // To be constructed only by DSSIPluginFactory
    friend class DSSIPluginFactory;

    // Constructor that creates the buffers internally
    // 
    DSSIPluginInstance(RealTimePluginFactory *factory,
                       int client,
                       QString identifier,
                       int position,
                       sv_samplerate_t sampleRate,
                       int blockSize,
                       int idealChannelCount,
                       const DSSI_Descriptor* descriptor);
    
    void init();
    void instantiate(sv_samplerate_t sampleRate);
    void cleanup();
    void activate();
    void deactivate();
    void connectPorts();

    bool handleController(snd_seq_event_t *ev);
    void setPortValueFromController(int portNumber, int controlValue);
    void selectProgramAux(std::string program, bool backupPortValues);
    void checkProgramCache() const;

    void initialiseGroupMembership();
    void runGrouped(const RealTime &);

    // For use in DSSIPluginFactory (set in the DSSI_Host_Descriptor):
    static int requestMidiSend(LADSPA_Handle instance,
                               unsigned char ports,
                               unsigned char channels);
    static void midiSend(LADSPA_Handle instance,
                         snd_seq_event_t *events,
                         unsigned long eventCount);
    static int requestNonRTThread(LADSPA_Handle instance,
                                  void (*runFunction)(LADSPA_Handle));

    int                        m_client;
    int                        m_position;
    LADSPA_Handle              m_instanceHandle;
    const DSSI_Descriptor     *m_descriptor;

    std::vector<std::pair<int, LADSPA_Data*> > m_controlPortsIn;
    std::vector<std::pair<int, LADSPA_Data*> > m_controlPortsOut;

    std::vector<LADSPA_Data>  m_backupControlPortsIn;

    std::map<int, int>        m_controllerMap;

    std::vector<int>          m_audioPortsIn;
    std::vector<int>          m_audioPortsOut;

    struct ProgramControl {
        int msb;
        int lsb;
        int program;
    };
    ProgramControl m_pending;

    struct ProgramDescriptor {
        int bank;
        int program;
        std::string name;
    };
    mutable std::vector<ProgramDescriptor> m_cachedPrograms;
    mutable bool m_programCacheValid;

    RingBuffer<snd_seq_event_t> m_eventBuffer;

    int                       m_blockSize;
    sample_t                **m_inputBuffers;
    sample_t                **m_outputBuffers;
    bool                      m_ownBuffers;
    int                       m_idealChannelCount;
    int                       m_outputBufferCount;
    sv_samplerate_t           m_sampleRate;
    float                    *m_latencyPort;
    bool                      m_run;
    
    bool                      m_bypassed;
    std::string               m_program;
    bool                      m_grouped;
    RealTime                  m_lastRunTime;

    RealTime                  m_lastEventSendTime;
    bool                      m_haveLastEventSendTime;

    QMutex                    m_processLock;

    typedef std::set<DSSIPluginInstance *> PluginSet;
    typedef std::map<QString, PluginSet> GroupMap;
    static GroupMap m_groupMap;
    static snd_seq_event_t **m_groupLocalEventBuffers;
    static size_t m_groupLocalEventBufferCount;

    static Scavenger<ScavengerArrayWrapper<snd_seq_event_t *> > m_bufferScavenger;

    class NonRTPluginThread : public Thread
    {
    public:
        NonRTPluginThread(LADSPA_Handle handle,
                          void (*runFunction)(LADSPA_Handle)) :
            m_handle(handle),
            m_runFunction(runFunction),
            m_exiting(false) { }

        void run() override;
        void setExiting() { m_exiting = true; }

    protected:
        LADSPA_Handle m_handle;
        void (*m_runFunction)(LADSPA_Handle);
        bool m_exiting;
    };
    static std::map<LADSPA_Handle, std::set<NonRTPluginThread *> > m_threads;
};

#endif // _DSSIPLUGININSTANCE_H_

