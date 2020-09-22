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

#include "View.h"
#include "layer/Layer.h"
#include "data/model/Model.h"
#include "base/ZoomConstraint.h"
#include "base/Profiler.h"
#include "base/Pitch.h"
#include "base/Preferences.h"
#include "base/HitCount.h"
#include "ViewProxy.h"

#include "layer/TimeRulerLayer.h"
#include "layer/SingleColourLayer.h"
#include "layer/PaintAssistant.h"

#include "data/model/RelativelyFineZoomConstraint.h"
#include "data/model/RangeSummarisableTimeValueModel.h"

#include "widgets/IconLoader.h"

#include <QPainter>
#include <QPaintEvent>
#include <QRect>
#include <QApplication>
#include <QProgressDialog>
#include <QTextStream>
#include <QFont>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSvgGenerator>

#include <iostream>
#include <cassert>
#include <cmath>

//#define DEBUG_VIEW 1
//#define DEBUG_VIEW_WIDGET_PAINT 1
//#define DEBUG_PROGRESS_STUFF 1
//#define DEBUG_VIEW_SCALE_CHOICE 1

View::View(QWidget *w, bool showProgress) :
    QFrame(w),
    m_id(getNextId()),
    m_centreFrame(0),
    m_zoomLevel(ZoomLevel::FramesPerPixel, 1024),
    m_followPan(true),
    m_followZoom(true),
    m_followPlay(PlaybackScrollPageWithCentre),
    m_followPlayIsDetached(false),
    m_playPointerFrame(0),
    m_showProgress(showProgress),
    m_cache(nullptr),
    m_buffer(nullptr),
    m_cacheValid(false),
    m_cacheCentreFrame(0),
    m_cacheZoomLevel(ZoomLevel::FramesPerPixel, 1024),
    m_selectionCached(false),
    m_deleting(false),
    m_haveSelectedLayer(false),
    m_useAligningProxy(false),
    m_alignmentProgressBar({ {}, nullptr }),
    m_manager(nullptr),
    m_propertyContainer(new ViewPropertyContainer(this))
{
//    SVCERR << "View::View[" << getId() << "]" << endl;
}

View::~View()
{
//    SVCERR << "View::~View[" << getId() << "]" << endl;

    m_deleting = true;
    delete m_propertyContainer;
    delete m_cache;
    delete m_buffer;
}

PropertyContainer::PropertyList
View::getProperties() const
{
    PropertyContainer::PropertyList list;
    list.push_back("Global Scroll");
    list.push_back("Global Zoom");
    list.push_back("Follow Playback");
    return list;
}

QString
View::getPropertyLabel(const PropertyName &pn) const
{
    if (pn == "Global Scroll") return tr("Global Scroll");
    if (pn == "Global Zoom") return tr("Global Zoom");
    if (pn == "Follow Playback") return tr("Follow Playback");
    return "";
}

PropertyContainer::PropertyType
View::getPropertyType(const PropertyContainer::PropertyName &name) const
{
    if (name == "Global Scroll") return PropertyContainer::ToggleProperty;
    if (name == "Global Zoom") return PropertyContainer::ToggleProperty;
    if (name == "Follow Playback") return PropertyContainer::ValueProperty;
    return PropertyContainer::InvalidProperty;
}

int
View::getPropertyRangeAndValue(const PropertyContainer::PropertyName &name,
                               int *min, int *max, int *deflt) const
{
    if (deflt) *deflt = 1;
    if (name == "Global Scroll") return m_followPan;
    if (name == "Global Zoom") return m_followZoom;
    if (name == "Follow Playback") {
        if (min) *min = 0;
        if (max) *max = 2;
        if (deflt) *deflt = int(PlaybackScrollPageWithCentre);
        switch (m_followPlay) {
        case PlaybackScrollContinuous: return 0;
        case PlaybackScrollPageWithCentre: case PlaybackScrollPage: return 1;
        case PlaybackIgnore: return 2;
        }
    }
    if (min) *min = 0;
    if (max) *max = 0;
    if (deflt) *deflt = 0;
    return 0;
}

QString
View::getPropertyValueLabel(const PropertyContainer::PropertyName &name,
                            int value) const
{
    if (name == "Follow Playback") {
        switch (value) {
        default:
        case 0: return tr("Scroll");
        case 1: return tr("Page");
        case 2: return tr("Off");
        }
    }
    return tr("<unknown>");
}

void
View::setProperty(const PropertyContainer::PropertyName &name, int value)
{
    if (name == "Global Scroll") {
        setFollowGlobalPan(value != 0);
    } else if (name == "Global Zoom") {
        setFollowGlobalZoom(value != 0);
    } else if (name == "Follow Playback") {
        switch (value) {
        default:
        case 0: setPlaybackFollow(PlaybackScrollContinuous); break;
        case 1: setPlaybackFollow(PlaybackScrollPageWithCentre); break;
        case 2: setPlaybackFollow(PlaybackIgnore); break;
        }
    }
}

int
View::getPropertyContainerCount() const
{
    return int(m_fixedOrderLayers.size()) + 1; // the 1 is for me
}

const PropertyContainer *
View::getPropertyContainer(int i) const
{
    return (const PropertyContainer *)(((View *)this)->
                                       getPropertyContainer(i));
}

PropertyContainer *
View::getPropertyContainer(int i)
{
    if (i == 0) return m_propertyContainer;
    return m_fixedOrderLayers[i-1];
}

bool
View::getVisibleExtentsForUnit(QString unit,
                               double &min, double &max,
                               bool &log) const
{
#ifdef DEBUG_VIEW_SCALE_CHOICE
    SVCERR << "View[" << getId() << "]::getVisibleExtentsForUnit("
           << unit << ")" << endl;
#endif
    
    Layer *layer = getScaleProvidingLayerForUnit(unit);

    QString layerUnit;
    double layerMin, layerMax;

    if (!layer) {
#ifdef DEBUG_VIEW_SCALE_CHOICE
        SVCERR << "View[" << getId() << "]::getVisibleExtentsForUnit("
               << unit << "): No scale-providing layer for this unit, "
               << "taking union of extents of layers with this unit" << endl;
#endif
        bool haveAny = false;
        bool layerLog;
        for (auto i = m_layerStack.rbegin(); i != m_layerStack.rend(); ++i) { 
            Layer *layer = *i;
            if (layer->getValueExtents(layerMin, layerMax,
                                       layerLog, layerUnit)) {
                if (unit.toLower() != layerUnit.toLower()) {
                    continue;
                }
                if (!haveAny || layerMin < min) {
                    min = layerMin;
                }
                if (!haveAny || layerMax > max) {
                    max = layerMax;
                }
                if (!haveAny || layerLog) {
                    log = layerLog;
                }
                haveAny = true;
            }
        }
        return haveAny;
    }

    return (layer->getValueExtents(layerMin, layerMax, log, layerUnit) &&
            layer->getDisplayExtents(min, max));
}
        
Layer *
View::getScaleProvidingLayerForUnit(QString unit) const
{
    // Return the layer which is used to provide the min/max/log for
    // any auto-align layer of a given unit. This is also the layer
    // that will draw the scale, if possible.
    //
    // The returned layer is
    // 
    // - the topmost visible layer having that unit that is not also
    // auto-aligning; or if there is no such layer,
    //
    // - the topmost layer of any visibility having that unit that is
    // not also auto-aligning (because a dormant layer can still draw
    // a scale, and it makes sense for layers aligned to it not to
    // jump about when its visibility is toggled); or if there is no
    // such layer,
    //
    // - none

    Layer *dormantOption = nullptr;
    
    for (auto i = m_layerStack.rbegin(); i != m_layerStack.rend(); ++i) { 

        Layer *layer = *i;

#ifdef DEBUG_VIEW_SCALE_CHOICE
        SVCERR << "View[" << getId() << "]::getScaleProvidingLayerForUnit("
               << unit << "): Looking at layer " << layer
               << " (" << layer->getLayerPresentationName() << ")" << endl;
#endif
        
        QString layerUnit;
        double layerMin = 0.0, layerMax = 0.0;
        bool layerLog = false;

        if (!layer->getValueExtents(layerMin, layerMax, layerLog, layerUnit)) {
#ifdef DEBUG_VIEW_SCALE_CHOICE
            SVCERR << "... it has no value extents" << endl;
#endif
            continue;
        }

        if (layerUnit.toLower() != unit.toLower()) {
#ifdef DEBUG_VIEW_SCALE_CHOICE
            SVCERR << "... it has the wrong unit (" << layerUnit << ")" << endl;
#endif
            continue;
        }

        double displayMin = 0.0, displayMax = 0.0;
        if (!layer->getDisplayExtents(displayMin, displayMax)) {
#ifdef DEBUG_VIEW_SCALE_CHOICE
            SVCERR << "... it has no display extents (is auto-aligning or not alignable)" << endl;
#endif
            continue;
        }

        if (layer->isLayerDormant(this)) {
#ifdef DEBUG_VIEW_SCALE_CHOICE
            SVCERR << "... it's dormant" << endl;
#endif
            if (!dormantOption) {
                dormantOption = layer;
            }
            continue;
        }

#ifdef DEBUG_VIEW_SCALE_CHOICE
        SVCERR << "... it's good" << endl;
#endif
        return layer;
    }

    return dormantOption;
}

bool
View::getVisibleExtentsForAnyUnit(double &min, double &max,
                                  bool &log, QString &unit) const
{
    bool have = false;

    // Iterate in reverse order, so as to return display extents of
    // topmost layer that fits the bill
    
    for (auto i = m_layerStack.rbegin(); i != m_layerStack.rend(); ++i) { 

        Layer *layer = *i;

        if (layer->isLayerDormant(this)) {
            continue;
        }
        
        QString layerUnit;
        double layerMin = 0.0, layerMax = 0.0;
        bool layerLog = false;

        if (!layer->getValueExtents(layerMin, layerMax, layerLog, layerUnit)) {
            continue;
        }
        if (layerUnit == "") {
            continue;
        }

        double displayMin = 0.0, displayMax = 0.0;
        
        if (layer->getDisplayExtents(displayMin, displayMax)) {

            min = displayMin;
            max = displayMax;
            log = layerLog;
            unit = layerUnit;
            have = true;
            break;
        }
    }

    return have;
}

int
View::getTextLabelYCoord(const Layer *layer, QPainter &paint) const
{
    std::map<int, Layer *> sortedLayers;

    for (LayerList::const_iterator i = m_layerStack.begin();
         i != m_layerStack.end(); ++i) { 
        if ((*i)->needsTextLabelHeight()) {
            sortedLayers[(*i)->getExportId()] = *i;
        }
    }

    int y = scalePixelSize(15) + paint.fontMetrics().ascent();

    for (std::map<int, Layer *>::const_iterator i = sortedLayers.begin();
         i != sortedLayers.end(); ++i) {
        if (i->second == layer) break;
        y += paint.fontMetrics().height();
    }

    return y;
}

void
View::propertyContainerSelected(View *client, PropertyContainer *pc)
{
    if (client != this) return;
    
    if (pc == m_propertyContainer) {
        if (m_haveSelectedLayer) {
            m_haveSelectedLayer = false;
            update();
        }
        return;
    }

    m_cacheValid = false;

    Layer *selectedLayer = nullptr;

    for (LayerList::iterator i = m_layerStack.begin(); i != m_layerStack.end(); ++i) {
        if (*i == pc) {
            selectedLayer = *i;
            m_layerStack.erase(i);
            break;
        }
    }

    if (selectedLayer) {
        m_haveSelectedLayer = true;
        m_layerStack.push_back(selectedLayer);
        update();
    } else {
        m_haveSelectedLayer = false;
    }

    emit propertyContainerSelected(pc);
}

void
View::toolModeChanged()
{
//    SVDEBUG << "View::toolModeChanged(" << m_manager->getToolMode() << ")" << endl;
}

void
View::overlayModeChanged()
{
    m_cacheValid = false;
    update();
}

void
View::zoomWheelsEnabledChanged()
{
    // subclass might override this
}

sv_frame_t
View::getStartFrame() const
{
    return getFrameForX(0);
}

sv_frame_t
View::getEndFrame() const
{
    return getFrameForX(width()) - 1;
}

void
View::setStartFrame(sv_frame_t f)
{
    setCentreFrame(f + sv_frame_t(round
                                  (m_zoomLevel.pixelsToFrames(width() / 2))));
}

bool
View::setCentreFrame(sv_frame_t f, bool e)
{
    bool changeVisible = false;

#ifdef DEBUG_VIEW
    SVCERR << "View[" << getId() << "]::setCentreFrame: from " << m_centreFrame
           << " to " << f << endl;
#endif

    if (m_centreFrame != f) {

        sv_frame_t formerCentre = m_centreFrame;
        m_centreFrame = f;
        
        if (m_zoomLevel.zone == ZoomLevel::PixelsPerFrame) {

#ifdef DEBUG_VIEW
            SVCERR << "View[" << getId() << "]::setCentreFrame: in PixelsPerFrame zone, so change must be visible" << endl;
#endif
            update();
            changeVisible = true;

        } else {
        
            int formerPixel = int(formerCentre / m_zoomLevel.level);
            int newPixel = int(m_centreFrame / m_zoomLevel.level);
        
            if (newPixel != formerPixel) {

#ifdef DEBUG_VIEW
                SVCERR << "View[" << getId() << "]::setCentreFrame: newPixel " << newPixel << ", formerPixel " << formerPixel << endl;
#endif
                // ensure the centre frame is a multiple of the zoom level
                m_centreFrame = sv_frame_t(newPixel) * m_zoomLevel.level;

#ifdef DEBUG_VIEW
                SVCERR << "View[" << getId()
                       << "]::setCentreFrame: centre frame rounded to "
                       << m_centreFrame << " (zoom level is "
                       << m_zoomLevel.level << ")" << endl;
#endif
                
                update();
                changeVisible = true;
            }
        }

        if (e) {
            sv_frame_t rf = alignToReference(m_centreFrame);
#ifdef DEBUG_VIEW
            SVCERR << "View[" << getId() << "]::setCentreFrame(" << f
                 << "): m_centreFrame = " << m_centreFrame
                 << ", emitting centreFrameChanged with aligned frame "
                 << rf << endl;
#endif
            emit centreFrameChanged(rf, m_followPan, m_followPlay);
        }
    }

    return changeVisible;
}

int
View::getXForFrame(sv_frame_t frame) const
{
    // In FramesPerPixel mode, the pixel should be the one "covering"
    // the given frame, i.e. to the "left" of it - not necessarily the
    // nearest boundary.
    
    sv_frame_t level = m_zoomLevel.level;
    sv_frame_t fdiff = frame - m_centreFrame;
    int result = 0;

    bool inRange = false;
    if (m_zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        inRange = ((fdiff / level) < sv_frame_t(INT_MAX) &&
                   (fdiff / level) > sv_frame_t(INT_MIN));
    } else {
        inRange = (fdiff < sv_frame_t(INT_MAX) / level &&
                   fdiff > sv_frame_t(INT_MIN) / level);
    }

    if (inRange) {
        
        sv_frame_t adjusted;

        if (m_zoomLevel.zone == ZoomLevel::FramesPerPixel) {
            sv_frame_t roundedCentreFrame = (m_centreFrame / level) * level;
            fdiff = frame - roundedCentreFrame;
            adjusted = fdiff / level;
            if ((fdiff < 0) && ((fdiff % level) != 0)) {
                --adjusted; // round to the left
            }
        } else {
            adjusted = fdiff * level;
        }

        adjusted = adjusted + (width()/2);

        if (adjusted > INT_MAX || adjusted < INT_MIN) {
            inRange = false;
        } else {
            result = int(adjusted);
        }
    }

    if (!inRange) {
        SVCERR << "ERROR: Frame " << frame
               << " is out of range in View::getXForFrame" << endl;
        SVCERR << "ERROR: (centre frame = " << getCentreFrame() << ", fdiff = "
               << fdiff << ", zoom level = " << m_zoomLevel << ")" << endl;
        SVCERR << "ERROR: This is a logic error: getXForFrame should not be "
               << "called for locations unadjacent to the current view"
               << endl;
        return 0;
    }

#ifdef DEBUG_VIEW
    if (m_zoomLevel.zone == ZoomLevel::PixelsPerFrame) {
        sv_frame_t reversed = getFrameForX(result);
        if (reversed != frame) {
            SVCERR << "View[" << getId() << "]::getXForFrame: WARNING: Converted frame " << frame << " to x " << result << " in PixelsPerFrame zone, but the reverse conversion gives frame " << reversed << " (error = " << reversed - frame << ")" << endl;
            SVCERR << "(centre frame = " << getCentreFrame() << ", fdiff = "
                   << fdiff << ", level = " << level << ", centre % level = "
                   << (getCentreFrame() % level) << ", fdiff % level = "
                   << (fdiff % level) << ", frame % level = "
                   << (frame % level) << ", reversed % level = "
                   << (reversed % level) << ")" << endl;
        }
    }
#endif

    return result;
}

sv_frame_t
View::getFrameForX(int x) const
{
    // Note, this must always return a value that is on a zoom-level
    // boundary - regardless of whether the nominal centre frame is on
    // such a boundary or not. (It is legitimate for the centre frame
    // not to be on a zoom-level boundary, because the centre frame
    // may be shared with other views having different zoom levels.)

    // In FramesPerPixel mode, the frame returned for a given x should
    // be the first for which getXForFrame(frame) == x; a corollary is
    // that if the centre frame is not on a zoom-level boundary, then
    // getFrameForX(x) should not return the centre frame for any x.
    
    // In PixelsPerFrame mode, the frame returned should be the one
    // immediately left of the given pixel, not necessarily the
    // nearest.

    int diff = x - (width()/2);
    sv_frame_t level = m_zoomLevel.level;
    sv_frame_t fdiff, result;
    
    if (m_zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        sv_frame_t roundedCentreFrame = (m_centreFrame / level) * level;
        fdiff = diff * level;
        result = fdiff + roundedCentreFrame;
    } else {
        fdiff = diff / level;
        if ((diff < 0) && ((diff % level) != 0)) {
            --fdiff; // round to the left
        }
        result = fdiff + m_centreFrame;
    }

#ifdef DEBUG_VIEW_WIDGET_PAINT
/*
    if (x == 0) {
        SVCERR << "getFrameForX(" << x << "): diff = " << diff << ", fdiff = "
               << fdiff << ", m_centreFrame = " << m_centreFrame
               << ", level = " << m_zoomLevel.level
               << ", diff % level = " << (diff % m_zoomLevel.level)
               << ", nominal " << fdiff + m_centreFrame
               << ", will return " << result
               << endl;
    }
*/    
#endif
    
#ifdef DEBUG_VIEW
    if (m_zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        int reversed = getXForFrame(result);
        if (reversed != x) {
            SVCERR << "View[" << getId() << "]::getFrameForX: WARNING: Converted pixel " << x << " to frame " << result << " in FramesPerPixel zone, but the reverse conversion gives pixel " << reversed << " (error = " << reversed - x << ")" << endl;
            SVCERR << "(centre frame = " << getCentreFrame()
                   << ", width/2 = " << width()/2 << ", diff = " << diff
                   << ", fdiff = " << fdiff << ", level = " << level
                   << ", centre % level = " << (getCentreFrame() % level)
                   << ", fdiff % level = " << (fdiff % level)
                   << ", frame % level = " << (result % level) << ")" << endl;
        }
    }
#endif
    
    return result;
}

double
View::getYForFrequency(double frequency,
                       double minf,
                       double maxf, 
                       bool logarithmic) const
{
    Profiler profiler("View::getYForFrequency");

    int h = height();

    if (logarithmic) {

        static double lastminf = 0.0, lastmaxf = 0.0;
        static double logminf = 0.0, logmaxf = 0.0;

        if (lastminf != minf) {
            lastminf = (minf == 0.0 ? 1.0 : minf);
            logminf = log10(minf);
        }
        if (lastmaxf != maxf) {
            lastmaxf = (maxf < lastminf ? lastminf : maxf);
            logmaxf = log10(maxf);
        }

        if (logminf == logmaxf) return 0;
        return h - (h * (log10(frequency) - logminf)) / (logmaxf - logminf);

    } else {
        
        if (minf == maxf) return 0;
        return h - (h * (frequency - minf)) / (maxf - minf);
    }
}

double
View::getFrequencyForY(double y,
                       double minf,
                       double maxf,
                       bool logarithmic) const
{
    double h = height();

    if (logarithmic) {

        static double lastminf = 0.0, lastmaxf = 0.0;
        static double logminf = 0.0, logmaxf = 0.0;

        if (lastminf != minf) {
            lastminf = (minf == 0.0 ? 1.0 : minf);
            logminf = log10(minf);
        }
        if (lastmaxf != maxf) {
            lastmaxf = (maxf < lastminf ? lastminf : maxf);
            logmaxf = log10(maxf);
        }

        if (logminf == logmaxf) return 0;
        return pow(10.0, logminf + ((logmaxf - logminf) * (h - y)) / h);

    } else {

        if (minf == maxf) return 0;
        return minf + ((h - y) * (maxf - minf)) / h;
    }
}

ZoomLevel
View::getZoomLevel() const
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
//        cout << "zoom level: " << m_zoomLevel << endl;
#endif
    return m_zoomLevel;
}

int
View::effectiveDevicePixelRatio() const
{
#ifdef Q_OS_MAC
    int dpratio = devicePixelRatio();
    if (dpratio > 1) {
        QSettings settings;
        settings.beginGroup("Preferences");
        if (!settings.value("scaledHiDpi", true).toBool()) {
            dpratio = 1;
        }
        settings.endGroup();
    }
    return dpratio;
#else
    return 1;
#endif
}

void
View::setZoomLevel(ZoomLevel z)
{
//!!!    int dpratio = effectiveDevicePixelRatio();
//    if (z < dpratio) return;
//    if (z < 1) z = 1;
    if (m_zoomLevel == z) {
        return;
    }
    m_zoomLevel = z;
    emit zoomLevelChanged(z, m_followZoom);
    update();
}

bool
View::hasLightBackground() const
{
    bool darkPalette = false;
    if (m_manager) darkPalette = m_manager->getGlobalDarkBackground();

    Layer::ColourSignificance maxSignificance = Layer::ColourAbsent;
    bool mostSignificantHasDarkBackground = false;
    
    for (LayerList::const_iterator i = m_layerStack.begin();
         i != m_layerStack.end(); ++i) {

        Layer::ColourSignificance s = (*i)->getLayerColourSignificance();
        bool light = (*i)->hasLightBackground();

        if (int(s) > int(maxSignificance)) {
            maxSignificance = s;
            mostSignificantHasDarkBackground = !light;
        } else if (s == maxSignificance && !light) {
            mostSignificantHasDarkBackground = true;
        }
    }

    if (int(maxSignificance) >= int(Layer::ColourDistinguishes)) {
        return !mostSignificantHasDarkBackground;
    } else {
        return !darkPalette;
    }
}

QColor
View::getBackground() const
{
    bool light = hasLightBackground();

    QColor widgetbg = palette().window().color();
    bool widgetLight =
        (widgetbg.red() + widgetbg.green() + widgetbg.blue()) > 384;

    if (widgetLight == light) {
        if (widgetLight) {
            return widgetbg.lighter();
        } else {
            return widgetbg.darker();
        }
    }
    else if (light) return Qt::white;
    else return Qt::black;
}

QColor
View::getForeground() const
{
    bool light = hasLightBackground();

    QColor widgetfg = palette().text().color();
    bool widgetLight =
        (widgetfg.red() + widgetfg.green() + widgetfg.blue()) > 384;

    if (widgetLight != light) return widgetfg;
    else if (light) return Qt::black;
    else return Qt::white;
}

void
View::addLayer(Layer *layer)
{
    m_cacheValid = false;

    SingleColourLayer *scl = dynamic_cast<SingleColourLayer *>(layer);
    if (scl) scl->setDefaultColourFor(this);

    m_fixedOrderLayers.push_back(layer);
    m_layerStack.push_back(layer);

    QProgressBar *pb = new QProgressBar(this);
    pb->setMinimum(0);
    pb->setMaximum(0);
    pb->setFixedWidth(80);
    pb->setTextVisible(false);

    QPushButton *cancel = new QPushButton(this);
    cancel->setIcon(IconLoader().load("cancel"));
    cancel->setFlat(true);
    int scaled20 = scalePixelSize(20);
    cancel->setFixedSize(QSize(scaled20, scaled20));
    connect(cancel, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    
    ProgressBarRec pbr;
    pbr.cancel = cancel;
    pbr.bar = pb;
    pbr.lastStallCheckValue = 0;
    pbr.stallCheckTimer = new QTimer();
    connect(pbr.stallCheckTimer, SIGNAL(timeout()), this,
            SLOT(progressCheckStalledTimerElapsed()));

    m_progressBars[layer] = pbr;

    QFont f(pb->font());
    int fs = Preferences::getInstance()->getViewFontSize();
    f.setPointSize(std::min(fs, int(ceil(fs * 0.85))));

    cancel->hide();

    pb->setFont(f);
    pb->hide();
    
    connect(layer, SIGNAL(layerParametersChanged()),
            this,    SLOT(layerParametersChanged()));
    connect(layer, SIGNAL(layerParameterRangesChanged()),
            this,    SLOT(layerParameterRangesChanged()));
    connect(layer, SIGNAL(layerMeasurementRectsChanged()),
            this,    SLOT(layerMeasurementRectsChanged()));
    connect(layer, SIGNAL(layerNameChanged()),
            this,    SLOT(layerNameChanged()));
    connect(layer, SIGNAL(modelChanged(ModelId)),
            this,    SLOT(modelChanged(ModelId)));
    connect(layer, SIGNAL(modelCompletionChanged(ModelId)),
            this,    SLOT(modelCompletionChanged(ModelId)));
    connect(layer, SIGNAL(modelAlignmentCompletionChanged(ModelId)),
            this,    SLOT(modelAlignmentCompletionChanged(ModelId)));
    connect(layer, SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
            this,    SLOT(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
    connect(layer, SIGNAL(modelReplaced()),
            this,    SLOT(modelReplaced()));

    update();

    emit propertyContainerAdded(layer);
}

void
View::removeLayer(Layer *layer)
{
    if (m_deleting) {
        return;
    }

    m_cacheValid = false;

    for (LayerList::iterator i = m_fixedOrderLayers.begin();
         i != m_fixedOrderLayers.end();
         ++i) {
        if (*i == layer) {
            m_fixedOrderLayers.erase(i);
            break;
        }
    }

    for (LayerList::iterator i = m_layerStack.begin(); 
         i != m_layerStack.end();
         ++i) {
        if (*i == layer) {
            m_layerStack.erase(i);
            if (m_progressBars.find(layer) != m_progressBars.end()) {
                delete m_progressBars[layer].bar;
                delete m_progressBars[layer].cancel;
                delete m_progressBars[layer].stallCheckTimer;
                m_progressBars.erase(layer);
            }
            break;
        }
    }
    
    disconnect(layer, SIGNAL(layerParametersChanged()),
               this,    SLOT(layerParametersChanged()));
    disconnect(layer, SIGNAL(layerParameterRangesChanged()),
               this,    SLOT(layerParameterRangesChanged()));
    disconnect(layer, SIGNAL(layerNameChanged()),
               this,    SLOT(layerNameChanged()));
    disconnect(layer, SIGNAL(modelChanged(ModelId)),
               this,    SLOT(modelChanged(ModelId)));
    disconnect(layer, SIGNAL(modelCompletionChanged(ModelId)),
               this,    SLOT(modelCompletionChanged(ModelId)));
    disconnect(layer, SIGNAL(modelAlignmentCompletionChanged(ModelId)),
               this,    SLOT(modelAlignmentCompletionChanged(ModelId)));
    disconnect(layer, SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
               this,    SLOT(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
    disconnect(layer, SIGNAL(modelReplaced()),
               this,    SLOT(modelReplaced()));

    update();

    emit propertyContainerRemoved(layer);
}

Layer *
View::getInteractionLayer()
{
    Layer *sl = getSelectedLayer();
    if (sl && !(sl->isLayerDormant(this))) {
        return sl;
    }
    if (!m_layerStack.empty()) {
        int n = getLayerCount();
        while (n > 0) {
            --n;
            Layer *layer = getLayer(n);
            if (!(layer->isLayerDormant(this))) {
                return layer;
            }
        }
    }
    return nullptr;
}

const Layer *
View::getInteractionLayer() const
{
    return const_cast<const Layer *>(const_cast<View *>(this)->getInteractionLayer());
}

Layer *
View::getSelectedLayer()
{
    if (m_haveSelectedLayer && !m_layerStack.empty()) {
        return getLayer(getLayerCount() - 1);
    } else {
        return nullptr;
    }
}

const Layer *
View::getSelectedLayer() const
{
    return const_cast<const Layer *>(const_cast<View *>(this)->getSelectedLayer());
}

void
View::setViewManager(ViewManager *manager)
{
    if (m_manager) {
        m_manager->disconnect(this, SLOT(globalCentreFrameChanged(sv_frame_t)));
        m_manager->disconnect(this, SLOT(viewCentreFrameChanged(View *, sv_frame_t)));
        m_manager->disconnect(this, SLOT(viewManagerPlaybackFrameChanged(sv_frame_t)));
        m_manager->disconnect(this, SLOT(viewZoomLevelChanged(View *, ZoomLevel, bool)));
        m_manager->disconnect(this, SLOT(toolModeChanged()));
        m_manager->disconnect(this, SLOT(selectionChanged()));
        m_manager->disconnect(this, SLOT(overlayModeChanged()));
        m_manager->disconnect(this, SLOT(zoomWheelsEnabledChanged()));
        disconnect(m_manager, SLOT(viewCentreFrameChanged(sv_frame_t, bool, PlaybackFollowMode)));
        disconnect(m_manager, SLOT(zoomLevelChanged(ZoomLevel, bool)));
    }

    m_manager = manager;

    connect(m_manager, SIGNAL(globalCentreFrameChanged(sv_frame_t)),
            this, SLOT(globalCentreFrameChanged(sv_frame_t)));
    connect(m_manager, SIGNAL(viewCentreFrameChanged(View *, sv_frame_t)),
            this, SLOT(viewCentreFrameChanged(View *, sv_frame_t)));
    connect(m_manager, SIGNAL(playbackFrameChanged(sv_frame_t)),
            this, SLOT(viewManagerPlaybackFrameChanged(sv_frame_t)));

    connect(m_manager, SIGNAL(viewZoomLevelChanged(View *, ZoomLevel, bool)),
            this, SLOT(viewZoomLevelChanged(View *, ZoomLevel, bool)));

    connect(m_manager, SIGNAL(toolModeChanged()),
            this, SLOT(toolModeChanged()));
    connect(m_manager, SIGNAL(selectionChanged()),
            this, SLOT(selectionChanged()));
    connect(m_manager, SIGNAL(inProgressSelectionChanged()),
            this, SLOT(selectionChanged()));
    connect(m_manager, SIGNAL(overlayModeChanged()),
            this, SLOT(overlayModeChanged()));
    connect(m_manager, SIGNAL(showCentreLineChanged()),
            this, SLOT(overlayModeChanged()));
    connect(m_manager, SIGNAL(zoomWheelsEnabledChanged()),
            this, SLOT(zoomWheelsEnabledChanged()));

    connect(this, SIGNAL(centreFrameChanged(sv_frame_t, bool,
                                            PlaybackFollowMode)),
            m_manager, SLOT(viewCentreFrameChanged(sv_frame_t, bool,
                                                   PlaybackFollowMode)));

    connect(this, SIGNAL(zoomLevelChanged(ZoomLevel, bool)),
            m_manager, SLOT(viewZoomLevelChanged(ZoomLevel, bool)));

    switch (m_followPlay) {
        
    case PlaybackScrollPage:
    case PlaybackScrollPageWithCentre:
        setCentreFrame(m_manager->getGlobalCentreFrame(), false);
        break;

    case PlaybackScrollContinuous:
        setCentreFrame(m_manager->getPlaybackFrame(), false);
        break;

    case PlaybackIgnore:
        if (m_followPan) {
            setCentreFrame(m_manager->getGlobalCentreFrame(), false);
        }
        break;
    }

    if (m_followZoom) setZoomLevel(m_manager->getGlobalZoom());

    movePlayPointer(getAlignedPlaybackFrame());

    toolModeChanged();
}

void
View::setViewManager(ViewManager *vm, sv_frame_t initialCentreFrame)
{
    setViewManager(vm);
    setCentreFrame(initialCentreFrame, false);
}

void
View::setFollowGlobalPan(bool f)
{
    m_followPan = f;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::setFollowGlobalZoom(bool f)
{
    m_followZoom = f;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::setPlaybackFollow(PlaybackFollowMode m)
{
    m_followPlay = m;
    emit propertyContainerPropertyChanged(m_propertyContainer);
}

void
View::modelChanged(ModelId modelId)
{
#if defined(DEBUG_VIEW_WIDGET_PAINT) || defined(DEBUG_PROGRESS_STUFF)
    SVCERR << "View[" << getId() << "]::modelChanged(" << modelId << ")" << endl;
#endif

    // If the model that has changed is not used by any of the cached
    // layers, we won't need to recreate the cache
    
    bool recreate = false;

    bool discard;
    LayerList scrollables = getScrollableBackLayers(false, discard);
    for (LayerList::const_iterator i = scrollables.begin();
         i != scrollables.end(); ++i) {
        if ((*i)->getModel() == modelId) {
            recreate = true;
            break;
        }
    }

    if (recreate) {
        m_cacheValid = false;
    }

    emit layerModelChanged();

    checkProgress(modelId);

    update();
}

void
View::modelChangedWithin(ModelId modelId,
                         sv_frame_t startFrame, sv_frame_t endFrame)
{
    sv_frame_t myStartFrame = getStartFrame();
    sv_frame_t myEndFrame = getEndFrame();

#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR << "View[" << getId() << "]::modelChangedWithin(" << startFrame << "," << endFrame << ") [me " << myStartFrame << "," << myEndFrame << "]" << endl;
#endif

    if (myStartFrame > 0 && endFrame < myStartFrame) {
        checkProgress(modelId);
        return;
    }
    if (startFrame > myEndFrame) {
        checkProgress(modelId);
        return;
    }

    // If the model that has changed is not used by any of the cached
    // layers, we won't need to recreate the cache
    
    bool recreate = false;

    bool discard;
    LayerList scrollables = getScrollableBackLayers(false, discard);
    for (LayerList::const_iterator i = scrollables.begin();
         i != scrollables.end(); ++i) {
        if ((*i)->getModel() == modelId) {
            recreate = true;
            break;
        }
    }

    if (recreate) {
        m_cacheValid = false;
    }

    if (startFrame < myStartFrame) startFrame = myStartFrame;
    if (endFrame > myEndFrame) endFrame = myEndFrame;

    checkProgress(modelId);

    update();
}    

void
View::modelCompletionChanged(ModelId modelId)
{
#ifdef DEBUG_PROGRESS_STUFF
    SVCERR << "View[" << getId() << "]::modelCompletionChanged(" << modelId << ")" << endl;
#endif
    checkProgress(modelId);
}

void
View::modelAlignmentCompletionChanged(ModelId modelId)
{
#ifdef DEBUG_PROGRESS_STUFF
    SVCERR << "View[" << getId() << "]::modelAlignmentCompletionChanged(" << modelId << ")" << endl;
#endif
    checkAlignmentProgress(modelId);
}

void
View::modelReplaced()
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR << "View[" << getId() << "]::modelReplaced()" << endl;
#endif
    m_cacheValid = false;
    update();
}

void
View::layerParametersChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());

#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVDEBUG << "View::layerParametersChanged()" << endl;
#endif

    m_cacheValid = false;
    update();

    if (layer) {
        emit propertyContainerPropertyChanged(layer);
    }
}

void
View::layerParameterRangesChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) emit propertyContainerPropertyRangeChanged(layer);
}

void
View::layerMeasurementRectsChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) update();
}

void
View::layerNameChanged()
{
    Layer *layer = dynamic_cast<Layer *>(sender());
    if (layer) emit propertyContainerNameChanged(layer);
}

void
View::globalCentreFrameChanged(sv_frame_t rf)
{
    if (m_followPan) {
        sv_frame_t f = alignFromReference(rf);
#ifdef DEBUG_VIEW
        SVCERR << "View[" << getId() << "]::globalCentreFrameChanged(" << rf
                  << "): setting centre frame to " << f << endl;
#endif
        setCentreFrame(f, false);
    }
}

void
View::viewCentreFrameChanged(View *, sv_frame_t )
{
    // We do nothing with this, but a subclass might
}

void
View::viewManagerPlaybackFrameChanged(sv_frame_t f)
{
    if (m_manager) {
        if (sender() != m_manager) return;
    }

#ifdef DEBUG_VIEW        
    SVCERR << "View[" << getId() << "]::viewManagerPlaybackFrameChanged(" << f << ")" << endl;
#endif

    f = getAlignedPlaybackFrame();

#ifdef DEBUG_VIEW
    SVCERR << " -> aligned frame = " << f << endl;
#endif

    movePlayPointer(f);
}

void
View::movePlayPointer(sv_frame_t newFrame)
{
#ifdef DEBUG_VIEW
    SVCERR << "View[" << getId() << "]::movePlayPointer(" << newFrame << ")" << endl;
#endif

    if (m_playPointerFrame == newFrame) return;
    bool visibleChange =
        (getXForFrame(m_playPointerFrame) != getXForFrame(newFrame));

#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR << "View[" << getId() << "]::movePlayPointer: moving from "
           << m_playPointerFrame << " to " << newFrame << ", visible = "
           << visibleChange << endl;
#endif
    
    sv_frame_t oldPlayPointerFrame = m_playPointerFrame;
    m_playPointerFrame = newFrame;
    if (!visibleChange) return;

    bool somethingGoingOn =
        ((QApplication::mouseButtons() != Qt::NoButton) ||
         (QApplication::keyboardModifiers() & Qt::AltModifier));

    bool pointerInVisibleArea =
        long(m_playPointerFrame) >= getStartFrame() &&
        (m_playPointerFrame < getEndFrame() ||
         // include old pointer location so we know to refresh when moving out
         oldPlayPointerFrame < getEndFrame());

    switch (m_followPlay) {

    case PlaybackScrollContinuous:
        if (!somethingGoingOn) {
            setCentreFrame(m_playPointerFrame, false);
        }
        break;

    case PlaybackScrollPage:
    case PlaybackScrollPageWithCentre:

        if (!pointerInVisibleArea && somethingGoingOn) {

            m_followPlayIsDetached = true;

        } else if (!pointerInVisibleArea && m_followPlayIsDetached) {

            // do nothing; we aren't tracking until the pointer comes back in

        } else {

            int xold = getXForFrame(oldPlayPointerFrame);
            update(xold - 4, 0, 9, height());

            sv_frame_t w = getEndFrame() - getStartFrame();
            w -= w/5;
            sv_frame_t sf = m_playPointerFrame;
            if (w > 0) {
                sf = (sf / w) * w - w/8;
            }

            if (m_manager &&
                m_manager->isPlaying() &&
                m_manager->getPlaySelectionMode()) {
                MultiSelection::SelectionList selections = m_manager->getSelections();
                if (!selections.empty()) {
                    sv_frame_t selectionStart = selections.begin()->getStartFrame();
                    if (sf < selectionStart - w / 10) {
                        sf = selectionStart - w / 10;
                    }
                }
            }

#ifdef DEBUG_VIEW_WIDGET_PAINT
            SVCERR << "PlaybackScrollPage: f = " << m_playPointerFrame << ", sf = " << sf << ", start frame "
                 << getStartFrame() << endl;
#endif

            // We don't consider scrolling unless the pointer is outside
            // the central visible range already

            int xnew = getXForFrame(m_playPointerFrame);

#ifdef DEBUG_VIEW_WIDGET_PAINT
            SVCERR << "xnew = " << xnew << ", width = " << width() << endl;
#endif

            bool shouldScroll = (xnew > (width() * 7) / 8);

            if (!m_followPlayIsDetached && (xnew < width() / 8)) {
                shouldScroll = true;
            }

            if (xnew > width() / 8) {
                m_followPlayIsDetached = false;
            } else if (somethingGoingOn) {
                m_followPlayIsDetached = true;
            }

            if (!somethingGoingOn && shouldScroll) {
                sv_frame_t offset = getFrameForX(width()/2) - getStartFrame();
                sv_frame_t newCentre = sf + offset;
                bool changed = setCentreFrame(newCentre, false);
                if (changed) {
                    xold = getXForFrame(oldPlayPointerFrame);
                    update(xold - 4, 0, 9, height());
                }
            }

            update(xnew - 4, 0, 9, height());
        }
        break;

    case PlaybackIgnore:
        if (m_playPointerFrame >= getStartFrame() &&
            m_playPointerFrame < getEndFrame()) {
            update();
        }
        break;
    }
}

void
View::viewZoomLevelChanged(View *p, ZoomLevel z, bool locked)
{
#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR  << "View[" << getId() << "]: viewZoomLevelChanged(" << p << ", " << z << ", " << locked << ")" << endl;
#endif
    if (m_followZoom && p != this && locked) {
        setZoomLevel(z);
    }
}

void
View::selectionChanged()
{
    if (m_selectionCached) {
        m_cacheValid = false;
        m_selectionCached = false;
    }
    update();
}

sv_frame_t
View::getFirstVisibleFrame() const
{
    sv_frame_t f0 = getStartFrame();
    sv_frame_t f = getModelsStartFrame();
    if (f0 < 0 || f0 < f) return f;
    return f0;
}

sv_frame_t 
View::getLastVisibleFrame() const
{
    sv_frame_t f0 = getEndFrame();
    sv_frame_t f = getModelsEndFrame();
    if (f0 > f) return f;
    return f0;
}

sv_frame_t
View::getModelsStartFrame() const
{
    bool first = true;
    sv_frame_t startFrame = 0;

    for (Layer *layer: m_layerStack) {

        auto model = ModelById::get(layer->getModel());

        if (model && model->isOK()) {

            sv_frame_t thisStartFrame = model->getStartFrame();

            if (first || thisStartFrame < startFrame) {
                startFrame = thisStartFrame;
            }
            first = false;
        }
    }
    
    return startFrame;
}

sv_frame_t
View::getModelsEndFrame() const
{
    bool first = true;
    sv_frame_t endFrame = 0;

    for (Layer *layer: m_layerStack) {

        auto model = ModelById::get(layer->getModel());

        if (model && model->isOK()) {

            sv_frame_t thisEndFrame = model->getEndFrame();

            if (first || thisEndFrame > endFrame) {
                endFrame = thisEndFrame;
            }
            first = false;
        }
    }

    if (first) return getModelsStartFrame();
    return endFrame;
}

sv_samplerate_t
View::getModelsSampleRate() const
{
    //!!! Just go for the first, for now.  If we were supporting
    // multiple samplerates, we'd probably want to do frame/time
    // conversion in the model

    //!!! nah, this wants to always return the sr of the main model!

    for (Layer *layer: m_layerStack) {

        auto model = ModelById::get(layer->getModel());

        if (model && model->isOK()) {
            return model->getSampleRate();
        }
    }

    return 0;
}

View::ModelSet
View::getModels()
{
    ModelSet models;

    for (int i = 0; i < getLayerCount(); ++i) {

        Layer *layer = getLayer(i);

        if (dynamic_cast<TimeRulerLayer *>(layer)) {
            continue;
        }

        if (layer && !layer->getModel().isNone()) {
            models.insert(layer->getModel());
        }
    }

    return models;
}

ModelId
View::getAligningModel() const
{
    ModelId aligning, reference;
    getAligningAndReferenceModels(aligning, reference);
    return aligning;
}

void
View::getAligningAndReferenceModels(ModelId &aligning,
                                    ModelId &reference) const
{
    if (!m_manager ||
        !m_manager->getAlignMode() ||
        m_manager->getPlaybackModel().isNone()) {
        return;
    }

    ModelId anyModel;

    for (auto layer: m_layerStack) {

        if (!layer) continue;
        if (dynamic_cast<TimeRulerLayer *>(layer)) continue;

        ModelId thisId = layer->getModel();
        auto model = ModelById::get(thisId);
        if (!model) continue;

        anyModel = thisId;

        if (!model->getAlignmentReference().isNone()) {

            if (layer->isLayerOpaque() ||
                std::dynamic_pointer_cast
                <RangeSummarisableTimeValueModel>(model)) {

                aligning = thisId;
                reference = model->getAlignmentReference();
                return;

            } else if (aligning.isNone()) {
                
                aligning = thisId;
                reference = model->getAlignmentReference();
            }
        }
    }

    if (aligning.isNone()) {
        aligning = anyModel;
        reference = {};
    }
}

sv_frame_t
View::alignFromReference(sv_frame_t f) const
{
    if (!m_manager || !m_manager->getAlignMode()) return f;
    auto aligningModel = ModelById::get(getAligningModel());
    if (!aligningModel) return f;
    return aligningModel->alignFromReference(f);
}

sv_frame_t
View::alignToReference(sv_frame_t f) const
{
    if (!m_manager->getAlignMode()) return f;
    auto aligningModel = ModelById::get(getAligningModel());
    if (!aligningModel) return f;
    return aligningModel->alignToReference(f);
}

sv_frame_t
View::getAlignedPlaybackFrame() const
{
    if (!m_manager) return 0;
    sv_frame_t pf = m_manager->getPlaybackFrame();
    if (!m_manager->getAlignMode()) return pf;

    auto aligningModel = ModelById::get(getAligningModel());
    if (!aligningModel) return pf;

    sv_frame_t af = aligningModel->alignFromReference(pf);

    return af;
}

bool
View::areLayersScrollable() const
{
    // True iff all views are scrollable
    for (LayerList::const_iterator i = m_layerStack.begin(); i != m_layerStack.end(); ++i) {
        if (!(*i)->isLayerScrollable(this)) return false;
    }
    return true;
}

View::LayerList
View::getScrollableBackLayers(bool testChanged, bool &changed) const
{
    changed = false;

    // We want a list of all the scrollable layers that are behind the
    // backmost non-scrollable layer.

    LayerList scrollables;
    bool metUnscrollable = false;

    for (LayerList::const_iterator i = m_layerStack.begin(); i != m_layerStack.end(); ++i) {
//        SVDEBUG << "View::getScrollableBackLayers: calling isLayerDormant on layer " << *i << endl;
//        SVCERR << "(name is " << (*i)->objectName() << ")"
//                  << endl;
//        SVDEBUG << "View::getScrollableBackLayers: I am " << getId() << endl;
        if ((*i)->isLayerDormant(this)) continue;
        if ((*i)->isLayerOpaque()) {
            // You can't see anything behind an opaque layer!
            scrollables.clear();
            if (metUnscrollable) break;
        }
        if (!metUnscrollable && (*i)->isLayerScrollable(this)) {
            scrollables.push_back(*i);
        } else {
            metUnscrollable = true;
        }
    }

    if (testChanged && scrollables != m_lastScrollableBackLayers) {
        m_lastScrollableBackLayers = scrollables;
        changed = true;
    }
    return scrollables;
}

View::LayerList
View::getNonScrollableFrontLayers(bool testChanged, bool &changed) const
{
    changed = false;
    LayerList nonScrollables;

    // Everything in front of the first non-scrollable from the back
    // should also be considered non-scrollable

    bool started = false;

    for (LayerList::const_iterator i = m_layerStack.begin(); i != m_layerStack.end(); ++i) {
        if ((*i)->isLayerDormant(this)) continue;
        if (!started && (*i)->isLayerScrollable(this)) {
            continue;
        }
        started = true;
        if ((*i)->isLayerOpaque()) {
            // You can't see anything behind an opaque layer!
            nonScrollables.clear();
        }
        nonScrollables.push_back(*i);
    }

    if (testChanged && nonScrollables != m_lastNonScrollableBackLayers) {
        m_lastNonScrollableBackLayers = nonScrollables;
        changed = true;
    }

    return nonScrollables;
}

ZoomLevel
View::getZoomConstraintLevel(ZoomLevel zoomLevel,
                             ZoomConstraint::RoundingDirection dir)
    const
{
    using namespace std::rel_ops;
    
    ZoomLevel candidate =
        RelativelyFineZoomConstraint().getNearestZoomLevel(zoomLevel, dir);

    for (auto i : m_layerStack) {

        if (i->supportsOtherZoomLevels() || !(i->getZoomConstraint())) {
            continue;
        }
        
        ZoomLevel thisLevel =
            i->getZoomConstraint()->getNearestZoomLevel(zoomLevel, dir);

        // Go for the block size that's furthest from the one
        // passed in.  Most of the time, that's what we want.
        if ((thisLevel > zoomLevel && thisLevel > candidate) ||
            (thisLevel < zoomLevel && thisLevel < candidate)) {
            candidate = thisLevel;
        }
    }

    return candidate;
}

int
View::countZoomLevels() const
{
    int n = 0;
    ZoomLevel min = ZoomConstraint().getMinZoomLevel();
    ZoomLevel max = ZoomConstraint().getMaxZoomLevel();
    ZoomLevel level = min;
    while (true) {
        ++n;
        if (level == max) {
            break;
        }
        level = getZoomConstraintLevel
            (level.incremented(), ZoomConstraint::RoundUp);
    }
//    SVCERR << "View::countZoomLevels: " << n << endl;
    return n;
}

ZoomLevel
View::getZoomLevelByIndex(int ix) const
{
    int n = 0;
    ZoomLevel min = ZoomConstraint().getMinZoomLevel();
    ZoomLevel max = ZoomConstraint().getMaxZoomLevel();
    ZoomLevel level = min;
    while (true) {
        if (n == ix) {
//            SVCERR << "View::getZoomLevelByIndex: " << ix << " -> " << level
//                 << endl;
            return level;
        }
        ++n;
        if (level == max) {
            break;
        }
        level = getZoomConstraintLevel
            (level.incremented(), ZoomConstraint::RoundUp);
    }
//    SVCERR << "View::getZoomLevelByIndex: " << ix << " -> " << max << " (max)"
//         << endl;
    return max;
}

int
View::getZoomLevelIndex(ZoomLevel z) const
{
    int n = 0;
    ZoomLevel min = ZoomConstraint().getMinZoomLevel();
    ZoomLevel max = ZoomConstraint().getMaxZoomLevel();
    ZoomLevel level = min;
    while (true) {
        if (z == level) {
//            SVCERR << "View::getZoomLevelIndex: " << z << " -> " << n
//                 << endl;
            return n;
        }
        ++n;
        if (level == max) {
            break;
        }
        level = getZoomConstraintLevel
            (level.incremented(), ZoomConstraint::RoundUp);
    }
//    SVCERR << "View::getZoomLevelIndex: " << z << " -> " << n << " (max)"
//         << endl;
    return n;
}

double
View::scaleSize(double size) const
{
    static double ratio = 0.0;

    if (ratio == 0.0) {
        double baseEm;
#ifdef Q_OS_MAC
        baseEm = 17.0;
#else
        baseEm = 15.0;
#endif
        double em = QFontMetrics(QFont()).height();
        ratio = em / baseEm;

        SVDEBUG << "View::scaleSize: ratio is " << ratio
                << " (em = " << em << ")" << endl;

        if (ratio < 1.0) {
            SVDEBUG << "View::scaleSize: rounding ratio up to 1.0" << endl;
            ratio = 1.0;
        }
    }

    return size * ratio;
}

int
View::scalePixelSize(int size) const
{
    double d = scaleSize(size);
    int i = int(d + 0.5);
    if (size != 0 && i == 0) i = 1;
    return i;
}

double
View::scalePenWidth(double width) const 
{
    if (width <= 0) { // zero-width pen, produce a scaled one-pixel pen
        width = 1;
    }
    double ratio = scaleSize(1.0);
    return width * sqrt(ratio);
}

QPen
View::scalePen(QPen pen) const 
{
    return QPen(pen.color(), scalePenWidth(pen.width()));
}

bool
View::areLayerColoursSignificant() const
{
    for (LayerList::const_iterator i = m_layerStack.begin(); i != m_layerStack.end(); ++i) {
        if ((*i)->getLayerColourSignificance() ==
            Layer::ColourHasMeaningfulValue) return true;
        if ((*i)->isLayerOpaque()) break;
    }
    return false;
}

bool
View::hasTopLayerTimeXAxis() const
{
    LayerList::const_iterator i = m_layerStack.end();
    if (i == m_layerStack.begin()) return false;
    --i;
    return (*i)->hasTimeXAxis();
}

void
View::zoom(bool in)
{
    ZoomLevel newZoomLevel = m_zoomLevel;

    if (in) {
        newZoomLevel = getZoomConstraintLevel(m_zoomLevel.decremented(),
                                              ZoomConstraint::RoundDown);
    } else {
        newZoomLevel = getZoomConstraintLevel(m_zoomLevel.incremented(),
                                              ZoomConstraint::RoundUp);
    }

    using namespace std::rel_ops;
    
    if (newZoomLevel != m_zoomLevel) {
        setZoomLevel(newZoomLevel);
    }
}

void
View::scroll(bool right, bool lots, bool e)
{
    sv_frame_t delta;
    if (lots) {
        delta = (getEndFrame() - getStartFrame()) / 2;
    } else {
        delta = (getEndFrame() - getStartFrame()) / 20;
    }
    if (right) delta = -delta;

#ifdef DEBUG_VIEW
    SVCERR << "View::scroll(" << right << ", " << lots << ", " << e << "): "
           << "delta = " << delta << ", m_centreFrame = " << m_centreFrame
           << endl;
#endif
    
    if (m_centreFrame < delta) {
        setCentreFrame(0, e);
    } else if (m_centreFrame - delta >= getModelsEndFrame()) {
        setCentreFrame(getModelsEndFrame(), e);
    } else {
        setCentreFrame(m_centreFrame - delta, e);
    }
}

void
View::cancelClicked()
{
    QPushButton *cancel = qobject_cast<QPushButton *>(sender());
    if (!cancel) return;

    Layer *layer = nullptr;
    
    for (ProgressMap::iterator i = m_progressBars.begin();
         i != m_progressBars.end(); ++i) {
        if (i->second.cancel == cancel) {
            layer = i->first;
            break;
        }
    }

    if (layer) {
        emit cancelButtonPressed(layer);
    }
}

void
View::checkProgress(ModelId modelId)
{
    if (!m_showProgress) {
#ifdef DEBUG_PROGRESS_STUFF
        SVCERR << "View[" << getId() << "]::checkProgress(" << modelId << "): "
               << "m_showProgress is off" << endl;
#endif
        return;
    }

#ifdef DEBUG_PROGRESS_STUFF
    SVCERR << "View[" << getId() << "]::checkProgress(" << modelId << ")" << endl;
#endif

    QSettings settings;
    settings.beginGroup("View");
    bool showCancelButton = settings.value("showcancelbuttons", true).toBool();
    settings.endGroup();
    
    int ph = height();
    bool found = false;

    if (m_alignmentProgressBar.bar) {
        ph -= m_alignmentProgressBar.bar->height();
    }

    for (ProgressMap::iterator i = m_progressBars.begin();
         i != m_progressBars.end(); ++i) {

        QProgressBar *pb = i->second.bar;
        QPushButton *cancel = i->second.cancel;

        if (i->first && i->first->getModel() == modelId) {

            found = true;
            
            // The timer is used to test for stalls.  If the progress
            // bar does not get updated for some length of time, the
            // timer prompts it to go back into "indeterminate" mode
            QTimer *timer = i->second.stallCheckTimer;

            int completion = i->first->getCompletion(this);
            QString error = i->first->getError(this);

#ifdef DEBUG_PROGRESS_STUFF
            SVCERR << "View[" << getId() << "]::checkProgress(" << modelId << "): "
                   << "found progress bar " << pb << " for layer at height " << ph
                   << ": completion = " << completion << endl;
#endif

            if (error != "" && error != m_lastError) {
                QMessageBox::critical(this, tr("Layer rendering error"), error);
                m_lastError = error;
            }

            if (completion > 0) {
                pb->setMaximum(100); // was 0, for indeterminate start
            }

            if (completion < 100 &&
                ModelById::isa<RangeSummarisableTimeValueModel>(modelId)) {
                update(); // ensure duration &c gets updated
            }

            if (completion >= 100) {

                pb->hide();
                cancel->hide();
                timer->stop();

            } else if (i->first->isLayerDormant(this)) {

                // A dormant (invisible) layer can still be busy
                // generating, but we don't usually want to indicate
                // it because it probably means it's a duplicate of a
                // visible layer
#ifdef DEBUG_PROGRESS_STUFF
                SVCERR << "View[" << getId() << "]::checkProgress("
                       << modelId << "): layer is dormant" << endl;
#endif
                pb->hide();
                cancel->hide();
                timer->stop();
                
            } else {

                if (!pb->isVisible()) {
                    i->second.lastStallCheckValue = 0;
                    timer->setInterval(2000);
                    timer->start();
                }

                if (showCancelButton) {
                
                    int scaled20 = scalePixelSize(20);

                    cancel->move(0, ph - pb->height()/2 - scaled20/2);
                    cancel->show();

                    pb->setValue(completion);
                    pb->move(scaled20, ph - pb->height());

                } else {

                    cancel->hide();

                    pb->setValue(completion);
                    pb->move(0, ph - pb->height());
                }

                pb->show();
                pb->update();

                if (pb->isVisible()) {
                    ph -= pb->height();
                }
            }
        } else {
            if (pb->isVisible()) {
                ph -= pb->height();
            }
        }
    }

    if (!found) {
#ifdef DEBUG_PROGRESS_STUFF
        SVCERR << "View[" << getId() << "]::checkProgress(" << modelId << "): "
               << "failed to find layer for model in progress map"
               << endl;
#endif
    }
}

void
View::checkAlignmentProgress(ModelId modelId)
{
    if (!m_showProgress) {
#ifdef DEBUG_PROGRESS_STUFF
        SVCERR << "View[" << getId() << "]::checkAlignmentProgress(" << modelId << "): "
               << "m_showProgress is off" << endl;
#endif
        return;
    }

#ifdef DEBUG_PROGRESS_STUFF
    SVCERR << "View[" << getId() << "]::checkAlignmentProgress(" << modelId << ")" << endl;
#endif

    if (!m_alignmentProgressBar.alignedModel.isNone() &&
        modelId != m_alignmentProgressBar.alignedModel) {
#ifdef DEBUG_PROGRESS_STUFF
        SVCERR << "View[" << getId() << "]::checkAlignmentProgress(" << modelId << "): Different model (we're currently showing alignment progress for " << modelId << ", ignoring" << endl;
#endif
        return;
    }
    
    auto model = ModelById::get(modelId);
    if (!model) {
#ifdef DEBUG_PROGRESS_STUFF
        SVCERR << "View[" << getId() << "]::checkAlignmentProgress(" << modelId << "): Model gone" << endl;
#endif
        m_alignmentProgressBar.alignedModel = {};
        delete m_alignmentProgressBar.bar;
        m_alignmentProgressBar.bar = nullptr;
        return;
    }

    int completion = model->getAlignmentCompletion();

#ifdef DEBUG_PROGRESS_STUFF
    SVCERR << "View[" << getId() << "]::checkAlignmentProgress(" << modelId << "): Completion is " << completion << endl;
#endif

    int ph = height();

    if (completion >= 100) {
        m_alignmentProgressBar.alignedModel = {};
        delete m_alignmentProgressBar.bar;
        m_alignmentProgressBar.bar = nullptr;
        return;
    }

    QProgressBar *pb = m_alignmentProgressBar.bar;
    if (!pb) {
        pb = new QProgressBar(this);
        pb->setMinimum(0);
        pb->setMaximum(100);
        pb->setFixedWidth(80);
        pb->setTextVisible(false);
        m_alignmentProgressBar.alignedModel = modelId;
        m_alignmentProgressBar.bar = pb;
    }
        
    pb->setValue(completion);
    pb->move(0, ph - pb->height());
    pb->show();
    pb->update();
}

void
View::progressCheckStalledTimerElapsed()
{
    QObject *s = sender();
    QTimer *t = qobject_cast<QTimer *>(s);
    if (!t) return;

    for (ProgressMap::iterator i =  m_progressBars.begin();
         i != m_progressBars.end(); ++i) {

        if (i->second.stallCheckTimer == t) {

            int value = i->second.bar->value();

#ifdef DEBUG_PROGRESS_STUFF
            SVCERR << "View[" << getId() << "]::progressCheckStalledTimerElapsed for layer " << i->first << ": value is " << value << endl;
#endif
    
            if (value > 0 && value == i->second.lastStallCheckValue) {
                i->second.bar->setMaximum(0); // indeterminate
            }
            i->second.lastStallCheckValue = value;
            return;
        }
    }
}

int
View::getProgressBarWidth() const
{
    if (m_alignmentProgressBar.bar) {
        return m_alignmentProgressBar.bar->width();
    }
    
    for (ProgressMap::const_iterator i = m_progressBars.begin();
         i != m_progressBars.end(); ++i) {
        if (i->second.bar && i->second.bar->isVisible()) {
            return i->second.bar->width();
        }
    }

    return 0;
}

void
View::setPaintFont(QPainter &paint)
{
    int scaleFactor = 1;
    int dpratio = effectiveDevicePixelRatio();
    if (dpratio > 1) {
        QPaintDevice *dev = paint.device();
        if (dynamic_cast<QPixmap *>(dev) || dynamic_cast<QImage *>(dev)) {
            scaleFactor = dpratio;
        }
    }

    QFont font(paint.font());
    font.setPointSize(Preferences::getInstance()->getViewFontSize()
                      * scaleFactor);
    paint.setFont(font);
}

QRect
View::getPaintRect() const
{
    return rect();
}

void
View::paintEvent(QPaintEvent *e)
{
//    Profiler prof("View::paintEvent", false);

#ifdef DEBUG_VIEW_WIDGET_PAINT
    {
        sv_frame_t startFrame = getStartFrame();
        SVCERR << "View[" << getId() << "]::paintEvent: centre frame is " << m_centreFrame << " (start frame " << startFrame << ", height " << height() << ")" << endl;
    }
#endif

    if (m_layerStack.empty()) {
        QFrame::paintEvent(e);
        return;
    }

    // ensure our constraints are met
    m_zoomLevel = getZoomConstraintLevel
        (m_zoomLevel, ZoomConstraint::RoundNearest);

    // We have a cache, which retains the state of scrollable (back)
    // layers from one paint to the next, and a buffer, which we paint
    // onto before copying directly to the widget. Both are at scaled
    // resolution (e.g. 2x on a pixel-doubled display), whereas the
    // paint event always comes in at formal (1x) resolution.

    // If we touch the cache, we always leave it in a valid state
    // across its whole extent. When another method invalidates the
    // cache, it does so by setting m_cacheValid false, so if that
    // flag is true on entry, then the cache is valid across its whole
    // extent - although it may be valid for a different centre frame,
    // zoom level, or view size from those now in effect.

    // Our process goes:
    // 
    // 1. Check whether we have any scrollable (cacheable) layers.  If
    //    we don't, then invalidate and ignore the cache and go to
    //    step 5.  Otherwise:
    // 
    // 2. Check the cache, scroll as necessary, identify any area that
    //    needs to be refreshed (this might be the whole cache).
    //
    // 3. Paint to cache the area that needs to be refreshed, from the
    //    stack of scrollable layers.
    //
    // 4. Paint to buffer from cache: if there are no non-cached areas
    //    or selections and the cache has not scrolled, then paint the
    //    union of the area of cache that has changed and the area
    //    that the paint event reported as exposed; otherwise paint
    //    the whole.
    //
    // 5. Paint the exposed area to the buffer from the cache plus all
    //    the layers that haven't been cached, plus selections etc.
    //
    // 6. Paint the exposed rect from the buffer.
    //
    // Note that all rects except the target for the final step are at
    // cache (scaled, 2x as applicable) resolution.

    int dpratio = effectiveDevicePixelRatio();

    QRect requestedPaintArea(scaledRect(rect(), dpratio));
    if (e) {
        // cut down to only the area actually exposed
        requestedPaintArea &= scaledRect(e->rect(), dpratio);
    }

    // If not all layers are scrollable, but some of the back layers
    // are, we should store only those in the cache.

    bool layersChanged = false;
    LayerList scrollables = getScrollableBackLayers(true, layersChanged);
    LayerList nonScrollables = getNonScrollableFrontLayers(true, layersChanged);

#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR << "View[" << getId() << "]::paintEvent: have " << scrollables.size()
              << " scrollable back layers and " << nonScrollables.size()
              << " non-scrollable front layers" << endl;
#endif

    if (layersChanged || scrollables.empty()) {
        m_cacheValid = false;
    }

    QRect wholeArea(scaledRect(rect(), dpratio));
    QSize wholeSize(scaledSize(size(), dpratio));

    if (!m_buffer || wholeSize != m_buffer->size()) {
        delete m_buffer;
        m_buffer = new QPixmap(wholeSize);
    }

    bool shouldUseCache = false;
    bool shouldRepaintCache = false;
    QRect cacheAreaToRepaint;
    
    static HitCount count("View cache");

    if (!scrollables.empty()) {

        shouldUseCache = true;
        shouldRepaintCache = true;
        cacheAreaToRepaint = wholeArea;

#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVCERR << "View[" << getId() << "]: cache " << m_cache << ", cache zoom "
                  << m_cacheZoomLevel << ", zoom " << m_zoomLevel << endl;
#endif

        using namespace std::rel_ops;
    
        if (!m_cacheValid ||
            !m_cache ||
            m_cacheZoomLevel != m_zoomLevel ||
            m_cache->size() != wholeSize) {

            // cache is not valid at all

            if (requestedPaintArea.width() < wholeSize.width() / 10) {

                m_cacheValid = false;
                shouldUseCache = false;
                shouldRepaintCache = false;

#ifdef DEBUG_VIEW_WIDGET_PAINT
                SVCERR << "View[" << getId() << "]::paintEvent: cache is invalid but only small area requested, will repaint directly instead" << endl;
#endif
            } else {

                if (!m_cache ||
                    m_cache->size() != wholeSize) {
                    delete m_cache;
                    m_cache = new QPixmap(wholeSize);
                }

#ifdef DEBUG_VIEW_WIDGET_PAINT
                SVCERR << "View[" << getId() << "]::paintEvent: cache is invalid, will repaint whole" << endl;
#endif
            }

            count.miss();
            
        } else if (m_cacheCentreFrame != m_centreFrame) {

#ifdef DEBUG_VIEW_WIDGET_PAINT
            SVCERR << "View[" << getId() << "]::paintEvent: cache centre frame is " << m_cacheCentreFrame << endl;
#endif

            int dx = dpratio * (getXForFrame(m_cacheCentreFrame) -
                                getXForFrame(m_centreFrame));

            if (dx > -m_cache->width() && dx < m_cache->width()) {

                m_cache->scroll(dx, 0, m_cache->rect(), nullptr);

                if (dx < 0) {
                    cacheAreaToRepaint = 
                        QRect(m_cache->width() + dx, 0, -dx, m_cache->height());
                } else {
                    cacheAreaToRepaint = 
                        QRect(0, 0, dx, m_cache->height());
                }

                count.partial();

#ifdef DEBUG_VIEW_WIDGET_PAINT
                SVCERR << "View[" << getId() << "]::paintEvent: scrolled cache by " << dx << endl;
#endif
            } else {
                count.miss();
#ifdef DEBUG_VIEW_WIDGET_PAINT
                SVCERR << "View[" << getId() << "]::paintEvent: scrolling too far" << endl;
#endif
            }

        } else {
#ifdef DEBUG_VIEW_WIDGET_PAINT
            SVCERR << "View[" << getId() << "]::paintEvent: cache is good" << endl;
#endif
            count.hit();
            shouldRepaintCache = false;
        }
    }

#ifdef DEBUG_VIEW_WIDGET_PAINT
    SVCERR << "View[" << getId() << "]::paintEvent: m_cacheValid = " << m_cacheValid << ", shouldUseCache = " << shouldUseCache << ", shouldRepaintCache = " << shouldRepaintCache << ", cacheAreaToRepaint = " << cacheAreaToRepaint.x() << "," << cacheAreaToRepaint.y() << " " << cacheAreaToRepaint.width() << "x" << cacheAreaToRepaint.height() << endl;
#endif

    if (shouldRepaintCache && !shouldUseCache) {
        // If we are repainting the cache, then we paint the
        // scrollables only to the cache, not to the buffer. So if
        // shouldUseCache is also false, then the scrollables can't
        // appear because they will only be on the cache
        throw std::logic_error("ERROR: shouldRepaintCache is true, but shouldUseCache is false: this can't lead to the correct result");
    }

    // Create the ViewProxy for geometry provision, using the
    // device-pixel ratio for pixel-doubled hi-dpi rendering as
    // appropriate.

    ViewProxy proxy(this, dpratio);

    // Some layers may need an aligning proxy. If a layer's model has
    // a source model that is the reference model for the aligning
    // model, and the layer is tagged as to be aligned, then we might
    // use an aligning proxy. Note this is actually made use of only
    // if m_useAligningProxy is true further down.
    
    ModelId alignmentModelId;
    ModelId alignmentReferenceId;
    auto aligningModel = ModelById::get(getAligningModel());
    if (aligningModel) {
        alignmentModelId = aligningModel->getAlignment();
        alignmentReferenceId = aligningModel->getAlignmentReference();
#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVCERR << "alignmentModelId = " << alignmentModelId << " (reference = " << alignmentReferenceId << ")" << endl;
#endif
    } else {
#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVCERR << "no aligningModel" << endl;
#endif
    }
    ViewProxy aligningProxy(this, dpratio, alignmentModelId);
    
    // Scrollable (cacheable) items first. If we are repainting the
    // cache, then we paint these to the cache; otherwise straight to
    // the buffer.
    QRect areaToPaint;
    QPainter paint;

    if (shouldRepaintCache) {
        paint.begin(m_cache);
        areaToPaint = cacheAreaToRepaint;
    } else {
        paint.begin(m_buffer);
        areaToPaint = requestedPaintArea;
    }

    setPaintFont(paint);
    paint.setClipRect(areaToPaint);

    paint.setPen(getBackground());
    paint.setBrush(getBackground());
    paint.drawRect(areaToPaint);

    paint.setPen(getForeground());
    paint.setBrush(Qt::NoBrush);
        
    for (LayerList::iterator i = scrollables.begin();
         i != scrollables.end(); ++i) {

        paint.setRenderHint(QPainter::Antialiasing, false);
        paint.save();

        Layer *layer = *i;
        
        bool useAligningProxy = false;
        if (m_useAligningProxy) {
            if (layer->getModel() == alignmentReferenceId ||
                layer->getSourceModel() == alignmentReferenceId) {
                useAligningProxy = true;
            }
        }

#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVCERR << "Painting scrollable layer " << layer << " (model " << layer->getModel() << ", source model " << layer->getSourceModel() << ") with shouldRepaintCache = " << shouldRepaintCache << ", useAligningProxy = " << useAligningProxy << ", dpratio = " << dpratio << ", areaToPaint = " << areaToPaint.x() << "," << areaToPaint.y() << " " << areaToPaint.width() << "x" << areaToPaint.height() << endl;
#endif
        
        layer->paint(useAligningProxy ? &aligningProxy : &proxy,
                     paint, areaToPaint);

        paint.restore();
    }

    paint.end();

    if (shouldRepaintCache) {
        // and now we have
        m_cacheValid = true;
        m_cacheCentreFrame = m_centreFrame;
        m_cacheZoomLevel = m_zoomLevel;
    }

    if (shouldUseCache) {
        paint.begin(m_buffer);
        paint.drawPixmap(requestedPaintArea, *m_cache, requestedPaintArea);
        paint.end();
    }

    // Now non-cacheable items.

    paint.begin(m_buffer);
    paint.setClipRect(requestedPaintArea);
    setPaintFont(paint);
    if (scrollables.empty()) {
        paint.setPen(getBackground());
        paint.setBrush(getBackground());
        paint.drawRect(requestedPaintArea);
    }
        
    paint.setPen(getForeground());
    paint.setBrush(Qt::NoBrush);
        
    for (LayerList::iterator i = nonScrollables.begin(); 
         i != nonScrollables.end(); ++i) {
        
        Layer *layer = *i;
        
        bool useAligningProxy = false;
        if (m_useAligningProxy) {
            if (layer->getModel() == alignmentReferenceId ||
                layer->getSourceModel() == alignmentReferenceId) {
                useAligningProxy = true;
            }
        }

#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVCERR << "Painting non-scrollable layer " << layer << " (model " << layer->getModel() << ", source model " << layer->getSourceModel() << ") with shouldRepaintCache = " << shouldRepaintCache << ", useAligningProxy = " << useAligningProxy << ", dpratio = " << dpratio << ", requestedPaintArea = " << requestedPaintArea.x() << "," << requestedPaintArea.y() << " " << requestedPaintArea.width() << "x" << requestedPaintArea.height() << endl;
#endif

        layer->paint(useAligningProxy ? &aligningProxy : &proxy,
                     paint, requestedPaintArea);
    }
        
    paint.end();

    // Now paint to widget from buffer: target rects from here on,
    // unlike all the preceding, are at formal (1x) resolution

    paint.begin(this);
    setPaintFont(paint);
    if (e) paint.setClipRect(e->rect());

    QRect finalPaintRect = e ? e->rect() : rect();
    paint.drawPixmap(finalPaintRect, *m_buffer, 
                     scaledRect(finalPaintRect, dpratio));

    drawSelections(paint);
    drawPlayPointer(paint);

    paint.end();

    QFrame::paintEvent(e);
}

void
View::drawSelections(QPainter &paint)
{
    if (!hasTopLayerTimeXAxis()) return;

    MultiSelection::SelectionList selections;

    if (m_manager) {
        selections = m_manager->getSelections();
        if (m_manager->haveInProgressSelection()) {
            bool exclusive;
            Selection inProgressSelection =
                m_manager->getInProgressSelection(exclusive);
            if (exclusive) selections.clear();
            selections.insert(inProgressSelection);
        }
    }

    paint.save();

    bool translucent = !areLayerColoursSignificant();

    if (translucent) {
        paint.setBrush(QColor(150, 150, 255, 80));
    } else {
        paint.setBrush(Qt::NoBrush);
    }

    sv_samplerate_t sampleRate = getModelsSampleRate();

    QPoint localPos;
    sv_frame_t illuminateFrame = -1;
    bool closeToLeft, closeToRight;

    if (shouldIlluminateLocalSelection(localPos, closeToLeft, closeToRight)) {
        illuminateFrame = getFrameForX(localPos.x());
    }

    const QFontMetrics &metrics = paint.fontMetrics();

    for (MultiSelection::SelectionList::iterator i = selections.begin();
         i != selections.end(); ++i) {

        int p0 = getXForFrame(alignFromReference(i->getStartFrame()));
        int p1 = getXForFrame(alignFromReference(i->getEndFrame()));

        if (p1 < 0 || p0 > width()) continue;

#ifdef DEBUG_VIEW_WIDGET_PAINT
        SVDEBUG << "View::drawSelections: " << p0 << ",-1 [" << (p1-p0) << "x" << (height()+1) << "]" << endl;
#endif

        bool illuminateThis =
            (illuminateFrame >= 0 && i->contains(illuminateFrame));

        double h = height();
        double penWidth = scalePenWidth(1.0);
        double half = penWidth/2.0;

        paint.setPen(QPen(QColor(150, 150, 255), penWidth));

        if (translucent && shouldLabelSelections()) {
            paint.drawRect(QRectF(p0, -penWidth, p1 - p0, h + 2*penWidth));
        } else {
            // Make the top & bottom lines of the box visible if we
            // are lacking some of the other visual cues.  There's no
            // particular logic to this, it's just a question of what
            // I happen to think looks nice.
            paint.drawRect(QRectF(p0, half, p1 - p0, h - penWidth));
        }

        if (illuminateThis) {
            paint.save();
            penWidth = scalePenWidth(2.0);
            half = penWidth/2.0;
            paint.setPen(QPen(getForeground(), penWidth));
            if (closeToLeft) {
                paint.drawLine(QLineF(p0, half, p1, half));
                paint.drawLine(QLineF(p0, half, p0, h - half));
                paint.drawLine(QLineF(p0, h - half, p1, h - half));
            } else if (closeToRight) {
                paint.drawLine(QLineF(p0, half, p1, half));
                paint.drawLine(QLineF(p1, half, p1, h - half));
                paint.drawLine(QLineF(p0, h - half, p1, h - half));
            } else {
                paint.setBrush(Qt::NoBrush);
                paint.drawRect(QRectF(p0, half, p1 - p0, h - penWidth));
            }
            paint.restore();
        }

        if (sampleRate && shouldLabelSelections() && m_manager &&
            m_manager->shouldShowSelectionExtents()) {
            
            QString startText = QString("%1 / %2")
                .arg(QString::fromStdString
                     (RealTime::frame2RealTime
                      (i->getStartFrame(), sampleRate).toText(true)))
                .arg(i->getStartFrame());
            
            QString endText = QString(" %1 / %2")
                .arg(QString::fromStdString
                     (RealTime::frame2RealTime
                      (i->getEndFrame(), sampleRate).toText(true)))
                .arg(i->getEndFrame());
            
            QString durationText = QString("(%1 / %2) ")
                .arg(QString::fromStdString
                     (RealTime::frame2RealTime
                      (i->getEndFrame() - i->getStartFrame(), sampleRate)
                      .toText(true)))
                .arg(i->getEndFrame() - i->getStartFrame());
            
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

            int sw = metrics.width(startText),
                ew = metrics.width(endText),
                dw = metrics.width(durationText);

            int sy = metrics.ascent() + metrics.height() + 4;
            int ey = sy;
            int dy = sy + metrics.height();

            int sx = p0 + 2;
            int ex = sx;
            int dx = sx;

            bool durationBothEnds = true;

            if (sw + ew > (p1 - p0)) {
                ey += metrics.height();
                dy += metrics.height();
                durationBothEnds = false;
            }

            if (ew < (p1 - p0)) {
                ex = p1 - 2 - ew;
            }

            if (dw < (p1 - p0)) {
                dx = p1 - 2 - dw;
            }

            PaintAssistant::drawVisibleText(this, paint, sx, sy, startText,
                                            PaintAssistant::OutlinedText);
            PaintAssistant::drawVisibleText(this, paint, ex, ey, endText,
                                            PaintAssistant::OutlinedText);
            PaintAssistant::drawVisibleText(this, paint, dx, dy, durationText,
                                            PaintAssistant::OutlinedText);
            if (durationBothEnds) {
                PaintAssistant::drawVisibleText(this, paint, sx, dy, durationText,
                                                PaintAssistant::OutlinedText);
            }
        }
    }

    paint.restore();
}

void
View::drawPlayPointer(QPainter &paint)
{
    bool showPlayPointer = true;

    if (m_followPlay == PlaybackScrollContinuous) {
        showPlayPointer = false;
    } else if (m_playPointerFrame <= getStartFrame() ||
               m_playPointerFrame >= getEndFrame()) {
        showPlayPointer = false;
    } else if (m_manager && !m_manager->isPlaying()) {
        if (m_playPointerFrame == getCentreFrame() &&
            m_manager->shouldShowCentreLine() &&
            m_followPlay != PlaybackIgnore) {
            // Don't show the play pointer when it is redundant with
            // the centre line
            showPlayPointer = false;
        }
    }
    
    if (showPlayPointer) {

        int playx = getXForFrame(m_playPointerFrame);
        
        paint.setPen(getForeground());
        paint.drawLine(playx - 1, 0, playx - 1, height() - 1);
        paint.drawLine(playx + 1, 0, playx + 1, height() - 1);
        paint.drawPoint(playx, 0);
        paint.drawPoint(playx, height() - 1);
        paint.setPen(getBackground());
        paint.drawLine(playx, 1, playx, height() - 2);
    }
}

void
View::drawMeasurementRect(QPainter &paint, const Layer *topLayer, QRect r,
                          bool focus) const
{
//    SVDEBUG << "View::drawMeasurementRect(" << r.x() << "," << r.y() << " "
//              << r.width() << "x" << r.height() << ")" << endl;

    if (r.x() + r.width() < 0 || r.x() >= width()) return;

    if (r.width() != 0 || r.height() != 0) {
        paint.save();
        if (focus) {
            paint.setPen(Qt::NoPen);
            QColor brushColour(Qt::black);
            brushColour.setAlpha(hasLightBackground() ? 15 : 40);
            paint.setBrush(brushColour);
            if (r.x() > 0) {
                paint.drawRect(0, 0, r.x(), height());
            }
            if (r.x() + r.width() < width()) {
                paint.drawRect(r.x() + r.width(), 0, width()-r.x()-r.width(), height());
            }
            if (r.y() > 0) {
                paint.drawRect(r.x(), 0, r.width(), r.y());
            }
            if (r.y() + r.height() < height()) {
                paint.drawRect(r.x(), r.y() + r.height(), r.width(), height()-r.y()-r.height());
            }
            paint.setBrush(Qt::NoBrush);
        }
        paint.setPen(Qt::green);
        paint.drawRect(r);
        paint.restore();
    } else {
        paint.save();
        paint.setPen(Qt::green);
        paint.drawPoint(r.x(), r.y());
        paint.restore();
    }

    if (!focus) return;

    paint.save();
    QFont fn = paint.font();
    if (fn.pointSize() > 8) {
        fn.setPointSize(fn.pointSize() - 1);
        paint.setFont(fn);
    }

    int fontHeight = paint.fontMetrics().height();
    int fontAscent = paint.fontMetrics().ascent();

    double v0, v1;
    QString u0, u1;
    bool b0 = false, b1 = false;

    QString axs, ays, bxs, bys, dxs, dys;

    int axx, axy, bxx, bxy, dxx, dxy;
    int aw = 0, bw = 0, dw = 0;
    
    int labelCount = 0;

    // top-left point, x-coord

    if ((b0 = topLayer->getXScaleValue(this, r.x(), v0, u0))) {
        axs = QString("%1 %2").arg(v0).arg(u0);
        if (u0 == "Hz" && Pitch::isFrequencyInMidiRange(v0)) {
            axs = QString("%1 (%2)").arg(axs)
                .arg(Pitch::getPitchLabelForFrequency(v0));
        }
        aw = paint.fontMetrics().width(axs);
        ++labelCount;
    }

    // bottom-right point, x-coord
        
    if (r.width() > 0) {
        if ((b1 = topLayer->getXScaleValue(this, r.x() + r.width(), v1, u1))) {
            bxs = QString("%1 %2").arg(v1).arg(u1);
            if (u1 == "Hz" && Pitch::isFrequencyInMidiRange(v1)) {
                bxs = QString("%1 (%2)").arg(bxs)
                    .arg(Pitch::getPitchLabelForFrequency(v1));
            }
            bw = paint.fontMetrics().width(bxs);
        }
    }

    // dimension, width
        
    if (b0 && b1 && v1 != v0 && u0 == u1) {
        dxs = QString("[%1 %2]").arg(fabs(v1 - v0)).arg(u1);
        dw = paint.fontMetrics().width(dxs);
    }
    
    b0 = false;
    b1 = false;

    // top-left point, y-coord

    if ((b0 = topLayer->getYScaleValue(this, r.y(), v0, u0))) {
        ays = QString("%1 %2").arg(v0).arg(u0);
        if (u0 == "Hz" && Pitch::isFrequencyInMidiRange(v0)) {
            ays = QString("%1 (%2)").arg(ays)
                .arg(Pitch::getPitchLabelForFrequency(v0));
        }
        aw = std::max(aw, paint.fontMetrics().width(ays));
        ++labelCount;
    }

    // bottom-right point, y-coord

    if (r.height() > 0) {
        if ((b1 = topLayer->getYScaleValue(this, r.y() + r.height(), v1, u1))) {
            bys = QString("%1 %2").arg(v1).arg(u1);
            if (u1 == "Hz" && Pitch::isFrequencyInMidiRange(v1)) {
                bys = QString("%1 (%2)").arg(bys)
                    .arg(Pitch::getPitchLabelForFrequency(v1));
            }
            bw = std::max(bw, paint.fontMetrics().width(bys));
        }
    }

    bool bd = false;
    double dy = 0.f;
    QString du;

    // dimension, height
        
    if ((bd = topLayer->getYScaleDifference(this, r.y(), r.y() + r.height(),
                                            dy, du)) &&
        dy != 0) {
        if (du != "") {
            if (du == "Hz") {
                int semis;
                double cents;
                semis = Pitch::getPitchForFrequencyDifference(v0, v1, &cents);
                dys = QString("[%1 %2 (%3)]")
                    .arg(dy).arg(du)
                    .arg(Pitch::getLabelForPitchRange(semis, cents));
            } else {
                dys = QString("[%1 %2]").arg(dy).arg(du);
            }
        } else {
            dys = QString("[%1]").arg(dy);
        }
        dw = std::max(dw, paint.fontMetrics().width(dys));
    }

    int mw = r.width();
    int mh = r.height();

    bool edgeLabelsInside = false;
    bool sizeLabelsInside = false;

    if (mw < std::max(aw, std::max(bw, dw)) + 4) {
        // defaults stand
    } else if (mw < aw + bw + 4) {
        if (mh > fontHeight * labelCount * 3 + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        } else if (mh > fontHeight * labelCount * 2 + 4) {
            edgeLabelsInside = true;
        }
    } else if (mw < aw + bw + dw + 4) {
        if (mh > fontHeight * labelCount * 3 + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        } else if (mh > fontHeight * labelCount + 4) {
            edgeLabelsInside = true;
        }
    } else {
        if (mh > fontHeight * labelCount + 4) {
            edgeLabelsInside = true;
            sizeLabelsInside = true;
        }
    }

    if (edgeLabelsInside) {

        axx = r.x() + 2;
        axy = r.y() + fontAscent + 2;

        bxx = r.x() + r.width() - bw - 2;
        bxy = r.y() + r.height() - (labelCount-1) * fontHeight - 2;

    } else {

        axx = r.x() - aw - 2;
        axy = r.y() + fontAscent;
        
        bxx = r.x() + r.width() + 2;
        bxy = r.y() + r.height() - (labelCount-1) * fontHeight;
    }

    dxx = r.width()/2 + r.x() - dw/2;

    if (sizeLabelsInside) {

        dxy = r.height()/2 + r.y() - (labelCount * fontHeight)/2 + fontAscent;

    } else {

        dxy = r.y() + r.height() + fontAscent + 2;
    }
    
    if (axs != "") {
        PaintAssistant::drawVisibleText(this, paint, axx, axy, axs, PaintAssistant::OutlinedText);
        axy += fontHeight;
    }
    
    if (ays != "") {
        PaintAssistant::drawVisibleText(this, paint, axx, axy, ays, PaintAssistant::OutlinedText);
        axy += fontHeight;
    }

    if (bxs != "") {
        PaintAssistant::drawVisibleText(this, paint, bxx, bxy, bxs, PaintAssistant::OutlinedText);
        bxy += fontHeight;
    }

    if (bys != "") {
        PaintAssistant::drawVisibleText(this, paint, bxx, bxy, bys, PaintAssistant::OutlinedText);
        bxy += fontHeight;
    }

    if (dxs != "") {
        PaintAssistant::drawVisibleText(this, paint, dxx, dxy, dxs, PaintAssistant::OutlinedText);
        dxy += fontHeight;
    }

    if (dys != "") {
        PaintAssistant::drawVisibleText(this, paint, dxx, dxy, dys, PaintAssistant::OutlinedText);
        dxy += fontHeight;
    }

    paint.restore();
}

bool
View::render(QPainter &paint, int xorigin, sv_frame_t f0, sv_frame_t f1)
{
    int x0 = int(round(m_zoomLevel.framesToPixels(double(f0))));
    int x1 = int(round(m_zoomLevel.framesToPixels(double(f1))));

    int w = x1 - x0;

    sv_frame_t origCentreFrame = m_centreFrame;

    bool someLayersIncomplete = false;

    for (LayerList::iterator i = m_layerStack.begin();
         i != m_layerStack.end(); ++i) {

        int c = (*i)->getCompletion(this);
        if (c < 100) {
            someLayersIncomplete = true;
            break;
        }
    }

    if (someLayersIncomplete) {

        QProgressDialog progress(tr("Waiting for layers to be ready..."),
                                 tr("Cancel"), 0, 100, this);
        
        int layerCompletion = 0;

        while (layerCompletion < 100) {

            for (LayerList::iterator i = m_layerStack.begin();
                 i != m_layerStack.end(); ++i) {

                int c = (*i)->getCompletion(this);
                if (i == m_layerStack.begin() || c < layerCompletion) {
                    layerCompletion = c;
                }
            }

            if (layerCompletion >= 100) break;

            progress.setValue(layerCompletion);
            qApp->processEvents();
            if (progress.wasCanceled()) {
                update();
                return false;
            }

            usleep(50000);
        }
    }

    QProgressDialog progress(tr("Rendering image..."),
                             tr("Cancel"), 0, w / width(), this);

    for (int x = 0; x < w; x += width()) {

        progress.setValue(x / width());
        qApp->processEvents();
        if (progress.wasCanceled()) {
            m_centreFrame = origCentreFrame;
            update();
            return false;
        }

        m_centreFrame = f0 + sv_frame_t(round(m_zoomLevel.pixelsToFrames
                                              (x + width()/2)));
        
        QRect chunk(0, 0, width(), height());

        paint.setPen(getBackground());
        paint.setBrush(getBackground());

        paint.drawRect(QRect(xorigin + x, 0, width(), height()));

        paint.setPen(getForeground());
        paint.setBrush(Qt::NoBrush);

        for (LayerList::iterator i = m_layerStack.begin();
             i != m_layerStack.end(); ++i) {
            if (!((*i)->isLayerDormant(this))){

                paint.setRenderHint(QPainter::Antialiasing, false);

                paint.save();
                paint.translate(xorigin + x, 0);

                SVCERR << "Centre frame now: " << m_centreFrame << " drawing to " << chunk.x() + x + xorigin << ", " << chunk.width() << endl;

                (*i)->setSynchronousPainting(true);

                (*i)->paint(this, paint, chunk);

                (*i)->setSynchronousPainting(false);

                paint.restore();
            }
        }
    }

    m_centreFrame = origCentreFrame;
    update();
    return true;
}

QImage *
View::renderToNewImage()
{
    sv_frame_t f0 = getModelsStartFrame();
    sv_frame_t f1 = getModelsEndFrame();

    return renderPartToNewImage(f0, f1);
}

QImage *
View::renderPartToNewImage(sv_frame_t f0, sv_frame_t f1)
{
    int x0 = int(round(getZoomLevel().framesToPixels(double(f0))));
    int x1 = int(round(getZoomLevel().framesToPixels(double(f1))));
    
    QImage *image = new QImage(x1 - x0, height(), QImage::Format_RGB32);

    QPainter *paint = new QPainter(image);
    if (!render(*paint, 0, f0, f1)) {
        delete paint;
        delete image;
        return nullptr;
    } else {
        delete paint;
        return image;
    }
}

QSize
View::getRenderedImageSize()
{
    sv_frame_t f0 = getModelsStartFrame();
    sv_frame_t f1 = getModelsEndFrame();

    return getRenderedPartImageSize(f0, f1);
}
    
QSize
View::getRenderedPartImageSize(sv_frame_t f0, sv_frame_t f1)
{
    int x0 = int(round(getZoomLevel().framesToPixels(double(f0))));
    int x1 = int(round(getZoomLevel().framesToPixels(double(f1))));

    return QSize(x1 - x0, height());
}

bool
View::renderToSvgFile(QString filename)
{
    sv_frame_t f0 = getModelsStartFrame();
    sv_frame_t f1 = getModelsEndFrame();

    return renderPartToSvgFile(filename, f0, f1);
}

bool
View::renderPartToSvgFile(QString filename, sv_frame_t f0, sv_frame_t f1)
{
    int x0 = int(round(getZoomLevel().framesToPixels(double(f0))));
    int x1 = int(round(getZoomLevel().framesToPixels(double(f1))));

    QSvgGenerator generator;
    generator.setFileName(filename);
    generator.setSize(QSize(x1 - x0, height()));
    generator.setViewBox(QRect(0, 0, x1 - x0, height()));
    generator.setTitle(tr("Exported image from %1")
                       .arg(QApplication::applicationName()));
    
    QPainter paint;
    paint.begin(&generator);
    bool result = render(paint, 0, f0, f1);
    paint.end();
    return result;
}

void
View::toXml(QTextStream &stream,
            QString indent, QString extraAttributes) const
{
    stream << indent;

    int classicZoomValue, deepZoomValue;

    if (m_zoomLevel.zone == ZoomLevel::FramesPerPixel) {
        classicZoomValue = m_zoomLevel.level;
        deepZoomValue = 1;
    } else {
        classicZoomValue = 1;
        deepZoomValue = m_zoomLevel.level;
    }

    stream << QString("<view "
                      "centre=\"%1\" "
                      "zoom=\"%2\" "
                      "deepZoom=\"%3\" "
                      "followPan=\"%4\" "
                      "followZoom=\"%5\" "
                      "tracking=\"%6\" "
                      " %7>\n")
        .arg(m_centreFrame)
        .arg(classicZoomValue)
        .arg(deepZoomValue)
        .arg(m_followPan)
        .arg(m_followZoom)
        .arg(m_followPlay == PlaybackScrollContinuous ? "scroll" :
             m_followPlay == PlaybackScrollPageWithCentre ? "page" :
             m_followPlay == PlaybackScrollPage ? "daw" :
             "ignore")
        .arg(extraAttributes);

    for (int i = 0; i < (int)m_fixedOrderLayers.size(); ++i) {
        bool visible = !m_fixedOrderLayers[i]->isLayerDormant(this);
        m_fixedOrderLayers[i]->toBriefXml(stream, indent + "  ",
                                          QString("visible=\"%1\"")
                                          .arg(visible ? "true" : "false"));
    }

    stream << indent + "</view>\n";
}

ViewPropertyContainer::ViewPropertyContainer(View *v) :
    m_v(v)
{
//    SVCERR << "ViewPropertyContainer: " << getId() << " is owned by View " << v << endl;
    connect(m_v, SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this, SIGNAL(propertyChanged(PropertyContainer::PropertyName)));
}

ViewPropertyContainer::~ViewPropertyContainer()
{
}
