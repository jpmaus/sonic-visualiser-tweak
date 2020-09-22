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

#include "ScrollableImageCache.h"

#include "base/HitCount.h"

#include <iostream>
using namespace std;

//#define DEBUG_SCROLLABLE_IMAGE_CACHE 1

void
ScrollableImageCache::scrollTo(const LayerGeometryProvider *v,
                               sv_frame_t newStartFrame)
{
    static HitCount count("ScrollableImageCache: scrolling");
    
    int dx = (v->getXForFrame(m_startFrame) -
              v->getXForFrame(newStartFrame));
    
#ifdef DEBUG_SCROLLABLE_IMAGE_CACHE
    cerr << "ScrollableImageCache::scrollTo: start frame " << m_startFrame
         << " -> " << newStartFrame << ", dx = " << dx << endl;
#endif

    if (m_startFrame == newStartFrame) {
        // haven't moved
        count.hit();
        return;
    }
        
    m_startFrame = newStartFrame;
        
    if (!isValid()) {
        count.miss();
        return;
    }

    int w = m_image.width();

    if (dx == 0) {
        // haven't moved visibly (even though start frame may have changed)
        count.hit();
        return;
    }

    if (dx <= -w || dx >= w) {
        // scrolled entirely off
        invalidate();
        count.miss();
        return;
    }

    count.partial();
        
    // dx is in range, cache is scrollable

    int dxp = dx;
    if (dxp < 0) dxp = -dxp;

    int copylen = (w - dxp) * int(sizeof(QRgb));
    for (int y = 0; y < m_image.height(); ++y) {
        QRgb *line = (QRgb *)m_image.scanLine(y);
        if (dx < 0) {
            memmove(line, line + dxp, copylen);
        } else {
            memmove(line + dxp, line, copylen);
        }
    }
        
    // update valid area
        
    int px = m_validLeft;
    int pw = m_validWidth;
        
    px += dx;
        
    if (dx < 0) {
        // we scrolled left
        if (px < 0) {
            pw += px;
            px = 0;
            if (pw < 0) {
                pw = 0;
            }
        }
    } else {
        // we scrolled right
        if (px + pw > w) {
            pw = w - px;
            if (pw < 0) {
                pw = 0;
            }
        }
    }

    m_validLeft = px;
    m_validWidth = pw;
}

void
ScrollableImageCache::adjustToTouchValidArea(int &left, int &width,
                                             bool &isLeftOfValidArea) const
{
#ifdef DEBUG_SCROLLABLE_IMAGE_CACHE
    cerr << "ScrollableImageCache::adjustToTouchValidArea: left " << left
         << ", width " << width << endl;
    cerr << "ScrollableImageCache: my left " << m_validLeft
         << ", width " << m_validWidth << " so right " << (m_validLeft + m_validWidth) << endl;
#endif
    if (left < m_validLeft) {
        isLeftOfValidArea = true;
        if (left + width <= m_validLeft + m_validWidth) {
            width = m_validLeft - left;
        }
#ifdef DEBUG_SCROLLABLE_IMAGE_CACHE
        cerr << "ScrollableImageCache: we're left of valid area, adjusted width to " << width << endl;
#endif
    } else {
        isLeftOfValidArea = false;
        width = left + width - (m_validLeft + m_validWidth);
        left = m_validLeft + m_validWidth;
        if (width < 0) width = 0;
#ifdef DEBUG_SCROLLABLE_IMAGE_CACHE
        cerr << "ScrollableImageCache: we're right of valid area, adjusted left to " << left << ", width to " << width << endl;
#endif
    }
}
    
void
ScrollableImageCache::drawImage(int left,
                                int width,
                                QImage image,
                                int imageLeft,
                                int imageWidth)
{
    if (image.height() != m_image.height()) {
        cerr << "ScrollableImageCache::drawImage: ERROR: Supplied image height "
             << image.height() << " does not match cache height "
             << m_image.height() << endl;
        throw std::logic_error("Image height must match cache height in ScrollableImageCache::drawImage");
    }
    if (left < 0 || width < 0 || left + width > m_image.width()) {
        cerr << "ScrollableImageCache::drawImage: ERROR: Target area (left = "
             << left << ", width = " << width << ", so right = " << left + width
             << ") out of bounds for cache of width " << m_image.width() << endl;
        throw std::logic_error("Target area out of bounds in ScrollableImageCache::drawImage");
    }
    if (imageLeft < 0 || imageWidth < 0 ||
        imageLeft + imageWidth > image.width()) {
        cerr << "ScrollableImageCache::drawImage: ERROR: Source area (left = "
             << imageLeft << ", width = " << imageWidth << ", so right = "
             << imageLeft + imageWidth << ") out of bounds for image of "
             << "width " << image.width() << endl;
        throw std::logic_error("Source area out of bounds in ScrollableImageCache::drawImage");
    }
        
    QPainter painter(&m_image);
    painter.drawImage(QRect(left, 0, width, m_image.height()),
                      image,
                      QRect(imageLeft, 0, imageWidth, image.height()));
    painter.end();

    if (!isValid()) {
        m_validLeft = left;
        m_validWidth = width;
        return;
    }
        
    if (left < m_validLeft) {
        if (left + width > m_validLeft + m_validWidth) {
            // new image completely contains the old valid area --
            // use the new area as is
            m_validLeft = left;
            m_validWidth = width;
        } else if (left + width < m_validLeft) {
            // new image completely off left of old valid area --
            // we can't extend the valid area because the bit in
            // between is not valid, so must use the new area only
            m_validLeft = left;
            m_validWidth = width;
        } else {
            // new image overlaps old valid area on left side --
            // use new left edge, and extend width to existing
            // right edge
            m_validWidth = (m_validLeft + m_validWidth) - left;
            m_validLeft = left;
        }
    } else {
        if (left > m_validLeft + m_validWidth) {
            // new image completely off right of old valid area --
            // we can't extend the valid area because the bit in
            // between is not valid, so must use the new area only
            m_validLeft = left;
            m_validWidth = width;
        } else if (left + width > m_validLeft + m_validWidth) {
            // new image overlaps old valid area on right side --
            // use existing left edge, and extend width to new
            // right edge
            m_validWidth = (left + width) - m_validLeft;
            // (m_validLeft unchanged)
        } else {
            // new image completely contained within old valid
            // area -- leave the old area unchanged
        }
    }
}

