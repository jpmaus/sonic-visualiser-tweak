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

#ifndef SV_DENSE_THREE_DIMENSIONAL_MODEL_H
#define SV_DENSE_THREE_DIMENSIONAL_MODEL_H

#include "Model.h"
#include "TabularModel.h"
#include "base/ColumnOp.h"
#include "base/ZoomConstraint.h"
#include "base/RealTime.h"

#include <QMutex>
#include <QVector>

class DenseThreeDimensionalModel : public Model,
                                   public TabularModel
{
    Q_OBJECT

public:
    /**
     * Return the number of sample frames covered by each column of bins.
     */
    virtual int getResolution() const = 0;

    /**
     * Return the number of columns of bins in the model.
     */
    virtual int getWidth() const = 0;

    /**
     * Return the number of bins in each column.
     */
    virtual int getHeight() const = 0; 

    /**
     * Return the minimum permissible value in each bin.
     */
    virtual float getMinimumLevel() const = 0;

    /**
     * Return the maximum permissible value in each bin.
     */
    virtual float getMaximumLevel() const = 0;

    typedef ColumnOp::Column Column;

    /**
     * Get data from the given column of bin values.
     */
    virtual Column getColumn(int column) const = 0;

    /**
     * Get the single data point from the n'th bin of the given column.
     */
    virtual float getValueAt(int column, int n) const = 0;

    /**
     * Get the name of a given bin (i.e. a label to associate with
     * that bin across all columns).
     */
    virtual QString getBinName(int n) const = 0;

    /**
     * Return true if the bins have values as well as names. If this
     * returns true, getBinValue() may be used to retrieve the values.
     */
    virtual bool hasBinValues() const { return false; }

    /**
     * Return the value of bin n, if any. This is a "vertical scale"
     * value which does not vary from one column to the next. This is
     * only meaningful if hasBinValues() returns true.
     */
    virtual float getBinValue(int n) const { return float(n); }

    /**
     * Obtain the name of the unit of the values returned from
     * getBinValue(), if any.
     */
    virtual QString getBinValueUnit() const { return ""; }

    /**
     * Estimate whether a logarithmic scale might be appropriate for
     * the value scale.
     */
    virtual bool shouldUseLogValueScale() const = 0;

    /**
     * Utility function to query whether a given bin is greater than
     * its (vertical) neighbours.
     */
    bool isLocalPeak(int x, int y) {
        float value = getValueAt(x, y);
        if (y > 0 && value < getValueAt(x, y - 1)) return false;
        if (y < getHeight() - 1 && value < getValueAt(x, y + 1)) return false;
        return true;
    }

    /**
     * Utility function to query whether a given bin is greater than a
     * certain threshold.
     */
    bool isOverThreshold(int x, int y, float threshold) {
        return getValueAt(x, y) > threshold;
    }

    QString getTypeName() const override { return tr("Dense 3-D"); }

    virtual int getCompletion() const override = 0;

    /*
       TabularModel methods.
       This class is non-editable -- subclasses may be editable.
       Row and column are transposed for the tabular view (which is
       "on its side").
     */
    
    int getRowCount() const override { return getWidth(); }
    int getColumnCount() const override { return getHeight() + 2; }

    bool isEditable() const override { return false; }
    Command *getSetDataCommand(int, int, const QVariant &, int) override { return nullptr; }
    Command *getInsertRowCommand(int) override { return nullptr; }
    Command *getRemoveRowCommand(int) override { return nullptr; }
    
    QString getHeading(int column) const override
    {
        switch (column) {
        case 0: return tr("Time");
        case 1: return tr("Frame");
        default:
            QString name = getBinName(column - 2);
            if (name == "") {
                name = tr("(bin %1)").arg(column - 2);
            }
            return name;
        }
    }

    QVariant getData(int row, int column, int) const 
    override {
        switch (column) {
        case 0: {
            RealTime rt = RealTime::frame2RealTime
                (row * getResolution() + getStartFrame(), getSampleRate());
            return rt.toText().c_str();
        }
        case 1:
            return int(row * getResolution() + getStartFrame());
        default:
            return getValueAt(row, column - 2);
        }
    }

    bool isColumnTimeValue(int col) const override {
        return col < 2;
    }
    SortType getSortType(int) const override {
        return SortNumeric;
    }

    sv_frame_t getFrameForRow(int row) const override {
        return sv_frame_t(row) * getResolution() + getStartFrame();
    }
    int getRowForFrame(sv_frame_t frame) const override {
        return int((frame - getStartFrame()) / getResolution());
    }

protected:
    DenseThreeDimensionalModel() { }
};

#endif
