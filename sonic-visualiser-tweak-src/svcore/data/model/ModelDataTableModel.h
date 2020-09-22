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

#ifndef SV_MODEL_DATA_TABLE_MODEL_H
#define SV_MODEL_DATA_TABLE_MODEL_H

#include <QAbstractItemModel>

#include <vector>

#include "base/BaseTypes.h"

#include "TabularModel.h"
#include "Model.h"

class TabularModel;
class Command;

class ModelDataTableModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ModelDataTableModel(ModelId modelId); // a TabularModel
    virtual ~ModelDataTableModel();

    QVariant data(const QModelIndex &index, int role) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    bool insertRow(int row, const QModelIndex &parent = QModelIndex());
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    Qt::ItemFlags flags(const QModelIndex &index) const override;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex &index) const override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QModelIndex getModelIndexForFrame(sv_frame_t frame) const;
    sv_frame_t getFrameForModelIndex(const QModelIndex &) const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    QModelIndex findText(QString text) const;

    void setCurrentRow(int row);
    int getCurrentRow() const;

signals:
    void frameSelected(int);
    void addCommand(Command *);
    void currentChanged(const QModelIndex &);
    void modelRemoved();

protected slots:
    void modelChanged(ModelId);
    void modelChangedWithin(ModelId, sv_frame_t, sv_frame_t);

protected:
    std::shared_ptr<TabularModel> getTabularModel() const {
        return ModelById::getAs<TabularModel>(m_model);
    }
    
    ModelId m_model;
    int m_sortColumn;
    Qt::SortOrder m_sortOrdering;
    int m_currentRow;
    typedef std::vector<int> RowList;
    mutable RowList m_sort;
    mutable RowList m_rsort;
    int getSorted(int row) const;
    int getUnsorted(int row) const;
    void resort() const;
    void resortNumeric() const;
    void resortAlphabetical() const;
    void clearSort();
};

#endif
