/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef WRITABLE_WAVE_FILE_MODEL_H
#define WRITABLE_WAVE_FILE_MODEL_H

#include "WaveFileModel.h"
#include "ReadOnlyWaveFileModel.h"
#include "PowerOfSqrtTwoZoomConstraint.h"

class WavFileWriter;
class WavFileReader;

class WritableWaveFileModel : public WaveFileModel
{
    Q_OBJECT

public:
    enum class Normalisation { None, Peak };

    /**
     * Create a WritableWaveFileModel of the given sample rate and
     * channel count, storing data in a new float-type extended WAV
     * file with the given path. If path is the empty string, the data
     * will be stored in a newly-created temporary file.
     *
     * If normalisation == None, sample values will be written
     * verbatim, and will be ready to read as soon as they have been
     * written. Otherwise samples will be normalised on writing; this
     * will require an additional pass and temporary file, and no
     * samples will be available to read until after writeComplete()
     * has returned.
     */
    WritableWaveFileModel(QString path,
                          sv_samplerate_t sampleRate,
                          int channels,
                          Normalisation normalisation);
    
    /**
     * Create a WritableWaveFileModel of the given sample rate and
     * channel count, storing data in a new float-type extended WAV
     * file in a temporary location. This is equivalent to passing an
     * empty path to the constructor above.
     *
     * If normalisation == None, sample values will be written
     * verbatim, and will be ready to read as soon as they have been
     * written. Otherwise samples will be normalised on writing; this
     * will require an additional pass and temporary file, and no
     * samples will be available to read until after writeComplete()
     * has returned.
     */
    WritableWaveFileModel(sv_samplerate_t sampleRate,
                          int channels,
                          Normalisation normalisation);

    /**
     * Create a WritableWaveFileModel of the given sample rate and
     * channel count, storing data in a new float-type extended WAV
     * file in a temporary location, and applying no normalisation.
     *
     * This is equivalent to passing an empty path and
     * Normalisation::None to the first constructor above.
     */
    WritableWaveFileModel(sv_samplerate_t sampleRate,
                          int channels);

    ~WritableWaveFileModel();

    /**
     * Call addSamples to append a block of samples to the end of the
     * file.
     *
     * This function only appends the samples to the file being
     * written; it does not update the model's view of the samples in
     * that file. That is, it updates the file on disc but the model
     * itself does not change its content. This is because re-reading
     * the file to update the model may be more expensive than adding
     * the samples in the first place. If you are writing small
     * numbers of samples repeatedly, you probably only want the model
     * to update periodically rather than after every write.
     *
     * Call updateModel() periodically to tell the model to update its
     * own view of the samples in the file being written.
     *
     * Call setWriteProportion() periodically if the file being
     * written has known duration and you want the model to be able to
     * report the write progress as a percentage.
     *
     * Call writeComplete() when the file has been completely written.
     */
    virtual bool addSamples(const float *const *samples, sv_frame_t count);

    /**
     * Tell the model to update its own (read) view of the (written)
     * file. May cause modelChanged() and modelChangedWithin() to be
     * emitted. See the comment to addSamples above for rationale.
     */
    void updateModel();
    
    /**
     * Set the proportion of the file which has been written so far,
     * as a percentage. This may be used to indicate progress.
     *
     * Note that this differs from the "completion" percentage
     * reported through isReady()/getCompletion(). That percentage is
     * updated when "internal processing has advanced... but the model
     * has not changed externally", i.e. it reports progress in
     * calculating the initial state of a model. In contrast, an
     * update to setWriteProportion corresponds to a change in the
     * externally visible state of the model (i.e. it contains more
     * data than before).
     */
    void setWriteProportion(int proportion);

    /**
     * Indicate that writing is complete. You should call this even if
     * you have never called setWriteProportion() or updateModel().
     */
    void writeComplete();

    static const int PROPORTION_UNKNOWN;
    
    /**
     * Get the proportion of the file which has been written so far,
     * as a percentage. Return PROPORTION_UNKNOWN if unknown.
     */
    int getWriteProportion() const;
    
    bool isOK() const override;
    
    /**
     * Return the generation completion percentage of this model. This
     * is always 100, because the model is always in a complete state
     * -- it just contains varying amounts of data depending on how
     * much has been written.
     */
    int getCompletion() const override { return 100; }

    const ZoomConstraint *getZoomConstraint() const override {
        static PowerOfSqrtTwoZoomConstraint zc;
        return &zc;
    }

    sv_frame_t getFrameCount() const override;
    int getChannelCount() const override { return m_channels; }
    sv_samplerate_t getSampleRate() const override { return m_sampleRate; }
    sv_samplerate_t getNativeRate() const override { return m_sampleRate; }

    QString getTitle() const override {
        if (m_model) return m_model->getTitle();
        else return "";
    } 
    QString getMaker() const override {
        if (m_model) return m_model->getMaker();
        else return "";
    }
    QString getLocation() const override {
        if (m_model) return m_model->getLocation();
        else return "";
    }

    float getValueMinimum() const override { return -1.0f; }
    float getValueMaximum() const override { return  1.0f; }

    sv_frame_t getStartFrame() const override { return m_startFrame; }
    sv_frame_t getTrueEndFrame() const override { return m_startFrame + getFrameCount(); }

    void setStartFrame(sv_frame_t startFrame) override;

    floatvec_t getData(int channel, sv_frame_t start, sv_frame_t count) const override;

    std::vector<floatvec_t> getMultiChannelData(int fromchannel, int tochannel, sv_frame_t start, sv_frame_t count) const override;

    int getSummaryBlockSize(int desired) const override;

    void getSummaries(int channel, sv_frame_t start, sv_frame_t count,
                              RangeBlock &ranges, int &blockSize) const override;

    Range getSummary(int channel, sv_frame_t start, sv_frame_t count) const override;

    QString getTypeName() const override { return tr("Writable Wave File"); }

    void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const override;

signals:
    void writeCompleted(ModelId);

protected slots:
    void componentModelChanged(ModelId);
    void componentModelChangedWithin(ModelId, sv_frame_t, sv_frame_t);
    
protected:
    ReadOnlyWaveFileModel *m_model;

    /** When normalising, this writer is used to write verbatim
     *  samples to the temporary file prior to
     *  normalisation. Otherwise it's null
     */
    WavFileWriter *m_temporaryWriter;
    QString m_temporaryPath;

    /** When not normalising, this writer is used to write verbatim
     *  samples direct to the target file. When normalising, it is
     *  used to write normalised samples to the target after the
     *  temporary file has been completed. But it is still created on
     *  initialisation, so that there is a file header ready for the
     *  reader to address.
     */
    WavFileWriter *m_targetWriter;
    QString m_targetPath;

    WavFileReader *m_reader;
    Normalisation m_normalisation;
    sv_samplerate_t m_sampleRate;
    int m_channels;
    sv_frame_t m_frameCount;
    sv_frame_t m_startFrame;
    int m_proportion;

private:
    void init(QString path = "");
    void normaliseToTarget();
};

#endif

