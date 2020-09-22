/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_BASIC_COMPRESSED_DENSE_THREE_DIMENSIONAL_MODEL_H
#define SV_BASIC_COMPRESSED_DENSE_THREE_DIMENSIONAL_MODEL_H

#include "DenseThreeDimensionalModel.h"

#include <QReadWriteLock>

#include <vector>

class BasicCompressedDenseThreeDimensionalModel : public DenseThreeDimensionalModel
{
    Q_OBJECT

public:

    // BasicCompressedDenseThreeDimensionalModel supports a basic
    // compression method that reduces the size of multirate data
    // (e.g. wavelet transform outputs) that are stored as plain 3d
    // grids by about 60% or thereabouts.  However, it can only be
    // used for models whose columns are set in order from 0 and never
    // subsequently changed.  For a model that is actually going to be
    // edited, you need an EditableDenseThreeDimensionalModel.

    BasicCompressedDenseThreeDimensionalModel(sv_samplerate_t sampleRate,
                                              int resolution,
                                              int height,
                                              bool notifyOnAdd = true);

    bool isOK() const override;
    bool isReady(int *completion = 0) const override;
    void setCompletion(int completion, bool update = true);
    int getCompletion() const override;

    sv_samplerate_t getSampleRate() const override;
    sv_frame_t getStartFrame() const override;
    sv_frame_t getTrueEndFrame() const override;

    /**
     * Set the frame offset of the first column.
     */
    virtual void setStartFrame(sv_frame_t);

    /**
     * Return the number of sample frames covered by each set of bins.
     */
    int getResolution() const override;

    /**
     * Set the number of sample frames covered by each set of bins.
     */
    virtual void setResolution(int sz);

    /**
     * Return the number of columns.
     */
    int getWidth() const override;

    /**
     * Return the number of bins in each column.
     */
    int getHeight() const override;

    /**
     * Set the number of bins in each column.
     *
     * You can set (via setColumn) a vector of any length as a column,
     * but any column being retrieved will be resized to this height
     * (or the height that was supplied to the constructor, if this is
     * never called) on retrieval. That is, the model owner determines
     * the height of the model at a single stroke; the columns
     * themselves don't have any effect on the height of the model.
     */
    virtual void setHeight(int sz);

    /**
     * Return the minimum value of the value in each bin.
     */
    float getMinimumLevel() const override;

    /**
     * Set the minimum value of the value in a bin.
     */
    virtual void setMinimumLevel(float sz);

    /**
     * Return the maximum value of the value in each bin.
     */
    float getMaximumLevel() const override;

    /**
     * Set the maximum value of the value in a bin.
     */
    virtual void setMaximumLevel(float sz);

    /**
     * Get the set of bin values at the given column.
     */
    Column getColumn(int x) const override;

    /**
     * Get a single value, from the n'th bin of the given column.
     */
    float getValueAt(int x, int n) const override;

    /**
     * Set the entire set of bin values at the given column.
     */
    virtual void setColumn(int x, const Column &values);

    /**
     * Return the name of bin n. This is a single label per bin that
     * does not vary from one column to the next.
     */
    QString getBinName(int n) const override;

    /**
     * Set the name of bin n.
     */
    virtual void setBinName(int n, QString);

    /**
     * Set the names of all bins.
     */
    virtual void setBinNames(std::vector<QString> names);

    /**
     * Return true if the bins have values as well as names. (The
     * values may have been derived from the names, e.g. by parsing
     * numbers from them.) If this returns true, getBinValue() may be
     * used to retrieve the values.
     */
    bool hasBinValues() const override;

    /**
     * Return the value of bin n, if any. This is a "vertical scale"
     * value which does not vary from one column to the next. This is
     * only meaningful if hasBinValues() returns true.
     */
    float getBinValue(int n) const override;

    /**
     * Set the values of all bins (separate from their labels). These
     * are "vertical scale" values which do not vary from one column
     * to the next.
     */
    virtual void setBinValues(std::vector<float> values);

    /**
     * Obtain the name of the unit of the values returned from
     * getBinValue(), if any.
     */
    QString getBinValueUnit() const override;

    /**
     * Set the name of the unit of the values return from
     * getBinValue() if any.
     */
    virtual void setBinValueUnit(QString unit);

    /**
     * Return true if the distribution of values in the bins is such
     * as to suggest a log scale (mapping to colour etc) may be better
     * than a linear one.
     */
    bool shouldUseLogValueScale() const override;

    QString getTypeName() const override { return tr("Editable Dense 3-D"); }

    QString toDelimitedDataString(QString delimiter,
                                  DataExportOptions options,
                                  sv_frame_t startFrame,
                                  sv_frame_t duration) const override;

    void toXml(QTextStream &out,
                       QString indent = "",
                       QString extraAttributes = "") const override;

protected:
    typedef std::vector<Column> ValueMatrix;
    ValueMatrix m_data;

    // m_trunc is used for simple compression.  If at least the top N
    // elements of column x (for N = some proportion of the column
    // height) are equal to those of an earlier column x', then
    // m_trunc[x] will contain x-x' and column x will be truncated so
    // as to remove the duplicate elements.  If the equal elements are
    // at the bottom, then m_trunc[x] will contain x'-x (a negative
    // value).  If m_trunc[x] is 0 then the whole of column x is
    // stored.
    std::vector<signed char> m_trunc;
    void truncateAndStore(int index, const Column & values);
    Column expandAndRetrieve(int index) const;
    Column rightHeight(const Column &c) const;

    std::vector<QString> m_binNames;
    std::vector<float> m_binValues;
    QString m_binValueUnit;

    sv_frame_t m_startFrame;
    sv_samplerate_t m_sampleRate;
    int m_resolution;
    int m_yBinCount;
    float m_minimum;
    float m_maximum;
    bool m_haveExtents;
    bool m_notifyOnAdd;
    sv_frame_t m_sinceLastNotifyMin;
    sv_frame_t m_sinceLastNotifyMax;
    int m_completion;

    mutable QReadWriteLock m_lock;
};

#endif
