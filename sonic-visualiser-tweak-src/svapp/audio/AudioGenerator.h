/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_AUDIO_GENERATOR_H
#define SV_AUDIO_GENERATOR_H

class NoteModel;
class FlexiNoteModel;
class DenseTimeValueModel;
class SparseOneDimensionalModel;
class Playable;
class ClipMixer;
class ContinuousSynth;

#include <QObject>
#include <QMutex>

#include <set>
#include <map>
#include <vector>

#include "base/BaseTypes.h"
#include "data/model/Model.h"

class AudioGenerator : public QObject
{
    Q_OBJECT

public:
    AudioGenerator();
    virtual ~AudioGenerator();

    /**
     * Add a data model to be played from and initialise any necessary
     * audio generation code.  Returns true if the model will be
     * played.  The model will be added regardless of the return
     * value.
     */
    virtual bool addModel(ModelId model);

    /**
     * Remove a model.
     */
    virtual void removeModel(ModelId model);

    /**
     * Remove all models.
     */
    virtual void clearModels();

    /**
     * Reset playback, clearing buffers and the like.
     */
    virtual void reset();

    /**
     * Set the target channel count.  The buffer parameter to mixModel
     * must always point to at least this number of arrays.
     */
    virtual void setTargetChannelCount(int channelCount);

    /**
     * Return the internal processing block size.  The frameCount
     * argument to all mixModel calls must be a multiple of this
     * value.
     */
    virtual sv_frame_t getBlockSize() const;

    /**
     * Mix a single model into an output buffer.
     */
    virtual sv_frame_t mixModel(ModelId model,
                                sv_frame_t startFrame,
                                sv_frame_t frameCount,
                                float **buffer,
                                sv_frame_t fadeIn = 0,
                                sv_frame_t fadeOut = 0);

    /**
     * Specify that only the given set of models should be played.
     */
    virtual void setSoloModelSet(std::set<ModelId>s);

    /**
     * Specify that all models should be played as normal (if not
     * muted).
     */
    virtual void clearSoloModelSet();

protected slots:
    void playClipIdChanged(int playableId, QString);

protected:
    sv_samplerate_t m_sourceSampleRate;
    int m_targetChannelCount;
    int m_waveType;

    bool m_soloing;
    std::set<ModelId> m_soloModelSet;

    struct NoteOff {

        NoteOff(float _freq, sv_frame_t _offFrame, sv_frame_t _onFrame) :
            frequency(_freq), offFrame(_offFrame), onFrame(_onFrame) { }

        float frequency;
        sv_frame_t offFrame;

        // This is the frame at which the note whose note-off appears
        // here began. It is used to determine when we should silence
        // a note because the playhead has jumped back in time - if
        // the current frame for rendering is earlier than this one,
        // then we should end and discard the note
        //
        sv_frame_t onFrame;

        struct Comparator {
            bool operator()(const NoteOff &n1, const NoteOff &n2) const {
                if (n1.offFrame != n2.offFrame) {
                    return n1.offFrame < n2.offFrame;
                } else if (n1.onFrame != n2.onFrame) {
                    return n1.onFrame < n2.onFrame;
                } else {
                    return n1.frequency < n2.frequency;
                }
            }
        };
    };


    typedef std::map<ModelId, ClipMixer *> ClipMixerMap;

    typedef std::multiset<NoteOff, NoteOff::Comparator> NoteOffSet;
    typedef std::map<ModelId, NoteOffSet> NoteOffMap;

    typedef std::map<ModelId, ContinuousSynth *> ContinuousSynthMap;

    QMutex m_mutex;

    ClipMixerMap m_clipMixerMap;
    NoteOffMap m_noteOffs;
    static QString m_sampleDir;

    ContinuousSynthMap m_continuousSynthMap;

    bool usesClipMixer(ModelId);
    bool wantsQuieterClips(ModelId);
    bool usesContinuousSynth(ModelId);

    ClipMixer *makeClipMixerFor(ModelId model);
    ContinuousSynth *makeSynthFor(ModelId model);

    static void initialiseSampleDir();

    virtual sv_frame_t mixDenseTimeValueModel
    (ModelId model, sv_frame_t startFrame, sv_frame_t frameCount,
     float **buffer, float gain, float pan, sv_frame_t fadeIn, sv_frame_t fadeOut);

    virtual sv_frame_t mixClipModel
    (ModelId model, sv_frame_t startFrame, sv_frame_t frameCount,
     float **buffer, float gain, float pan);

    virtual sv_frame_t mixContinuousSynthModel
    (ModelId model, sv_frame_t startFrame, sv_frame_t frameCount,
     float **buffer, float gain, float pan);
    
    static const sv_frame_t m_processingBlockSize;

    float **m_channelBuffer;
    sv_frame_t m_channelBufSiz;
    int m_channelBufCount;
};

#endif

