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

#ifndef SCROLLABLE_IMAGE_CACHE_H
#define SCROLLABLE_IMAGE_CACHE_H

#include "base/BaseTypes.h"

#include "LayerGeometryProvider.h"

#include <QImage>
#include <QRect>
#include <QPainter>

/**
 * A cached image for a view that scrolls horizontally, such as a
 * spectrogram. The cache object holds an image, reports the size of
 * the image (likely the same as the underlying view, but it's the
 * caller's responsibility to set the size appropriately), can scroll
 * the image, and can report and update which contiguous horizontal
 * range of the image is valid.
 *
 * The only way to *update* the valid area in a cache is to draw to it
 * using the drawImage call.
 */
class ScrollableImageCache
{
public:
    ScrollableImageCache() :
        m_validLeft(0),
        m_validWidth(0),
        m_startFrame(0)
    {}

    void invalidate() {
        m_validWidth = 0;
    }
    
    bool isValid() const {
        return m_validWidth > 0;
    }

    QSize getSize() const {
        return m_image.size();
    }

    /**
     * Set the size of the cache. If the new size differs from the
     * current size, the cache is invalidated.
     */
    void resize(QSize newSize) {
        if (getSize() != newSize) {
            m_image = QImage(newSize, QImage::Format_ARGB32_Premultiplied);
            invalidate();
        }
    }
        
    int getValidLeft() const {
        return m_validLeft;
    }
    
    int getValidWidth() const {
        return m_validWidth;
    }

    int getValidRight() const {
        return m_validLeft + m_validWidth;
    }

    QRect getValidArea() const {
        return QRect(m_validLeft, 0, m_validWidth, m_image.height());
    }
    
    ZoomLevel getZoomLevel() const {
        return m_zoomLevel;
    }

    /**
     * Set the zoom level. If the new zoom level differs from the
     * current one, the cache is invalidated. (Determining whether to
     * invalidate the cache here is the only thing the zoom level is
     * used for.)
     */
    void setZoomLevel(ZoomLevel zoom) {
        using namespace std::rel_ops;
        if (m_zoomLevel != zoom) {
            m_zoomLevel = zoom;
            invalidate();
        }
    }

    sv_frame_t getStartFrame() const {
        return m_startFrame;
    }

    /**
     * Set the start frame. If the new start frame differs from the
     * current one, the cache is invalidated. To scroll, i.e. to set
     * the start frame while retaining cache validity where possible,
     * use scrollTo() instead.
     */
    void setStartFrame(sv_frame_t frame) {
        if (m_startFrame != frame) {
            m_startFrame = frame;
            invalidate();
        }
    }
    
    const QImage &getImage() const {
        return m_image;
    }

    /**
     * Set the new start frame for the cache, according to the
     * geometry of the supplied LayerGeometryProvider, if possible
     * also moving along any existing valid data within the cache so
     * that it continues to be valid for the new start frame.
     */
    void scrollTo(const LayerGeometryProvider *v, sv_frame_t newStartFrame);

    /**
     * Take a left coordinate and width describing a region, and
     * adjust them so that they are contiguous with the cache valid
     * region and so that the union of the adjusted region with the
     * cache valid region contains the supplied region.  Does not
     * modify anything about the cache, only about the arguments.
     */
    void adjustToTouchValidArea(int &left, int &width,
                                bool &isLeftOfValidArea) const;
    
    /**
     * Draw from an image onto the cache. The supplied image must have
     * the same height as the cache and the full height is always
     * drawn. The left and width parameters determine the target
     * region of the cache, the imageLeft and imageWidth parameters
     * the source region of the image.
     */
    void drawImage(int left,
                   int width,
                   QImage image,
                   int imageLeft,
                   int imageWidth);
    
private:
    QImage m_image;
    int m_validLeft;
    int m_validWidth;
    sv_frame_t m_startFrame;
    ZoomLevel m_zoomLevel;
};

#endif
