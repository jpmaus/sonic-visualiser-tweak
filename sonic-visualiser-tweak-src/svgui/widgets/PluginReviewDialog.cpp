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

#include "PluginReviewDialog.h"

#include <QGridLayout>
#include <QTableWidget>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QHeaderView>
#include <QScreen>
#include <QApplication>

#include "plugin/FeatureExtractionPluginFactory.h"
#include "plugin/RealTimePluginFactory.h"

PluginReviewDialog::PluginReviewDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle(tr("Plugins Loaded"));

    QGridLayout *layout = new QGridLayout;
    setLayout(layout);

    m_table = new QTableWidget;
    layout->addWidget(m_table, 0, 1);
    
    QDialogButtonBox *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    layout->addWidget(bb, 1, 1);
    connect(bb, SIGNAL(rejected()), this, SLOT(close()));
}

PluginReviewDialog::~PluginReviewDialog()
{
}

void
PluginReviewDialog::populate()
{
    FeatureExtractionPluginFactory *feFactory =
        FeatureExtractionPluginFactory::instance();
    QString err;
    std::vector<QString> feIds = feFactory->getPluginIdentifiers(err);

    RealTimePluginFactory *dssiFactory =
        RealTimePluginFactory::instance("dssi");
    std::vector<QString> dssiIds = dssiFactory->getPluginIdentifiers();

    RealTimePluginFactory *ladspaFactory =
        RealTimePluginFactory::instance("ladspa");
    std::vector<QString> ladspaIds = ladspaFactory->getPluginIdentifiers();

    m_table->setRowCount(int(feIds.size() + dssiIds.size() + ladspaIds.size()));
    m_table->setColumnCount(5);

    QStringList headers;
    int typeCol = 0, libCol = 1, idCol = 2, dirCol = 3, nameCol = 4;
    headers << tr("Type") << tr("Library")
            << tr("Identifier") << tr("Found in") << tr("Name");
    m_table->setHorizontalHeaderLabels(headers);

    int row = 0;

    for (QString id: feIds) {
        auto staticData = feFactory->getPluginStaticData(id);
        m_table->setItem(row, typeCol, new QTableWidgetItem
                         (tr("Vamp")));
        m_table->setItem(row, idCol, new QTableWidgetItem
                         (QString::fromStdString(staticData.basic.identifier)));
        m_table->setItem(row, nameCol, new QTableWidgetItem
                         (QString::fromStdString(staticData.basic.name)));
        QString path = feFactory->getPluginLibraryPath(id);
        m_table->setItem(row, libCol, new QTableWidgetItem
                         (QFileInfo(path).fileName()));
        m_table->setItem(row, dirCol, new QTableWidgetItem
                         (QFileInfo(path).path()));
        row++;
    }

    for (QString id: dssiIds) {
        auto descriptor = dssiFactory->getPluginDescriptor(id);
        if (!descriptor) continue;
        m_table->setItem(row, typeCol, new QTableWidgetItem
                         (tr("DSSI")));
        m_table->setItem(row, idCol, new QTableWidgetItem
                         (QString::fromStdString(descriptor->label)));
        m_table->setItem(row, nameCol, new QTableWidgetItem
                         (QString::fromStdString(descriptor->name)));
        QString path = dssiFactory->getPluginLibraryPath(id);
        m_table->setItem(row, libCol, new QTableWidgetItem
                         (QFileInfo(path).fileName()));
        m_table->setItem(row, dirCol, new QTableWidgetItem
                         (QFileInfo(path).path()));
        row++;
    }

    for (QString id: ladspaIds) {
        auto descriptor = ladspaFactory->getPluginDescriptor(id);
        if (!descriptor) continue;
        m_table->setItem(row, typeCol, new QTableWidgetItem
                         (tr("LADSPA")));
        m_table->setItem(row, idCol, new QTableWidgetItem
                         (QString::fromStdString(descriptor->label)));
        m_table->setItem(row, nameCol, new QTableWidgetItem
                         (QString::fromStdString(descriptor->name)));
        QString path = ladspaFactory->getPluginLibraryPath(id);
        m_table->setItem(row, libCol, new QTableWidgetItem
                         (QFileInfo(path).fileName()));
        m_table->setItem(row, dirCol, new QTableWidgetItem
                         (QFileInfo(path).path()));
        row++;
    }

    m_table->setSortingEnabled(true);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->resizeColumnsToContents();

    int twidth = m_table->horizontalHeader()->length();
    int theight = m_table->verticalHeader()->length();
    
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect available = screen->availableGeometry();

    int width = std::min(twidth + 30, (available.width() * 3) / 4);
    int height = std::min(theight + 30, (available.height() * 3) / 4);

    resize(width, height);
}

