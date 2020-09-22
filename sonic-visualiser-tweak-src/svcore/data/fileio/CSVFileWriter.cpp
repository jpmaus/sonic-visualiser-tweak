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

#include "CSVFileWriter.h"
#include "CSVStreamWriter.h"

#include "model/Model.h"
#include "model/SparseOneDimensionalModel.h"
#include "model/SparseTimeValueModel.h"
#include "model/NoteModel.h"
#include "model/TextModel.h"

#include "base/TempWriteFile.h"
#include "base/Exceptions.h"
#include "base/Selection.h"

#include <QFile>
#include <QTextStream>
#include <exception>

CSVFileWriter::CSVFileWriter(QString path,
                             Model *model,
                             QString delimiter,
                             DataExportOptions options) :
    m_path(path),
    m_model(model),
    m_error(""),
    m_delimiter(delimiter),
    m_options(options)
{
}

CSVFileWriter::~CSVFileWriter()
{
}

bool
CSVFileWriter::isOK() const
{
    return m_error == "";
}

QString
CSVFileWriter::getError() const
{
    return m_error;
}

void
CSVFileWriter::write()
{
    Selection all {
        m_model->getStartFrame(),
        m_model->getEndFrame()
    };
    MultiSelection selections;
    selections.addSelection(all);
    writeSelection(selections); 
}

void
CSVFileWriter::writeSelection(MultiSelection selection)
{
    try {
        TempWriteFile temp(m_path);

        QFile file(temp.getTemporaryFilename());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            m_error = tr("Failed to open file %1 for writing")
                .arg(temp.getTemporaryFilename());
            return;
        }
    
        QTextStream out(&file);

        sv_frame_t blockSize = 65536;

        if (m_model->isSparse()) {
            // Write the whole in one go, as re-seeking for each block
            // may be very costly otherwise
            sv_frame_t startFrame, endFrame;
            selection.getExtents(startFrame, endFrame);
            blockSize = endFrame - startFrame;
        }
        
        bool completed = CSVStreamWriter::writeInChunks(
            out,
            *m_model,
            selection,
            m_reporter,
            m_delimiter,
            m_options,
            blockSize
        );

        file.close();
        if (completed) {
            temp.moveToTarget();
        }

    } catch (FileOperationFailed &f) {
        m_error = f.what();
    } catch (const std::exception &e) { // ProgressReporter could throw
        m_error = e.what();
    }
}
