/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_TABULAR_MODEL_H
#define SV_TABULAR_MODEL_H

#include <QVariant>
#include <QString>

#include "base/RealTime.h"

class Command;

/**
 * TabularModel is an abstract base class for models that support
 * direct access to data in a tabular form.  A model that implements
 * TabularModel may be displayed and, perhaps, edited in a data
 * spreadsheet window.
 *
 * This is very like a cut-down QAbstractItemModel.  It assumes a
 * relationship between row number and frame time.
 */
class TabularModel
{
public:
    virtual ~TabularModel() { }

    /**
     * Return the number of rows (items) in the model.
     */
    virtual int getRowCount() const = 0;

    /** 
     * Return the number of columns (values/labels/etc per item).
     */
    virtual int getColumnCount() const = 0;

    /**
     * Return the heading for a given column, e.g. "Time" or "Value".
     * These are shown directly to the user, so must be translated
     * already.
     */
    virtual QString getHeading(int column) const = 0;

    enum { SortRole = Qt::UserRole };
    enum SortType { SortNumeric, SortAlphabetical };

    /**
     * Get the value in the given cell, for the given role. The role
     * is actually a Qt::ItemDataRole.
     */
    virtual QVariant getData(int row, int column, int role) const = 0;

    /**
     * Return true if the column is the frame time of the item, or an
     * alternative representation of it (i.e. anything that has the
     * same sort order). Duration is not a time value by this meaning.
     */
    virtual bool isColumnTimeValue(int col) const = 0;

    /**
     * Return the sort type (numeric or alphabetical) for the column.
     */
    virtual SortType getSortType(int col) const = 0;

    /**
     * Return the frame time for the given row.
     */
    virtual sv_frame_t getFrameForRow(int row) const = 0;

    /** 
     * Return the number of the first row whose frame time is not less
     * than the given one. If there is none, return getRowCount().
     */
    virtual int getRowForFrame(sv_frame_t frame) const = 0;

    /**
     * Return true if the model is user-editable, false otherwise.
     */
    virtual bool isEditable() const = 0;

    /**
     * Return a command to set the value in the given cell, for the
     * given role, to the contents of the supplied variant.
     *
     * If the model is not editable or the cell or value is out of
     * range, return nullptr.
     */
    virtual Command *getSetDataCommand(int row, int column,
                                       const QVariant &, int role) = 0;

    /**
     * Return a command to insert a new row before the row with the
     * given index.
     *
     * If the model is not editable or the index is out of range,
     * return nullptr.
     */
    virtual Command *getInsertRowCommand(int beforeRow) = 0;

    /**
     * Return a command to delete the row with the given index.
     *
     * If the model is not editable or the index is out of range,
     * return nullptr.
     */
    virtual Command *getRemoveRowCommand(int row) = 0;

protected:
    // Helpers
    
    static QVariant adaptFrameForRole(sv_frame_t frame,
                                      sv_samplerate_t rate,
                                      int role) {
        if (role == SortRole) return int(frame);
        RealTime rt = RealTime::frame2RealTime(frame, rate);
        if (role == Qt::EditRole) return rt.toString().c_str();
        else return rt.toText().c_str();
    }

    static QVariant adaptValueForRole(float value,
                                      QString unit,
                                      int role) {
        if (role == SortRole || role == Qt::EditRole) return value;
        else return QString("%1 %2").arg(value).arg(unit);
    }
};

#endif
