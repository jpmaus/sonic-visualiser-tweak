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

#ifndef SV_DENSE_TIME_VALUE_MODEL_H
#define SV_DENSE_TIME_VALUE_MODEL_H

#include <QObject>

#include "Model.h"

/**
 * Base class for models containing dense two-dimensional data (value
 * against time).  For example, audio waveform data.  Other time-value
 * plot data, especially if editable, will normally go into a
 * SparseTimeValueModel instead even if regularly sampled.
 */

class DenseTimeValueModel : public Model
{
    Q_OBJECT

public:
    DenseTimeValueModel() { }

    virtual ~DenseTimeValueModel() { }

    /**
     * Return the minimum possible value found in this model type.
     * (That is, the minimum that would be valid, not the minimum
     * actually found in a particular model).
     */
    virtual float getValueMinimum() const = 0;

    /**
     * Return the minimum possible value found in this model type.
     * (That is, the minimum that would be valid, not the minimum
     * actually found in a particular model).
     */
    virtual float getValueMaximum() const = 0;

    /**
     * Return the number of distinct channels for this model.
     */
    virtual int getChannelCount() const = 0;

    /**
     * Get the specified set of samples from the given channel of the
     * model in single-precision floating-point format. Returned
     * vector may have fewer samples than requested, if the end of
     * file was reached.
     *
     * If the channel is given as -1, mix all available channels and
     * return the result.
     */
    virtual floatvec_t getData(int channel, sv_frame_t start, sv_frame_t count)
        const = 0;

    /**
     * Get the specified set of samples from given contiguous range of
     * channels of the model in single-precision floating-point
     * format. Returned vector may have fewer samples than requested,
     * if the end of file was reached.
     */
    virtual std::vector<floatvec_t> getMultiChannelData(int fromchannel,
                                                        int tochannel,
                                                        sv_frame_t start,
                                                        sv_frame_t count)
        const = 0;

    bool canPlay() const override { return true; }
    QString getDefaultPlayClipId() const override { return ""; }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions options,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const override;

    QString getTypeName() const override { return tr("Dense Time-Value"); }
};

#endif
