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

#include "ModelDataTableModel.h"

#include "TabularModel.h"
#include "Model.h"

#include <map>
#include <algorithm>
#include <iostream>

ModelDataTableModel::ModelDataTableModel(ModelId m) :
    m_model(m),
    m_sortColumn(0),
    m_sortOrdering(Qt::AscendingOrder),
    m_currentRow(0)
{
    auto model = ModelById::get(m);
    if (model) {
        connect(model.get(), SIGNAL(modelChanged(ModelId)),
                this, SLOT(modelChanged(ModelId)));
        connect(model.get(), SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
                this, SLOT(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
    }
}

ModelDataTableModel::~ModelDataTableModel()
{
}

QVariant
ModelDataTableModel::data(const QModelIndex &index, int role) const
{
    auto model = getTabularModel();
    if (!model) return QVariant();
    if (role != Qt::EditRole && role != Qt::DisplayRole) return QVariant();
    if (!index.isValid()) return QVariant();
    QVariant d = model->getData(getUnsorted(index.row()), index.column(), role);
    return d;
}

bool
ModelDataTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    auto model = getTabularModel();
    if (!model) return false;
    if (!index.isValid()) return false;
    Command *command = model->getSetDataCommand(getUnsorted(index.row()),
                                                  index.column(),
                                                  value, role);
    if (command) {
        emit addCommand(command);
        return true;
    } else {
        return false;
    }
}

bool
ModelDataTableModel::insertRow(int row, const QModelIndex &parent)
{
    auto model = getTabularModel();
    if (!model) return false;
    if (parent.isValid()) return false;

    Command *command = model->getInsertRowCommand(getUnsorted(row));

    if (command) {
        emit addCommand(command);
    }

    return (command ? true : false);
}

bool
ModelDataTableModel::removeRow(int row, const QModelIndex &parent)
{
    auto model = getTabularModel();
    if (!model) return false;
    if (parent.isValid()) return false;

    Command *command = model->getRemoveRowCommand(getUnsorted(row));

    if (command) {
        emit addCommand(command);
    }

    return (command ? true : false);
}

Qt::ItemFlags
ModelDataTableModel::flags(const QModelIndex &) const
{
    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsEditable |
        Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable;
    return flags;
}

QVariant
ModelDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    auto model = getTabularModel();
    if (!model) return QVariant();

    if (orientation == Qt::Vertical && role == Qt::DisplayRole) {
        return section + 1;
    }
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        return model->getHeading(section);
    } 
    return QVariant();
}

QModelIndex
ModelDataTableModel::index(int row, int column, const QModelIndex &) const
{
    return createIndex(row, column, (void *)nullptr);
}

QModelIndex
ModelDataTableModel::parent(const QModelIndex &) const
{
    return QModelIndex();
}

int
ModelDataTableModel::rowCount(const QModelIndex &parent) const
{
    auto model = getTabularModel();
    if (!model) return 0;
    if (parent.isValid()) return 0;
    int count = model->getRowCount();
    return count;
}

int
ModelDataTableModel::columnCount(const QModelIndex &parent) const
{
    auto model = getTabularModel();
    if (!model) return 0;
    if (parent.isValid()) return 0;
    return model->getColumnCount();
}

QModelIndex 
ModelDataTableModel::getModelIndexForFrame(sv_frame_t frame) const
{
    auto model = getTabularModel();
    if (!model) return createIndex(0, 0);
    int row = model->getRowForFrame(frame);
    return createIndex(getSorted(row), 0, (void *)nullptr);
}

sv_frame_t
ModelDataTableModel::getFrameForModelIndex(const QModelIndex &index) const
{
    auto model = getTabularModel();
    if (!model) return 0;
    return model->getFrameForRow(getUnsorted(index.row()));
}

QModelIndex
ModelDataTableModel::findText(QString text) const
{
    auto model = getTabularModel();
    if (!model) return QModelIndex();
    if (text == "") return QModelIndex();
    int rows = rowCount();
    int cols = columnCount();
    int current = getCurrentRow();
    for (int row = 1; row <= rows; ++row) {
        int wrapped = (row + current) % rows;
        for (int col = 0; col < cols; ++col) {
            if (model->getSortType(col) != TabularModel::SortAlphabetical) {
                continue;
            }
            QString cell = model->getData(getUnsorted(wrapped), col,
                                            Qt::DisplayRole).toString();
            if (cell.contains(text, Qt::CaseInsensitive)) {
                return createIndex(wrapped, col);
            }
        }
    }
    return QModelIndex();
}

void
ModelDataTableModel::sort(int column, Qt::SortOrder sortOrder)
{
//    SVDEBUG << "ModelDataTableModel::sort(" << column << ", " << sortOrder
//              << ")" << endl;
    int prevCurrent = getCurrentRow();
    if (m_sortColumn != column) {
        clearSort();
    }
    m_sortColumn = column;
    m_sortOrdering = sortOrder;
    int current = getCurrentRow();
    if (current != prevCurrent) {
//         cerr << "Current row changed from " << prevCurrent << " to " << current << " for underlying row " << m_currentRow << endl;
        emit currentChanged(createIndex(current, 0, (void *)nullptr));
    }
    emit layoutChanged();
}

void
ModelDataTableModel::modelChanged(ModelId)
{
    SVDEBUG << "ModelDataTableModel::modelChanged" << endl;
    QModelIndex ix0;
    QModelIndex ix1;
    if (rowCount() > 0) {
        ix0 = createIndex(0, 0);
        int lastCol = columnCount() - 1;
        if (lastCol < 0) lastCol = 0;
        ix1 = createIndex(rowCount(), lastCol);
    }
    SVDEBUG << "emitting dataChanged from row " << ix0.row() << " to " << ix1.row() << endl;
    emit dataChanged(ix0, ix1);
    clearSort();
    emit layoutChanged();
}

void 
ModelDataTableModel::modelChangedWithin(ModelId, sv_frame_t f0, sv_frame_t f1)
{
    SVDEBUG << "ModelDataTableModel::modelChangedWithin(" << f0 << "," << f1 << ")" << endl;
    QModelIndex ix0 = getModelIndexForFrame(f0);
    QModelIndex ix1 = getModelIndexForFrame(f1);
    int row0 = ix0.row();
    int row1 = ix1.row();
    if (row0 > 0) {
        ix0 = createIndex(row0 - 1, ix0.column(), (void *)nullptr);
    }
    if (row1 + 1 < rowCount()) {
        ix1 = createIndex(row1 + 1, ix1.column(), (void *)nullptr);
    }
    SVDEBUG << "emitting dataChanged from row " << ix0.row() << " to " << ix1.row() << endl;
    emit dataChanged(ix0, ix1);
    clearSort();
    emit layoutChanged();
}

int
ModelDataTableModel::getSorted(int row) const
{
    auto model = getTabularModel();
    if (!model) return row;

    if (model->isColumnTimeValue(m_sortColumn)) {
        if (m_sortOrdering == Qt::AscendingOrder) {
            return row;
        } else {
            return rowCount() - row - 1;
        }
    }

    if (m_sort.empty()) {
        resort();
    }
    int result = 0;
    if (row >= 0 && row < (int)m_sort.size()) {
        result = m_sort[row];
    }
    if (m_sortOrdering == Qt::DescendingOrder) {
        result = rowCount() - result - 1;
    }

    return result;
}

int
ModelDataTableModel::getUnsorted(int row) const
{
    auto model = getTabularModel();
    if (!model) return row;

    if (model->isColumnTimeValue(m_sortColumn)) {
        if (m_sortOrdering == Qt::AscendingOrder) {
            return row;
        } else {
            return rowCount() - row - 1;
        }
    }

    if (m_sort.empty()) {
        resort();
    }

    int result = 0;
    if (row >= 0 && row < (int)m_sort.size()) {
        if (m_sortOrdering == Qt::AscendingOrder) {
            result = m_rsort[row];
        } else {
            result = m_rsort[rowCount() - row - 1];
        }
    }

    return result;
}

void
ModelDataTableModel::resort() const
{
    auto model = getTabularModel();
    if (!model) return;

    bool numeric = (model->getSortType(m_sortColumn) ==
                    TabularModel::SortNumeric);

//    cerr << "resort: numeric == " << numeric << endl;

    m_sort.clear();
    m_rsort.clear();

    if (numeric) resortNumeric();
    else resortAlphabetical();

    std::map<int, int> tmp;

    // rsort maps from sorted row number to original row number

    for (int i = 0; i < (int)m_rsort.size(); ++i) {
        tmp[m_rsort[i]] = i;
    }

    // tmp now maps from original row number to sorted row number

    for (std::map<int, int>::const_iterator i = tmp.begin(); i != tmp.end(); ++i) {
        m_sort.push_back(i->second);
    }

    // and sort now maps from original row number to sorted row number
}

void
ModelDataTableModel::resortNumeric() const
{
    auto model = getTabularModel();
    if (!model) return;

    typedef std::multimap<double, int> MapType;

    MapType rowMap;
    int rows = model->getRowCount();

    for (int i = 0; i < rows; ++i) {
        QVariant value = model->getData(i, m_sortColumn, TabularModel::SortRole);
        rowMap.insert(MapType::value_type(value.toDouble(), i));
    }

    for (MapType::iterator i = rowMap.begin(); i != rowMap.end(); ++i) {
//        cerr << "resortNumeric: " << i->second << ": " << i->first << endl;
        m_rsort.push_back(i->second);
    }

    // rsort now maps from sorted row number to original row number
}

void
ModelDataTableModel::resortAlphabetical() const
{
    auto model = getTabularModel();
    if (!model) return;

    typedef std::multimap<QString, int> MapType;

    MapType rowMap;
    int rows = model->getRowCount();

    for (int i = 0; i < rows; ++i) {
        QVariant value =
            model->getData(i, m_sortColumn, TabularModel::SortRole);
        rowMap.insert(MapType::value_type(value.toString(), i));
    }

    for (MapType::iterator i = rowMap.begin(); i != rowMap.end(); ++i) {
//        cerr << "resortAlphabetical: " << i->second << ": " << i->first << endl;
        m_rsort.push_back(i->second);
    }

    // rsort now maps from sorted row number to original row number
}

int
ModelDataTableModel::getCurrentRow() const
{
    return getSorted(m_currentRow);
}

void
ModelDataTableModel::setCurrentRow(int row)
{
    m_currentRow = getUnsorted(row);
}

void
ModelDataTableModel::clearSort()
{
//    int prevCurrent = getCurrentRow();
    m_sort.clear();
//    int current = getCurrentRow(); //!!! no --  not until the sort criteria have changed
//    if (current != prevCurrent) {
//        cerr << "Current row changed from " << prevCurrent << " to " << current << " for underlying row " << m_currentRow << endl;
//        emit currentRowChanged(createIndex(current, 0, 0));
//    }
}

    
