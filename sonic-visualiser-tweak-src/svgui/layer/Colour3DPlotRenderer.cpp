/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2016 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Colour3DPlotRenderer.h"
#include "RenderTimer.h"

#include "base/Profiler.h"
#include "base/HitCount.h"

#include "data/model/DenseThreeDimensionalModel.h"
#include "data/model/Dense3DModelPeakCache.h"
#include "data/model/FFTModel.h"

#include "LayerGeometryProvider.h"
#include "VerticalBinLayer.h"
#include "PaintAssistant.h"
#include "ImageRegionFinder.h"

#include "view/ViewManager.h" // for main model sample rate. Pity

#include <vector>

#include <utility>
using namespace std::rel_ops;

//#define DEBUG_COLOUR_PLOT_REPAINT 1
//#define DEBUG_COLOUR_PLOT_CACHE_SELECTION 1

using namespace std;

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(const LayerGeometryProvider *v, QPainter &paint, QRect rect)
{
    return render(v, paint, rect, false);
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::renderTimeConstrained(const LayerGeometryProvider *v,
                                            QPainter &paint, QRect rect)
{
    return render(v, paint, rect, true);
}

QRect
Colour3DPlotRenderer::getLargestUncachedRect(const LayerGeometryProvider *v)
{
    RenderType renderType = decideRenderType(v);

    if (renderType == DirectTranslucent) {
        return QRect(); // never cached
    }

    int h = m_cache.getSize().height();

    QRect areaLeft(0, 0, m_cache.getValidLeft(), h);
    QRect areaRight(m_cache.getValidRight(), 0,
                    m_cache.getSize().width() - m_cache.getValidRight(), h);

    if (areaRight.width() > areaLeft.width()) {
        return areaRight;
    } else {
        return areaLeft;
    }
}

bool
Colour3DPlotRenderer::geometryChanged(const LayerGeometryProvider *v)
{
    RenderType renderType = decideRenderType(v);

    if (renderType == DirectTranslucent) {
        return true; // never cached
    }

    if (m_cache.getSize() == v->getPaintSize() &&
        m_cache.getZoomLevel() == v->getZoomLevel() &&
        m_cache.getStartFrame() == v->getStartFrame()) {
        return false;
    } else {
        return true;
    }
}

Colour3DPlotRenderer::RenderResult
Colour3DPlotRenderer::render(const LayerGeometryProvider *v,
                             QPainter &paint, QRect rect, bool timeConstrained)
{
    RenderType renderType = decideRenderType(v);

    if (timeConstrained) {
        if (renderType != DrawBufferPixelResolution) {
            // Rendering should be fast in bin-resolution and direct
            // draw cases because we are quite well zoomed-in, and the
            // sums are easier this way. Calculating boundaries later
            // will be fiddly for partial paints otherwise.
            timeConstrained = false;

        } else if (m_secondsPerXPixelValid) {
            double predicted = m_secondsPerXPixel * rect.width();
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": Predicted time for width " << rect.width() << " = "
                    << predicted << " (" << m_secondsPerXPixel << " x "
                    << rect.width() << ")" << endl;
#endif
            if (predicted < 0.175) {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
                SVDEBUG << "render " << m_sources.source
                        << ": Predicted time looks fast enough: no partial renders"
                        << endl;
#endif
                timeConstrained = false;
            }
        }
    }
            
    int x0 = v->getXForViewX(rect.x());
    int x1 = v->getXForViewX(rect.x() + rect.width());
    if (x0 < 0) x0 = 0;
    if (x1 > v->getPaintWidth()) x1 = v->getPaintWidth();

    sv_frame_t startFrame = v->getStartFrame();

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": cache size is " << m_cache.getSize().width()
            << "x" << m_cache.getSize().height()
            << " at zoom level " << m_cache.getZoomLevel() << endl;
#endif

    bool justCreated = m_cache.getSize().isEmpty();
    
    bool justInvalidated =
        (m_cache.getSize() != v->getPaintSize() ||
         m_cache.getZoomLevel() != v->getZoomLevel());

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": justCreated = " << justCreated
            << ", justInvalidated = " << justInvalidated
            << endl;
#endif
    
    m_cache.resize(v->getPaintSize());
    m_cache.setZoomLevel(v->getZoomLevel());

    m_magCache.resize(v->getPaintSize().width());
    m_magCache.setZoomLevel(v->getZoomLevel());
    
    if (renderType == DirectTranslucent) {
        MagnitudeRange range = renderDirectTranslucent(v, paint, rect);
        return { rect, range };
    }
    
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": cache start " << m_cache.getStartFrame()
            << " valid left " << m_cache.getValidLeft()
            << " valid right " << m_cache.getValidRight()
            << endl;
    SVDEBUG << "render " << m_sources.source
            << ": view start " << startFrame
            << " x0 " << x0
            << " x1 " << x1
            << endl;
#endif

    static HitCount count("Colour3DPlotRenderer: image cache");

    if (m_cache.isValid()) { // some part of the cache is valid

        if (v->getXForFrame(m_cache.getStartFrame()) ==
            v->getXForFrame(startFrame) &&
            m_cache.getValidLeft() <= x0 &&
            m_cache.getValidRight() >= x1) {

#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": cache hit" << endl;
#endif
            count.hit();
            
            // cache is valid for the complete requested area
            paint.drawImage(rect, m_cache.getImage(), rect);

            MagnitudeRange range = m_magCache.getRange(x0, x1 - x0);

            return { rect, range };

        } else {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": cache partial hit" << endl;
#endif
            count.partial();
            
            // cache doesn't begin at the right frame or doesn't
            // contain the complete view, but might be scrollable or
            // partially usable
            m_cache.scrollTo(v, startFrame);
            m_magCache.scrollTo(v, startFrame);

            // if we are not time-constrained, then we want to paint
            // the whole area in one go; we don't return a partial
            // paint. To avoid providing the more complex logic to
            // handle painting discontiguous areas, if the only valid
            // part of cache is in the middle, just make the whole
            // thing invalid and start again.
            if (!timeConstrained) {
                if (m_cache.getValidLeft() > x0 &&
                    m_cache.getValidRight() < x1) {
                    m_cache.invalidate();
                }
            }
        }
    } else {
        // cache is completely invalid
        count.miss();
        m_cache.setStartFrame(startFrame);
        m_magCache.setStartFrame(startFrame);
    }

    bool rightToLeft = false;

    int reqx0 = x0;
    int reqx1 = x1;
    
    if (!m_cache.isValid() && timeConstrained) {
        if (x0 == 0 && x1 == v->getPaintWidth()) {
            
            // When rendering the whole area, in a context where we
            // might not be able to complete the work, start from
            // somewhere near the middle so that the region of
            // interest appears first.
            //
            // This is very useful if we actually are slow to render,
            // but if we're not sure how fast we'll be, we should
            // prefer not to because it can be distracting to render
            // fast from the middle and then jump back to fill in the
            // start. That is:
            //
            // - if our seconds-per-x-pixel count is invalid, then we
            // don't do this: we've probably only just been created
            // and don't know how fast we'll be yet (this happens
            // often while zooming rapidly in and out). The exception
            // to the exception is if we're displaying peak
            // frequencies; this we can assume to be slow. (Note that
            // if the seconds-per-x-pixel is valid and we know we're
            // fast, then we've already set timeConstrained false
            // above so this doesn't apply)
            // 
            // - if we're using a peak cache, we don't do this;
            // drawing from peak cache is often (even if not always)
            // fast.

            bool drawFromTheMiddle = true;

            if (!m_secondsPerXPixelValid &&
                (m_params.binDisplay != BinDisplay::PeakFrequencies)) {
                drawFromTheMiddle = false;
            } else {
                int peakCacheIndex = -1, binsPerPeak = -1;
                getPreferredPeakCache(v, peakCacheIndex, binsPerPeak);
                if (peakCacheIndex >= 0) { // have a peak cache
                    drawFromTheMiddle = false;
                }
            }

            if (drawFromTheMiddle) {
                double offset = 0.5 * (double(rand()) / double(RAND_MAX));
                x0 = int(x1 * offset);
            }
        }
    }

    if (m_cache.isValid()) {
            
        // When rendering only a part of the cache, we need to make
        // sure that the part we're rendering is adjacent to (or
        // overlapping) a valid area of cache, if we have one. The
        // alternative is to ditch the valid area of cache and render
        // only the requested area, but that's risky because this can
        // happen when just waving the pointer over a small part of
        // the view -- if we lose the partly-built cache every time
        // the user does that, we'll never finish building it.
        int left = x0;
        int width = x1 - x0;
        bool isLeftOfValidArea = false;
        m_cache.adjustToTouchValidArea(left, width, isLeftOfValidArea);
        x0 = left;
        x1 = x0 + width;

        // That call also told us whether we should be painting
        // sub-regions of our target region in right-to-left order in
        // order to ensure contiguity
        rightToLeft = isLeftOfValidArea;
    }
    
    // Note, we always paint the full height to cache. We want to
    // ensure the cache is coherent without having to worry about
    // vertical matching of required and valid areas as well as
    // horizontal.

    if (renderType == DrawBufferBinResolution) {

        renderToCacheBinResolution(v, x0, x1 - x0);

    } else { // must be DrawBufferPixelResolution, handled DirectTranslucent earlier

        if (timeConstrained && !justCreated && justInvalidated) {
            SVDEBUG << "render " << m_sources.source
                    << ": invalidated cache in time-constrained context, that's all we're doing for now - wait for next update to start filling" << endl;
        } else {
            renderToCachePixelResolution(v, x0, x1 - x0, rightToLeft, timeConstrained);
        }
    }

    QRect pr = rect & m_cache.getValidArea();
    paint.drawImage(pr.x(), pr.y(), m_cache.getImage(),
                    pr.x(), pr.y(), pr.width(), pr.height());

    if (!timeConstrained && (pr != rect)) {
        QRect cva = m_cache.getValidArea();
        SVCERR << "WARNING: failed to render entire requested rect "
               << "even when not time-constrained: wanted "
               << rect.x() << "," << rect.y() << " "
               << rect.width() << "x" << rect.height() << ", got "
               << pr.x() << "," << pr.y() << " "
               << pr.width() << "x" << pr.height()
               << ", after request of width " << (x1 - x0)
               << endl
               << "(cache valid area is "
               << cva.x() << "," << cva.y() << " "
               << cva.width() << "x" << cva.height() << ")"
               << endl;
    }

    MagnitudeRange range = m_magCache.getRange(reqx0, reqx1 - reqx0);

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": returning rect rendered as " << pr.x() << "," << pr.y()
            << " " << pr.width() << "x" << pr.height() << endl;
    SVDEBUG << "render " << m_sources.source
            << ": mag range from cache in x-range " << reqx0
            << " to " << reqx1 << " is " << range.getMin() << " -> "
            << range.getMax() << endl;
#endif
    
    return { pr, range };
}

Colour3DPlotRenderer::RenderType
Colour3DPlotRenderer::decideRenderType(const LayerGeometryProvider *v) const
{
    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    if (!model || !v || !(v->getViewManager())) {
        return DrawBufferPixelResolution; // or anything
    }

    int binResolution = model->getResolution();
    ZoomLevel zoomLevel = v->getZoomLevel();
    sv_samplerate_t modelRate = model->getSampleRate();

    double rateRatio = v->getViewManager()->getMainModelSampleRate() / modelRate;
    double relativeBinResolution = binResolution * rateRatio;

    if (m_params.binDisplay == BinDisplay::PeakFrequencies) {
        // no alternative works here
        return DrawBufferPixelResolution;
    }

    if (!m_params.alwaysOpaque && !m_params.interpolate) {

        // consider translucent option -- only if not smoothing & not
        // explicitly requested opaque & sufficiently zoomed-in
        
        if (model->getHeight() * 3 < v->getPaintHeight() &&
            zoomLevel < ZoomLevel(ZoomLevel::FramesPerPixel,
                                  int(round(relativeBinResolution / 3)))) {
            return DirectTranslucent;
        }
    }

    if (ZoomLevel(ZoomLevel::FramesPerPixel,
                  int(round(relativeBinResolution))) > zoomLevel) {
        return DrawBufferBinResolution;
    } else {
        return DrawBufferPixelResolution;
    }
}

ColumnOp::Column
Colour3DPlotRenderer::getColumn(int sx, int minbin, int nbins,
                                shared_ptr<DenseThreeDimensionalModel> source) const
{
    // order:
    // get column -> scale -> normalise -> record extents ->
    // peak pick -> distribute/interpolate -> apply display gain

    // we do the first bit here:
    // get column -> scale -> normalise

    ColumnOp::Column column;
    
    if (m_params.showDerivative && sx > 0) {

        auto prev = getColumnRaw(sx - 1, minbin, nbins, source);
        column = getColumnRaw(sx, minbin, nbins, source);
        
        for (int i = 0; i < nbins; ++i) {
            column[i] -= prev[i];
        }

    } else {
        column = getColumnRaw(sx, minbin, nbins, source);
    }

    if (m_params.colourScale.getScale() == ColourScaleType::Phase &&
        !m_sources.fft.isNone()) {
        return column;
    } else {
        column = ColumnOp::applyGain(column, m_params.scaleFactor);
        column = ColumnOp::normalize(column, m_params.normalization);
        return column;
    }
}

ColumnOp::Column
Colour3DPlotRenderer::getColumnRaw(int sx, int minbin, int nbins,
                                   shared_ptr<DenseThreeDimensionalModel> source) const
{
    Profiler profiler("Colour3DPlotRenderer::getColumn");

    ColumnOp::Column column;
    ColumnOp::Column fullColumn;

    if (m_params.colourScale.getScale() == ColourScaleType::Phase) {
        auto fftModel = ModelById::getAs<FFTModel>(m_sources.fft);
        if (fftModel) {
            fullColumn = fftModel->getPhases(sx);
        }
    }

    if (fullColumn.empty()) {
        fullColumn = source->getColumn(sx);
    }
    
    column = vector<float>(fullColumn.data() + minbin,
                           fullColumn.data() + minbin + nbins);
    return column;
}

MagnitudeRange
Colour3DPlotRenderer::renderDirectTranslucent(const LayerGeometryProvider *v,
                                              QPainter &paint,
                                              QRect rect)
{
    Profiler profiler("Colour3DPlotRenderer::renderDirectTranslucent");

    MagnitudeRange magRange;
    
    QPoint illuminatePos;
    bool illuminate = v->shouldIlluminateLocalFeatures
        (m_sources.verticalBinLayer, illuminatePos);

    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    if (!model) return magRange;
    
    int x0 = rect.left();
    int x1 = x0 + rect.width();

    int h = v->getPaintHeight();

    sv_frame_t modelStart = model->getStartFrame();
    sv_frame_t modelEnd = model->getEndFrame();
    int modelResolution = model->getResolution();

    double rateRatio =
        v->getViewManager()->getMainModelSampleRate() / model->getSampleRate();

    // the s-prefix values are source, i.e. model, column and bin numbers
    int sx0 = int((double(v->getFrameForX(x0)) / rateRatio - double(modelStart))
                  / modelResolution);
    int sx1 = int((double(v->getFrameForX(x1)) / rateRatio - double(modelStart))
                  / modelResolution);

    int sh = model->getHeight();

    const int buflen = 40;
    char labelbuf[buflen];

    int minbin = m_sources.verticalBinLayer->getIBinForY(v, h);
    if (minbin >= sh) minbin = sh - 1;
    if (minbin < 0) minbin = 0;
    
    int nbins  = m_sources.verticalBinLayer->getIBinForY(v, 0) - minbin + 1;
    if (minbin + nbins > sh) nbins = sh - minbin;

    int psx = -1;

    vector<float> preparedColumn;

    int modelWidth = model->getWidth();

    for (int sx = sx0; sx <= sx1; ++sx) {

        if (sx < 0 || sx >= modelWidth) {
            continue;
        }

        if (sx != psx) {

            // order:
            // get column -> scale -> normalise -> record extents ->
            // peak pick -> distribute/interpolate -> apply display gain

            // this does the first three:
            preparedColumn = getColumn(sx, minbin, nbins, model);
            
            magRange.sample(preparedColumn);

            if (m_params.binDisplay == BinDisplay::PeakBins) {
                preparedColumn = ColumnOp::peakPick(preparedColumn);
            }

            // Display gain belongs to the colour scale and is
            // applied by the colour scale object when mapping it

            psx = sx;
        }

        sv_frame_t fx = sx * modelResolution + modelStart;

        if (fx + modelResolution <= modelStart || fx > modelEnd) continue;

        int rx0 = v->getXForFrame(int(double(fx) * rateRatio));
        int rx1 = v->getXForFrame(int(double(fx + modelResolution + 1) * rateRatio));

        int rw = rx1 - rx0;
        if (rw < 1) rw = 1;

        // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
        // replacement (horizontalAdvance) was only added in Qt 5.11
        // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        
        bool showLabel = (rw > 10 &&
                          paint.fontMetrics().width("0.000000") < rw - 3 &&
                          paint.fontMetrics().height() < (h / sh));
        
        for (int sy = minbin; sy < minbin + nbins; ++sy) {

            int ry0 = m_sources.verticalBinLayer->getIYForBin(v, sy);
            int ry1 = m_sources.verticalBinLayer->getIYForBin(v, sy + 1);

            if (m_params.invertVertical) {
                ry0 = h - ry0 - 1;
                ry1 = h - ry1 - 1;
            }
                    
            QRect r(rx0, ry1, rw, ry0 - ry1);

            float value = preparedColumn[sy - minbin];
            QColor colour = m_params.colourScale.getColour(value,
                                                           m_params.colourRotation);

            if (rw == 1) {
                paint.setPen(colour);
                paint.setBrush(Qt::NoBrush);
                paint.drawLine(r.x(), r.y(), r.x(), r.y() + r.height() - 1);
                continue;
            }

            QColor pen(255, 255, 255, 80);
            QColor brush(colour);

            if (rw > 3 && r.height() > 3) {
                brush.setAlpha(160);
            }

            paint.setPen(Qt::NoPen);
            paint.setBrush(brush);

            if (illuminate) {
                if (r.contains(illuminatePos)) {
                    paint.setPen(v->getForeground());
                }
            }
            
#ifdef DEBUG_COLOUR_PLOT_REPAINT
//            SVDEBUG << "rect " << r.x() << "," << r.y() << " "
//                      << r.width() << "x" << r.height() << endl;
#endif

            paint.drawRect(r);

            if (showLabel) {
                double value = model->getValueAt(sx, sy);
                snprintf(labelbuf, buflen, "%06f", value);
                QString text(labelbuf);
                PaintAssistant::drawVisibleText
                    (v,
                     paint,
                     rx0 + 2,
                     ry0 - h / sh - 1 + 2 + paint.fontMetrics().ascent(),
                     text,
                     PaintAssistant::OutlinedText);
            }
        }
    }

    return magRange;
}

void
Colour3DPlotRenderer::getPreferredPeakCache(const LayerGeometryProvider *v,
                                            int &peakCacheIndex,
                                            int &binsPerPeak) const
{
    peakCacheIndex = -1;
    binsPerPeak = -1;

    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    if (!model) return;
    if (m_params.binDisplay == BinDisplay::PeakFrequencies) return;
    if (m_params.colourScale.getScale() == ColourScaleType::Phase) return;
    
    ZoomLevel zoomLevel = v->getZoomLevel();
    int binResolution = model->getResolution();
    
    for (int ix = 0; in_range_for(m_sources.peakCaches, ix); ++ix) {
        auto peakCache = ModelById::getAs<Dense3DModelPeakCache>
            (m_sources.peakCaches[ix]);
        if (!peakCache) continue;
        int bpp = peakCache->getColumnsPerPeak();
        ZoomLevel equivZoom(ZoomLevel::FramesPerPixel, binResolution * bpp);
#ifdef DEBUG_COLOUR_PLOT_CACHE_SELECTION
        SVDEBUG << "render " << m_sources.source
                << ": getPreferredPeakCache: zoomLevel = " << zoomLevel
                << ", cache " << ix << " has bpp = " << bpp
                << " for equivZoom = " << equivZoom << endl;
#endif
        if (zoomLevel >= equivZoom) {
            // this peak cache would work, though it might not be best
            if (bpp > binsPerPeak) {
                // ok, it's better than the best one we've found so far
                peakCacheIndex = ix;
                binsPerPeak = bpp;
            }
        }
    }

#ifdef DEBUG_COLOUR_PLOT_CACHE_SELECTION
    SVDEBUG << "render " << m_sources.source
            << ": getPreferredPeakCache: zoomLevel = " << zoomLevel
            << ", binResolution " << binResolution 
            << ", peakCaches " << m_sources.peakCaches.size()
            << ": preferring peakCacheIndex " << peakCacheIndex
            << " for binsPerPeak " << binsPerPeak
            << endl;
#endif
}

void
Colour3DPlotRenderer::renderToCachePixelResolution(const LayerGeometryProvider *v,
                                                   int x0, int repaintWidth,
                                                   bool rightToLeft,
                                                   bool timeConstrained)
{
    Profiler profiler("Colour3DPlotRenderer::renderToCachePixelResolution");
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": [PIXEL] renderToCachePixelResolution" << endl;
#endif
    
    // Draw to the draw buffer, and then copy from there. The draw
    // buffer is at the same resolution as the target in the cache, so
    // no extra scaling needed.

    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    if (!model) return;

    int h = v->getPaintHeight();

    clearDrawBuffer(repaintWidth, h);

    vector<int> binforx(repaintWidth);
    vector<double> binfory(h);
    
    int binResolution = model->getResolution();

    for (int x = 0; x < repaintWidth; ++x) {
        sv_frame_t f0 = v->getFrameForX(x0 + x);
        double s0 = double(f0 - model->getStartFrame()) / binResolution;
        binforx[x] = int(s0 + 0.0001);
    }

    int peakCacheIndex = -1;
    int binsPerPeak = -1;

    getPreferredPeakCache(v, peakCacheIndex, binsPerPeak);
    
    for (int y = 0; y < h; ++y) {
        binfory[y] = m_sources.verticalBinLayer->getBinForY(v, h - y - 1);
    }

    int attainedWidth;

    if (m_params.binDisplay == BinDisplay::PeakFrequencies) {
        attainedWidth = renderDrawBufferPeakFrequencies(v,
                                                        repaintWidth,
                                                        h,
                                                        binforx,
                                                        binfory,
                                                        rightToLeft,
                                                        timeConstrained);

    } else {
        attainedWidth = renderDrawBuffer(repaintWidth,
                                         h,
                                         binforx,
                                         binfory,
                                         peakCacheIndex,
                                         rightToLeft,
                                         timeConstrained);
    }

    if (attainedWidth == 0) return;

    // draw buffer is pixel resolution, no scaling factors or padding involved
    
    int paintedLeft = x0;
    if (rightToLeft) {
        paintedLeft += (repaintWidth - attainedWidth);
    }

    m_cache.drawImage(paintedLeft, attainedWidth,
                      m_drawBuffer,
                      paintedLeft - x0, attainedWidth);

    for (int i = 0; in_range_for(m_magRanges, i); ++i) {
        m_magCache.sampleColumn(i, m_magRanges.at(i));
    }
}

QImage
Colour3DPlotRenderer::scaleDrawBufferImage(QImage image,
                                           int targetWidth,
                                           int targetHeight) const
{
    int sourceWidth = image.width();
    int sourceHeight = image.height();

    // We can only do this if we're making the image larger --
    // otherwise peaks may be lost. So this should be called only when
    // rendering in DrawBufferBinResolution mode. Whenever the bin
    // size is smaller than the pixel size, in either x or y axis, we
    // should be using DrawBufferPixelResolution mode instead
    
    if (targetWidth < sourceWidth || targetHeight < sourceHeight) {
        throw std::logic_error("Colour3DPlotRenderer::scaleDrawBufferImage: Can only use this function when making the image larger; should be rendering DrawBufferPixelResolution instead");
    }

    if (sourceWidth <= 0 || sourceHeight <= 0) {
        throw std::logic_error("Colour3DPlotRenderer::scaleDrawBufferImage: Source image is empty");
    }

    if (targetWidth <= 0 || targetHeight <= 0) {
        throw std::logic_error("Colour3DPlotRenderer::scaleDrawBufferImage: Target image is empty");
    }        

    // This function exists because of some unpredictable behaviour
    // from Qt when scaling images with FastTransformation mode. We
    // continue to use Qt's scaler for SmoothTransformation but let's
    // bring the non-interpolated version "in-house" so we know what
    // it's really doing.
    
    if (m_params.interpolate) {
        return image.scaled(targetWidth, targetHeight,
                            Qt::IgnoreAspectRatio,
                            Qt::SmoothTransformation);
    }
    
    // Same format as the target cache
    QImage target(targetWidth, targetHeight,
                  QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < targetHeight; ++y) {

        QRgb *targetLine = reinterpret_cast<QRgb *>(target.scanLine(y));
        
        int sy = int((uint64_t(y) * sourceHeight) / targetHeight);
        if (sy == sourceHeight) --sy;

        // The source image is 8-bit indexed
        const uchar *sourceLine = image.constScanLine(sy);

        int psx = -1;
        QRgb colour = {};
        
        for (int x = 0; x < targetWidth; ++x) {

            int sx = int((uint64_t(x) * sourceWidth) / targetWidth);
            if (sx == sourceWidth) --sx;

            if (sx > psx) {
                colour = image.color(sourceLine[sx]);
            }
            
            targetLine[x] = colour;
            psx = sx;
        }
    }

    return target;
}

void
Colour3DPlotRenderer::renderToCacheBinResolution(const LayerGeometryProvider *v,
                                                 int x0, int repaintWidth)
{
    Profiler profiler("Colour3DPlotRenderer::renderToCacheBinResolution");
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": [BIN] renderToCacheBinResolution" << endl;
#endif
    
    // Draw to the draw buffer, and then scale-copy from there. Draw
    // buffer is at bin resolution, i.e. buffer x == source column
    // number. We use toolkit smooth scaling for interpolation.

    auto model = ModelById::getAs<DenseThreeDimensionalModel>(m_sources.source);
    if (!model) return;

    // The draw buffer will contain a fragment at bin resolution. We
    // need to ensure that it starts and ends at points where a
    // time-bin boundary occurs at an exact pixel boundary, and with a
    // certain amount of overlap across existing pixels so that we can
    // scale and draw from it without smoothing errors at the edges.

    // If (getFrameForX(x) / increment) * increment ==
    // getFrameForX(x), then x is a time-bin boundary.  We want two
    // such boundaries at either side of the draw buffer -- one which
    // we draw up to, and one which we subsequently crop at.

    sv_frame_t leftBoundaryFrame = -1, leftCropFrame = -1;
    sv_frame_t rightBoundaryFrame = -1, rightCropFrame = -1;

    int drawBufferWidth;
    int binResolution = model->getResolution();

    // These loops should eventually terminate provided that
    // getFrameForX always returns a multiple of the zoom level,
    // i.e. there is some x for which getFrameForX(x) == 0 and
    // subsequent return values are equally spaced
    
    for (int x = x0; ; --x) {
        sv_frame_t f = v->getFrameForX(x);
        if ((f / binResolution) * binResolution == f) {
            if (leftCropFrame == -1) leftCropFrame = f;
            else if (x < x0 - 2) {
                leftBoundaryFrame = f;
                break;
            }
        }
    }
    
    for (int x = x0 + repaintWidth; ; ++x) {
        sv_frame_t f = v->getFrameForX(x);
        if ((f / binResolution) * binResolution == f) {
            if (v->getXForFrame(f) < x0 + repaintWidth) {
                continue;
            }
            if (rightCropFrame == -1) rightCropFrame = f;
            else if (x > x0 + repaintWidth + 2) {
                rightBoundaryFrame = f;
                break;
            }
        }
    }

    drawBufferWidth = int
        ((rightBoundaryFrame - leftBoundaryFrame) / binResolution);
    
    int h = v->getPaintHeight();

    // For our purposes here, the draw buffer needs to be exactly our
    // target size (so we recreate always rather than just clear it)
    
    recreateDrawBuffer(drawBufferWidth, h);

    vector<int> binforx(drawBufferWidth);
    vector<double> binfory(h);
    
    for (int x = 0; x < drawBufferWidth; ++x) {
        binforx[x] = int(leftBoundaryFrame / binResolution) + x;
    }

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": binResolution " << binResolution << endl;
#endif
    
    for (int y = 0; y < h; ++y) {
        binfory[y] = m_sources.verticalBinLayer->getBinForY(v, h - y - 1);
    }

    int attainedWidth = renderDrawBuffer(drawBufferWidth,
                                         h,
                                         binforx,
                                         binfory,
                                         -1,
                                         false,
                                         false);

    if (attainedWidth == 0) return;

    int scaledLeft = v->getXForFrame(leftBoundaryFrame);
    int scaledRight = v->getXForFrame(rightBoundaryFrame);

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": scaling draw buffer from width " << m_drawBuffer.width()
            << " to " << (scaledRight - scaledLeft)
            << " (nb drawBufferWidth = "
            << drawBufferWidth << ", attainedWidth = "
            << attainedWidth << ")" << endl;
#endif

    QImage scaled = scaleDrawBufferImage
        (m_drawBuffer, scaledRight - scaledLeft, h);
            
    int scaledLeftCrop = v->getXForFrame(leftCropFrame);
    int scaledRightCrop = v->getXForFrame(rightCropFrame);
    
    int targetLeft = scaledLeftCrop;
    if (targetLeft < 0) {
        targetLeft = 0;
    }
    
    int targetWidth = scaledRightCrop - targetLeft;
    if (targetLeft + targetWidth > m_cache.getSize().width()) {
        targetWidth = m_cache.getSize().width() - targetLeft;
    }
    
    int sourceLeft = targetLeft - scaledLeft;
    if (sourceLeft < 0) {
        sourceLeft = 0;
    }

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": leftBoundaryFrame = " << leftBoundaryFrame
            << ", leftCropFrame = " << leftCropFrame
            << ", scaledLeft = " << scaledLeft
            << ", scaledLeftCrop = " << scaledLeftCrop
            << endl;
    SVDEBUG << "render " << m_sources.source
            << ": rightBoundaryFrame = " << rightBoundaryFrame
            << ", rightCropFrame = " << rightCropFrame
            << ", scaledRight = " << scaledRight
            << ", scaledRightCrop = " << scaledRightCrop
            << endl;
#endif
    
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": x0 = " << x0
            << ", repaintWidth = " << repaintWidth
            << ", targetLeft = " << targetLeft 
            << ", targetWidth = " << targetWidth << endl;
#endif
    
    if (targetWidth > 0) {
        // we are copying from an image that has already been scaled,
        // hence using the same width in both geometries
        m_cache.drawImage(targetLeft, targetWidth,
                          scaled,
                          sourceLeft, targetWidth);
    }
    
    for (int i = 0; i < targetWidth; ++i) {
        // but the mag range vector has not been scaled
        int sourceIx = int((double(i + sourceLeft) / scaled.width())
                           * int(m_magRanges.size()));
        if (in_range_for(m_magRanges, sourceIx)) {
            m_magCache.sampleColumn(i, m_magRanges.at(sourceIx));
        }
    }
}

int
Colour3DPlotRenderer::renderDrawBuffer(int w, int h,
                                       const vector<int> &binforx,
                                       const vector<double> &binfory,
                                       int peakCacheIndex,
                                       bool rightToLeft,
                                       bool timeConstrained)
{
    // Callers must have checked that the appropriate subset of
    // Sources data members are set for the supplied flags (e.g. that
    // peakCache corresponding to peakCacheIndex exists)
    
    RenderTimer timer(timeConstrained ?
                      RenderTimer::FastRender :
                      RenderTimer::NoTimeout);

    Profiler profiler("Colour3DPlotRenderer::renderDrawBuffer");
    
    int divisor = 1;

    std::shared_ptr<DenseThreeDimensionalModel> sourceModel;

    if (peakCacheIndex >= 0) {
        auto peakCache = ModelById::getAs<Dense3DModelPeakCache>
            (m_sources.peakCaches[peakCacheIndex]);
        if (peakCache) {
            divisor = peakCache->getColumnsPerPeak();
            sourceModel = peakCache;
        }
    }

    if (!sourceModel) {
        sourceModel = ModelById::getAs<DenseThreeDimensionalModel>
            (m_sources.source);
    }
    
    if (!sourceModel) return 0;

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": renderDrawBuffer: w = " << w << ", h = " << h
            << ", peakCacheIndex = " << peakCacheIndex << " (divisor = "
            << divisor << "), rightToLeft = " << rightToLeft
            << ", timeConstrained = " << timeConstrained << endl;
    SVDEBUG << "render " << m_sources.source
            << ": renderDrawBuffer: normalization = " << int(m_params.normalization)
            << ", binDisplay = " << int(m_params.binDisplay)
            << ", binScale = " << int(m_params.binScale)
            << ", alwaysOpaque = " << m_params.alwaysOpaque
            << ", interpolate = " << m_params.interpolate << endl;
#endif
    
    int sh = sourceModel->getHeight();
    
    int minbin = int(binfory[0] + 0.0001);
    if (minbin >= sh) minbin = sh - 1;
    if (minbin < 0) minbin = 0;

    int nbins  = int(binfory[h-1] + 0.0001) - minbin + 1;
    if (minbin + nbins > sh) nbins = sh - minbin;

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": minbin = " << minbin << ", nbins = " << nbins
            << ", last binfory = " << binfory[h-1]
            << " (rounds to " << int(binfory[h-1])
            << ") (model height " << sh << ")" << endl;
#endif
    
    int psx = -1;

    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }

    int xPixelCount = 0;
    
    vector<float> preparedColumn;

    int modelWidth = sourceModel->getWidth();

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": modelWidth " << modelWidth << ", divisor " << divisor << endl;
    SVDEBUG << "render " << m_sources.source
            << ": start = " << start << ", finish = " << finish << ", step = " << step << endl;
#endif
    
    for (int x = start; x != finish; x += step) {

        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++xPixelCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x] / divisor;
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1] / divisor;
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

#ifdef DEBUG_COLOUR_PLOT_REPAINT
//        SVDEBUG << "x = " << x << ", binforx[x] = " << binforx[x] << ", sx range " << sx0 << " -> " << sx1 << endl;
#endif

        vector<float> pixelPeakColumn;
        MagnitudeRange magRange;
        
        for (int sx = sx0; sx < sx1; ++sx) {

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {
                
                // order:
                // get column -> scale -> normalise -> record extents ->
                // peak pick -> distribute/interpolate -> apply display gain

                // this does the first three:
                ColumnOp::Column column = getColumn(sx, minbin, nbins,
                                                    sourceModel);

                magRange.sample(column);

                if (m_params.binDisplay == BinDisplay::PeakBins) {
                    column = ColumnOp::peakPick(column);
                }

                preparedColumn =
                    ColumnOp::distribute(column,
                                         h,
                                         binfory,
                                         minbin,
                                         m_params.interpolate);

                // Display gain belongs to the colour scale and is
                // applied by the colour scale object when mapping it
                
                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {

            for (int y = 0; y < h; ++y) {
                int py;
                if (m_params.invertVertical) {
                    py = y;
                } else {
                    py = h - y - 1;
                }
                m_drawBuffer.setPixel
                    (x,
                     py,
                     m_params.colourScale.getPixel(pixelPeakColumn[y]));
            }
            
            m_magRanges.push_back(magRange);
        }

        double fractionComplete = double(xPixelCount) / double(w);
        if (timer.outOfTime(fractionComplete)) {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": out of time with xPixelCount = " << xPixelCount << endl;
#endif
            updateTimings(timer, xPixelCount);
            return xPixelCount;
        }
    }

    updateTimings(timer, xPixelCount);

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": completed with xPixelCount = " << xPixelCount << endl;
#endif
    return xPixelCount;
}

int
Colour3DPlotRenderer::renderDrawBufferPeakFrequencies(const LayerGeometryProvider *v,
                                                      int w, int h,
                                                      const vector<int> &binforx,
                                                      const vector<double> &binfory,
                                                      bool rightToLeft,
                                                      bool timeConstrained)
{
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": [PEAK] renderDrawBufferPeakFrequencies" << endl;
#endif

    // Callers must have checked that the appropriate subset of
    // Sources data members are set for the supplied flags (e.g. that
    // fft model exists)
    
    RenderTimer timer(timeConstrained ?
                      RenderTimer::SlowRender :
                      RenderTimer::NoTimeout);

    auto fft = ModelById::getAs<FFTModel>(m_sources.fft);
    if (!fft) return 0;

    int sh = fft->getHeight();
    
    int minbin = int(binfory[0] + 0.0001);
    if (minbin >= sh) minbin = sh - 1;
    if (minbin < 0) minbin = 0;

    int nbins  = int(binfory[h-1]) - minbin + 1;
    if (minbin + nbins > sh) nbins = sh - minbin;

    FFTModel::PeakSet peakfreqs;

    int psx = -1;
    
    int start = 0;
    int finish = w;
    int step = 1;

    if (rightToLeft) {
        start = w-1;
        finish = -1;
        step = -1;
    }
    
    int xPixelCount = 0;
    
    vector<float> preparedColumn;

    int modelWidth = fft->getWidth();
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": modelWidth " << modelWidth << endl;
#endif
    
    double minFreq =
        (double(minbin) * fft->getSampleRate()) / fft->getFFTSize();
    double maxFreq =
        (double(minbin + nbins - 1) * fft->getSampleRate()) / fft->getFFTSize();

    bool logarithmic = (m_params.binScale == BinScale::Log);

#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": start = " << start << ", finish = " << finish
            << ", step = " << step << endl;
#endif
    
    for (int x = start; x != finish; x += step) {
        
        // x is the on-canvas pixel coord; sx (later) will be the
        // source column index
        
        ++xPixelCount;
        
        if (binforx[x] < 0) continue;

        int sx0 = binforx[x];
        int sx1 = sx0;
        if (x+1 < w) sx1 = binforx[x+1];
        if (sx0 < 0) sx0 = sx1 - 1;
        if (sx0 < 0) continue;
        if (sx1 <= sx0) sx1 = sx0 + 1;

        vector<float> pixelPeakColumn;
        MagnitudeRange magRange;
        
        for (int sx = sx0; sx < sx1; ++sx) {

            if (sx < 0 || sx >= modelWidth) {
                continue;
            }

            if (sx != psx) {
                preparedColumn = getColumn(sx, minbin, nbins, fft);
                magRange.sample(preparedColumn);
                psx = sx;
            }

            if (sx == sx0) {
                pixelPeakColumn = preparedColumn;
                peakfreqs = fft->getPeakFrequencies(FFTModel::AllPeaks, sx,
                                                    minbin, minbin + nbins - 1);
            } else {
                for (int i = 0; in_range_for(pixelPeakColumn, i); ++i) {
                    pixelPeakColumn[i] = std::max(pixelPeakColumn[i],
                                                  preparedColumn[i]);
                }
            }
        }

        if (!pixelPeakColumn.empty()) {

#ifdef DEBUG_COLOUR_PLOT_REPAINT
//            SVDEBUG << "found " << peakfreqs.size() << " peak freqs at column "
//                    << sx0 << endl;
#endif

            for (FFTModel::PeakSet::const_iterator pi = peakfreqs.begin();
                 pi != peakfreqs.end(); ++pi) {

                int bin = pi->first;
                double freq = pi->second;

                if (bin < minbin) continue;
                if (bin >= minbin + nbins) break;
            
                double value = pixelPeakColumn[bin - minbin];
            
                double y = v->getYForFrequency
                    (freq, minFreq, maxFreq, logarithmic);
            
                int iy = int(y + 0.5);
                if (iy < 0 || iy >= h) continue;

                auto pixel = m_params.colourScale.getPixel(value);

#ifdef DEBUG_COLOUR_PLOT_REPAINT
//                SVDEBUG << "frequency " << freq << " for bin " << bin
//                        << " -> y = " << y << ", iy = " << iy << ", value = "
//                        << value << ", pixel " << pixel << "\n";
#endif
                
                m_drawBuffer.setPixel(x, iy, pixel);
            }

            m_magRanges.push_back(magRange);

        } else {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": pixel peak column for range " << sx0 << " to " << sx1
                    << " is empty" << endl;
#endif
        }

        double fractionComplete = double(xPixelCount) / double(w);
        if (timer.outOfTime(fractionComplete)) {
#ifdef DEBUG_COLOUR_PLOT_REPAINT
            SVDEBUG << "render " << m_sources.source
                    << ": out of time" << endl;
#endif
            updateTimings(timer, xPixelCount);
            return xPixelCount;
        }
    }

    updateTimings(timer, xPixelCount);
    return xPixelCount;
}

void
Colour3DPlotRenderer::updateTimings(const RenderTimer &timer, int xPixelCount)
{
    double secondsPerXPixel = timer.secondsPerItem(xPixelCount);

    // valid if we have enough data points, or if the overall time is
    // massively slow anyway (as we definitely need to warn about that)
    bool valid = (xPixelCount > 20 || secondsPerXPixel > 0.01);

    if (valid) {
        m_secondsPerXPixel = secondsPerXPixel;
        m_secondsPerXPixelValid = true;
    
#ifdef DEBUG_COLOUR_PLOT_REPAINT
    SVDEBUG << "render " << m_sources.source
            << ": across " << xPixelCount
            << " x-pixels, seconds per x-pixel = "
            << m_secondsPerXPixel << " (total = "
            << (xPixelCount * m_secondsPerXPixel) << ")" << endl;
#endif
    }
}

void
Colour3DPlotRenderer::recreateDrawBuffer(int w, int h)
{
    m_drawBuffer = QImage(w, h, QImage::Format_Indexed8);

    for (int pixel = 0; pixel < 256; ++pixel) {
        m_drawBuffer.setColor
            ((unsigned char)pixel,
             m_params.colourScale.getColourForPixel
             (pixel, m_params.colourRotation).rgb());
    }

    m_drawBuffer.fill(0);
    m_magRanges.clear();
}

void
Colour3DPlotRenderer::clearDrawBuffer(int w, int h)
{
    if (m_drawBuffer.width() < w || m_drawBuffer.height() != h) {
        recreateDrawBuffer(w, h);
    } else {
        m_drawBuffer.fill(0);
        m_magRanges.clear();
    }
}

QRect
Colour3DPlotRenderer::findSimilarRegionExtents(QPoint p) const
{
    QImage image = m_cache.getImage();
    ImageRegionFinder finder;
    QRect rect = finder.findRegionExtents(&image, p);
    return rect;
}
