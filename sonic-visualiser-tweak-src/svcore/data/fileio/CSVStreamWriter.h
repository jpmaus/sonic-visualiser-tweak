/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2017 Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_CSV_STREAM_WRITER_H
#define SV_CSV_STREAM_WRITER_H

#include "base/BaseTypes.h"
#include "base/Selection.h"
#include "base/ProgressReporter.h"
#include "base/DataExportOptions.h"
#include "data/model/Model.h"
#include <QString>
#include <algorithm>
#include <numeric>

namespace CSVStreamWriter
{

template <class OutStream>
bool
writeInChunks(OutStream& oss,
              const Model& model,
              const MultiSelection& regions,
              ProgressReporter* reporter = nullptr,
              QString delimiter = ",",
              DataExportOptions options = DataExportDefaults,
              const sv_frame_t blockSize = 16384)
{
    const auto selections = regions.getSelections();
    if (blockSize <= 0 || selections.empty()) return false;

    // TODO, some form of checking validity of selections?
    const auto nFramesToWrite = std::accumulate(
        selections.begin(),
        selections.end(),
        0,
        [](sv_frame_t acc, const Selection& current) -> sv_frame_t {
            return acc + (current.getEndFrame() - current.getStartFrame());
        }
    );

    const auto wasCancelled = [&reporter]() { 
        return reporter && reporter->wasCancelled(); 
    };

    sv_frame_t nFramesWritten = 0;
    int previousProgress = 0;
    bool started = false;

    for (const auto& extents : selections) {
        const auto startFrame = extents.getStartFrame();
        const auto endFrame = extents.getEndFrame();
        auto readPtr = startFrame;
        while (readPtr < endFrame) {
            if (wasCancelled()) return false;

            const auto start = readPtr;
            const auto end = std::min(start + blockSize, endFrame);
            const auto data = model.toDelimitedDataString(
                delimiter,
                options,
                start,
                end - start
            ).trimmed();

            if ( data != "" ) {
                if (started) {
                    oss << "\n";
                } else {
                    started = true;
                }
                oss << data;
            }

            nFramesWritten += end - start;
            const int currentProgress =
                int(100 * nFramesWritten / nFramesToWrite);
            const bool hasIncreased = currentProgress > previousProgress;
            if (hasIncreased) {
                if (reporter) reporter->setProgress(currentProgress);
                previousProgress = currentProgress;
            }
            readPtr = end;
        }
    }
    return !wasCancelled(); // setProgress could process event loop
}

template <class OutStream>
bool 
writeInChunks(OutStream& oss,
              const Model& model,
              const Selection& extents,
              ProgressReporter* reporter = nullptr,
              QString delimiter = ",",
              DataExportOptions options = DataExportDefaults,
              const sv_frame_t blockSize = 16384)
{
    const auto startFrame = extents.isEmpty() ?
        model.getStartFrame() : extents.getStartFrame();
    const auto endFrame = extents.isEmpty() ?
        model.getEndFrame() : extents.getEndFrame();
    const auto hasValidExtents = startFrame >= 0 && endFrame > startFrame;
    if (!hasValidExtents) return false;
    Selection all {
        startFrame,
        endFrame
    };
    MultiSelection regions;
    regions.addSelection(all);
    return CSVStreamWriter::writeInChunks(
        oss,
        model,
        regions,
        reporter,
        delimiter,
        options,
        blockSize
    );
}

template <class OutStream>
bool 
writeInChunks(OutStream& oss,
              const Model& model,
              ProgressReporter* reporter = nullptr,
              QString delimiter = ",",
              DataExportOptions options = DataExportDefaults,
              const sv_frame_t blockSize = 16384)
{
    const Selection empty;
    return CSVStreamWriter::writeInChunks(
        oss,
        model,
        empty,
        reporter,
        delimiter,
        options,
        blockSize
    );
}
} // namespace CSVStreamWriter
#endif
