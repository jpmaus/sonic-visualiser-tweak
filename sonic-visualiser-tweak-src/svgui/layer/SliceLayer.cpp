/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "SliceLayer.h"

#include "view/View.h"
#include "base/AudioLevel.h"
#include "base/RangeMapper.h"
#include "base/RealTime.h"
#include "ColourMapper.h"
#include "ColourDatabase.h"

#include "PaintAssistant.h"

#include "base/Profiler.h"

#include <QPainter>
#include <QPainterPath>
#include <QTextStream>


SliceLayer::SliceLayer() :
    m_binAlignment(BinsSpanScalePoints),
    m_colourMap(int(ColourMapper::Ice)),
    m_colourInverted(false),
    m_energyScale(dBScale),
    m_samplingMode(SampleMean),
    m_plotStyle(PlotLines),
    m_binScale(LinearBins),
    m_normalize(false),
    m_threshold(0.0),
    m_initialThreshold(0.0),
    m_gain(1.0),
    m_minbin(0),
    m_maxbin(0),
    m_currentf0(0),
    m_currentf1(0)
{
}

SliceLayer::~SliceLayer()
{

}

void
SliceLayer::setSliceableModel(ModelId modelId)
{
    auto newModel = ModelById::getAs<DenseThreeDimensionalModel>(modelId);
    
    if (!modelId.isNone() && !newModel) {
        throw std::logic_error("Not a DenseThreeDimensionalModel");
    }

    if (m_sliceableModel == modelId) return;
    m_sliceableModel = modelId;

    if (newModel) {
        connectSignals(m_sliceableModel);

        if (m_minbin == 0 && m_maxbin == 0) {
            m_minbin = 0;
            m_maxbin = newModel->getHeight();
        }
    }
    
    emit modelReplaced();
    emit layerParametersChanged();
}

void
SliceLayer::sliceableModelReplaced(ModelId orig, ModelId replacement)
{
    SVDEBUG << "SliceLayer::sliceableModelReplaced(" << orig << ", " << replacement << ")" << endl;

    if (orig == m_sliceableModel) {
        setSliceableModel(replacement);
    }
}

QString
SliceLayer::getFeatureDescription(LayerGeometryProvider *v, QPoint &p) const
{
    int minbin, maxbin, range;
    return getFeatureDescriptionAux(v, p, true, minbin, maxbin, range);
}

QString
SliceLayer::getFeatureDescriptionAux(LayerGeometryProvider *v, QPoint &p,
                                     bool includeBinDescription,
                                     int &minbin, int &maxbin, int &range) const
{
    minbin = 0;
    maxbin = 0;
    
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return "";

    if (m_binAlignment == BinsSpanScalePoints) {
        minbin = int(getBinForX(v, p.x()));
        maxbin = int(getBinForX(v, p.x() + 1));
    } else {
        minbin = int(getBinForX(v, p.x()) + 0.5);
        maxbin = int(getBinForX(v, p.x() + 1) + 0.5);
    }        

    int mh = sliceableModel->getHeight();
    if (minbin >= mh) minbin = mh - 1;
    if (maxbin >= mh) maxbin = mh - 1;
    if (minbin < 0) minbin = 0;
    if (maxbin < 0) maxbin = 0;
    
    sv_samplerate_t sampleRate = sliceableModel->getSampleRate();

    sv_frame_t f0 = m_currentf0;
    sv_frame_t f1 = m_currentf1;

    RealTime rt0 = RealTime::frame2RealTime(f0, sampleRate);
    RealTime rt1 = RealTime::frame2RealTime(f1, sampleRate);
    
    range = int(f1 - f0 + 1);

    QString rtrangestr = QString("%1 s").arg((rt1 - rt0).toText().c_str());

    if (includeBinDescription) {

        int i0 = minbin - m_minbin;
        int i1 = maxbin - m_minbin;
        
        float minvalue = 0.0;
        if (in_range_for(m_values, i0)) minvalue = m_values[i0];

        float maxvalue = minvalue;
        if (in_range_for(m_values, i1)) maxvalue = m_values[i1];

        if (minvalue > maxvalue) std::swap(minvalue, maxvalue);
        
        QString binstr;
        if (maxbin != minbin) {
            binstr = tr("%1 - %2").arg(minbin+1).arg(maxbin+1);
        } else {
            binstr = QString("%1").arg(minbin+1);
        }

        QString valuestr;
        if (maxvalue != minvalue) {
            valuestr = tr("%1 - %2").arg(minvalue).arg(maxvalue);
        } else {
            valuestr = QString("%1").arg(minvalue);
        }

        QString description = tr("Time:\t%1 - %2\nRange:\t%3 samples (%4)\nBin:\t%5\n%6 value:\t%7")
            .arg(QString::fromStdString(rt0.toText(true)))
            .arg(QString::fromStdString(rt1.toText(true)))
            .arg(range)
            .arg(rtrangestr)
            .arg(binstr)
            .arg(m_samplingMode == NearestSample ? tr("First") :
                 m_samplingMode == SampleMean ? tr("Mean") : tr("Peak"))
            .arg(valuestr);
        
        return description;
    
    } else {

        QString description = tr("Time:\t%1 - %2\nRange:\t%3 samples (%4)")
            .arg(QString::fromStdString(rt0.toText(true)))
            .arg(QString::fromStdString(rt1.toText(true)))
            .arg(range)
            .arg(rtrangestr);
        
        return description;
    }
}

double
SliceLayer::getXForBin(const LayerGeometryProvider *v, double bin) const
{
    return getXForScalePoint(v, bin, m_minbin, m_maxbin);
}

double
SliceLayer::getXForScalePoint(const LayerGeometryProvider *v,
                              double p, double pmin, double pmax) const
{
    double x = 0;

    int pw = v->getPaintWidth();
    int origin = m_xorigins[v->getId()];
    int w = pw - origin;
    if (w < 1) w = 1;

    if (pmax <= pmin) {
        pmax = pmin + 1.0;
    }
    
    if (p < pmin) p = pmin;
    if (p > pmax) p = pmax;

    if (m_binScale == LinearBins) {
        x = (w * (p - pmin)) / (pmax - pmin);
    } else {

        if (m_binScale == InvertedLogBins) {
            // stoopid
            p = pmax - p;
        }
        
        // The 0.8 here is an awkward compromise. Our x-coord is
        // proportional to log of bin number, with the x-coord "of a
        // bin" being that of the left edge of the bin range. We can't
        // start counting bins from 0, as that would give us x = -Inf
        // and hide the first bin entirely. But if we start from 1, we
        // are giving a lot of space to the first bin, which in most
        // display modes won't be used because the "point" location
        // for that bin is in the middle of it. Yet in some modes
        // we'll still want it. A compromise is to count our first bin
        // as "a bit less than 1", so that most of it is visible but a
        // bit is tactfully cropped at the left edge so it doesn't
        // take up so much space.
        const double origin = 0.8;
        
        // sometimes we are called with a pmin/pmax range that begins
        // before 0: in that situation, we shift everything along by
        // the difference between 0 and pmin before doing any other
        // calculations
        double reqdshift = 0.0;
        if (pmin < 0) reqdshift = -pmin;

        double pminlog = log10(pmin + reqdshift + origin);
        double pmaxlog = log10(pmax + reqdshift + origin);
        double plog = log10(p + reqdshift + origin);
        x = (w * (plog - pminlog)) / (pmaxlog - pminlog);
        /*
        cerr << "getXForScalePoint(" << p << "): pmin = " << pmin
             << ", pmax = " << pmax << ", w = " << w
             << ", reqdshift = " << reqdshift
             << ", pminlog = " << pminlog << ", pmaxlog = " << pmaxlog
             << ", plog = " << plog 
             << " -> x = " << x << endl;
        */
        if (m_binScale == InvertedLogBins) {
            // still stoopid
            x = w - x;
        }
    }
    
    return x + origin;
}

double
SliceLayer::getBinForX(const LayerGeometryProvider *v, double x) const
{
    return getScalePointForX(v, x, m_minbin, m_maxbin);
}

double
SliceLayer::getScalePointForX(const LayerGeometryProvider *v,
                              double x, double pmin, double pmax) const
{
    double p = 0;

    int pw = v->getPaintWidth();
    int origin = m_xorigins[v->getId()];

    int w = pw - origin;
    if (w < 1) w = 1;

    x = x - origin;
    if (x < 0) x = 0;

    double eps = 1e-10;

    if (pmax <= pmin) {
        pmax = pmin + 1.0;
    }

    if (m_binScale == LinearBins) {
        p = pmin + eps + (x * (pmax - pmin)) / w;
    } else {

        if (m_binScale == InvertedLogBins) {
            x = w - x;
        }

        // See comments in getXForScalePoint

        const double origin = 0.8;
        double reqdshift = 0.0;
        if (pmin < 0) reqdshift = -pmin;

        double pminlog = log10(pmin + reqdshift + origin);
        double pmaxlog = log10(pmax + reqdshift + origin);

        double plog = pminlog + eps + (x * (pmaxlog - pminlog)) / w;
        p = pow(10.0, plog) - reqdshift - origin;

        if (m_binScale == InvertedLogBins) {
            p = pmax - p;
        }
    }

    return p;
}

double
SliceLayer::getYForValue(const LayerGeometryProvider *v, double value, double &norm) const
{
    norm = 0.0;

    if (m_yorigins.find(v->getId()) == m_yorigins.end()) return 0;

    value *= m_gain;

    int yorigin = m_yorigins[v->getId()];
    int h = m_heights[v->getId()];
    double thresh = getThresholdDb();

    double y = 0.0;

    if (h <= 0) return y;

    switch (m_energyScale) {

    case dBScale:
    {
        double db = thresh;
        if (value > 0.0) db = 10.0 * log10(fabs(value));
        if (db < thresh) db = thresh;
        norm = (db - thresh) / -thresh;
        y = yorigin - (double(h) * norm);
        break;
    }
    
    case MeterScale:
        y = AudioLevel::multiplier_to_preview(value, h);
        norm = double(y) / double(h);
        y = yorigin - y;
        break;
        
    case AbsoluteScale:
        value = fabs(value);
#if (__GNUC__ >= 7)
        __attribute__ ((fallthrough));
#endif 
        
    case LinearScale:
    default:
        norm = (value - m_threshold);
        if (norm < 0) norm = 0;
        y = yorigin - (double(h) * norm);
        break;
    }
    
    return y;
}

double
SliceLayer::getValueForY(const LayerGeometryProvider *v, double y) const
{
    double value = 0.0;

    if (m_yorigins.find(v->getId()) == m_yorigins.end()) return value;

    int yorigin = m_yorigins[v->getId()];
    int h = m_heights[v->getId()];
    double thresh = getThresholdDb();

    if (h <= 0) return value;

    y = yorigin - y;

    switch (m_energyScale) {

    case dBScale:
    {
        double db = ((y / h) * -thresh) + thresh;
        value = pow(10.0, db/10.0);
        break;
    }

    case MeterScale:
        value = AudioLevel::preview_to_multiplier(int(lrint(y)), h);
        break;

    case LinearScale:
    case AbsoluteScale:
    default:
        value = y / h + m_threshold;
    }

    return value / m_gain;
}

void
SliceLayer::paint(LayerGeometryProvider *v, QPainter &paint, QRect rect) const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel ||
        !sliceableModel->isOK() ||
        !sliceableModel->isReady()) return;

    Profiler profiler("SliceLayer::paint()");

    paint.save();
    paint.setRenderHint(QPainter::Antialiasing, true);
    paint.setBrush(Qt::NoBrush);

    if (v->getViewManager() && v->getViewManager()->shouldShowScaleGuides()) {
        if (!m_scalePoints.empty()) {
            paint.setPen(QColor(240, 240, 240)); //!!! and dark background?
            int ratio = int(round(double(v->getPaintHeight()) / 
                                  m_scalePaintHeight));
            for (int i = 0; i < (int)m_scalePoints.size(); ++i) {
                paint.drawLine(0, m_scalePoints[i] * ratio, 
                               rect.width(), m_scalePoints[i] * ratio);
            }
        }
    }

    int mh = sliceableModel->getHeight();
    int bin0 = 0;
    if (m_maxbin > m_minbin) {
        mh = m_maxbin - m_minbin;
        bin0 = m_minbin;
    }
    
    if (m_plotStyle == PlotBlocks) {
        // Must use actual zero-width pen, too slow otherwise
        paint.setPen(QPen(getBaseQColor(), 0));
    } else {
        // Similarly, if there are very many bins here, we use a
        // thinner pen
        QPen pen;
        if (mh < 10000) {
            pen = v->scalePen(QPen(getBaseQColor(), 0.8));
        } else {
            pen = QPen(getBaseQColor(), 1);
        }
        paint.setPen(pen);
    }

    int xorigin = getVerticalScaleWidth(v, true, paint) + 1;
    m_xorigins[v->getId()] = xorigin; // for use in getFeatureDescription
    
    int yorigin = v->getPaintHeight() - getHorizontalScaleHeight(v, paint) -
        paint.fontMetrics().height();
    int h = yorigin - paint.fontMetrics().height() - 8;

    m_yorigins[v->getId()] = yorigin; // for getYForValue etc
    m_heights[v->getId()] = h;

    if (h <= 0) return;

    QPainterPath path;
    
    int divisor = 0;

    m_values.clear();
    for (int bin = 0; bin < mh; ++bin) {
        m_values.push_back(0.0);
    }

    sv_frame_t f0 = v->getCentreFrame();
    int f0x = v->getXForFrame(f0);
    f0 = v->getFrameForX(f0x);
    sv_frame_t f1 = v->getFrameForX(f0x + 1);
    if (f1 > f0) --f1;

//    cerr << "centre frame " << v->getCentreFrame() << ", x " << f0x << ", f0 " << f0 << ", f1 " << f1 << endl;

    int res = sliceableModel->getResolution();
    int col0 = int(f0 / res);
    int col1 = col0;
    if (m_samplingMode != NearestSample) col1 = int(f1 / res);
    f0 = col0 * res;
    f1 = (col1 + 1) * res - 1;

//    cerr << "resolution " << res << ", col0 " << col0 << ", col1 " << col1 << ", f0 " << f0 << ", f1 " << f1 << endl;
//    cerr << "mh = " << mh << endl;

    m_currentf0 = f0;
    m_currentf1 = f1;

    BiasCurve curve;
    getBiasCurve(curve);
    int cs = int(curve.size());

    for (int col = col0; col <= col1; ++col) {
        DenseThreeDimensionalModel::Column column =
            sliceableModel->getColumn(col);
        for (int bin = 0; bin < mh; ++bin) {
            float value = column[bin0 + bin];
            if (bin < cs) value *= curve[bin];
            if (m_samplingMode == SamplePeak) {
                if (value > m_values[bin]) m_values[bin] = value;
            } else {
                m_values[bin] += value;
            }
        }
        ++divisor;
    }

    float max = 0.0;
    for (int bin = 0; bin < mh; ++bin) {
        if (m_samplingMode == SampleMean && divisor > 0) {
            m_values[bin] /= float(divisor);
        }
        if (m_values[bin] > max) max = m_values[bin];
    }
    if (max != 0.0 && m_normalize) {
        for (int bin = 0; bin < mh; ++bin) {
            m_values[bin] /= max;
        }
    }

    ColourMapper mapper(m_colourMap, m_colourInverted, 0, 1);

    double ytop = 0, ybottom = 0;
    bool firstBinOfPixel = true;

    QColor prevColour = v->getBackground();
    double prevYtop = 0;
    
    double xleft = -1, xmiddle = -1, xright = -1;
    double prevXmiddle = 0;

    for (int bin = 0; bin < mh; ++bin) {

        if (m_binAlignment == BinsSpanScalePoints) {
            if (xright >= 0) xleft = xright; // previous value of
            else xleft = getXForBin(v, bin0 + bin);
            xmiddle = getXForBin(v, bin0 + bin + 0.5);
            xright = getXForBin(v, bin0 + bin + 1);
        } else {
            if (xright >= 0) xleft = xright; // previous value of
            else xleft = getXForBin(v, bin0 + bin - 0.5);
            xmiddle = getXForBin(v, bin0 + bin);
            xright = getXForBin(v, bin0 + bin + 0.5);
        }

        double value = m_values[bin];
        double norm = 0.0;
        double y = getYForValue(v, value, norm);

        if (y < ytop || firstBinOfPixel) {
            ytop = y;
        }
        if (y > ybottom || firstBinOfPixel) {
            ybottom = y;
        }

        if (int(xright) != int(xleft) || bin+1 == mh) {

            if (m_plotStyle == PlotLines) {

                if (bin == 0) {
                    path.moveTo(xmiddle, y);
                } else {
                    if (ytop != ybottom) {
                        path.lineTo(xmiddle, ybottom);
                        path.lineTo(xmiddle, ytop);
                        path.moveTo(xmiddle, ybottom);
                    } else {
                        path.lineTo(xmiddle, ytop);
                    }
                }

            } else if (m_plotStyle == PlotSteps) {

                if (bin == 0) {
                    path.moveTo(xleft, y);
                } else {
                    path.lineTo(xleft, ytop);
                }
                path.lineTo(xright, ytop);

            } else if (m_plotStyle == PlotBlocks) {

                // work in pixel coords here, as we don't want the
                // vertical edges to be antialiased

                path.moveTo(QPoint(int(xleft), int(yorigin)));
                path.lineTo(QPoint(int(xleft), int(ytop)));
                path.lineTo(QPoint(int(xright), int(ytop)));
                path.lineTo(QPoint(int(xright), int(yorigin)));
                path.lineTo(QPoint(int(xleft), int(yorigin)));

            } else if (m_plotStyle == PlotFilledBlocks) {

                QColor c = mapper.map(norm);
                paint.setPen(Qt::NoPen);

                // work in pixel coords here, as we don't want the
                // vertical edges to be antialiased

                if (xright > xleft + 1) {
                
                    QVector<QPoint> pp;
                    
                    if (bin > 0) {
                        paint.setBrush(prevColour);
                        pp.clear();
                        pp << QPoint(int(prevXmiddle), int(yorigin));
                        pp << QPoint(int(prevXmiddle), int(prevYtop));
                        pp << QPoint(int((xmiddle + prevXmiddle) / 2),
                                     int((ytop + prevYtop) / 2));
                        pp << QPoint(int((xmiddle + prevXmiddle) / 2),
                                     int(yorigin));
                        paint.drawConvexPolygon(QPolygon(pp));

                        paint.setBrush(c);
                        pp.clear();
                        pp << QPoint(int((xmiddle + prevXmiddle) / 2),
                                     int(yorigin));
                        pp << QPoint(int((xmiddle + prevXmiddle) / 2),
                                     int((ytop + prevYtop) / 2));
                        pp << QPoint(int(xmiddle), int(ytop));
                        pp << QPoint(int(xmiddle), int(yorigin));
                        paint.drawConvexPolygon(QPolygon(pp));
                    }

                    prevColour = c;
                    prevYtop = ytop;

                } else {
                    
                    paint.fillRect(QRect(int(xleft), int(ytop),
                                         int(xright) - int(xleft),
                                         int(yorigin) - int(ytop)),
                                   c);
                }

                prevXmiddle = xmiddle;
            }

            firstBinOfPixel = true;

        } else {
            firstBinOfPixel = false;
        }
    }

    if (m_plotStyle != PlotFilledBlocks) {
        paint.drawPath(path);
    }
    paint.restore();
}

int
SliceLayer::getVerticalScaleWidth(LayerGeometryProvider *, bool, QPainter &paint) const
{
    // Qt 5.13 deprecates QFontMetrics::width(), but its suggested
    // replacement (horizontalAdvance) was only added in Qt 5.11
    // which is too new for us
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    int width;
    if (m_energyScale == LinearScale || m_energyScale == AbsoluteScale) {
        width = std::max(paint.fontMetrics().width("0.0") + 13,
                         paint.fontMetrics().width("x10-10"));
    } else {
        width = std::max(paint.fontMetrics().width(tr("0dB")),
                         paint.fontMetrics().width(tr("-Inf"))) + 13;
    }
    return width;
}

void
SliceLayer::paintVerticalScale(LayerGeometryProvider *v, bool, QPainter &paint, QRect rect) const
{
    double thresh = m_threshold;
    if (m_energyScale != LinearScale && m_energyScale != AbsoluteScale) {
        thresh = AudioLevel::dB_to_multiplier(getThresholdDb());
    }
    
//    int h = (rect.height() * 3) / 4;
//    int y = (rect.height() / 2) - (h / 2);
    
    int yorigin = v->getPaintHeight() - getHorizontalScaleHeight(v, paint) -
        paint.fontMetrics().height();
    int h = yorigin - paint.fontMetrics().height() - 8;
    if (h < 0) return;

    QRect actual(rect.x(), rect.y() + yorigin - h, rect.width(), h);

    int mult = 1;

    PaintAssistant::paintVerticalLevelScale
        (paint, actual, thresh, 1.0 / m_gain,
         PaintAssistant::Scale(m_energyScale),
         mult,
         const_cast<std::vector<int> *>(&m_scalePoints));

    // Ugly hack (but then everything about this scale drawing is a
    // bit ugly). In pixel-doubling hi-dpi scenarios, the scale is
    // painted at pixel-doubled resolution but we do explicit
    // pixel-doubling ourselves when painting the layer content. We
    // make a note of this here so that we can compare with the
    // equivalent dimension in the paint method when deciding where to
    // place scale continuation lines.
    m_scalePaintHeight = v->getPaintHeight();
    
    if (mult != 1 && mult != 0) {
        int log = int(lrint(log10(mult)));
        QString a = tr("x10");
        QString b = QString("%1").arg(-log);
        paint.drawText(3, 8 + paint.fontMetrics().ascent(), a);
        paint.drawText(3 + paint.fontMetrics().width(a),
                       3 + paint.fontMetrics().ascent(), b);
    }
}

bool
SliceLayer::hasLightBackground() const
{
    if (usesSolidColour()) {
        ColourMapper mapper(m_colourMap, m_colourInverted, 0, 1);
        return mapper.hasLightBackground();
    } else {
        return SingleColourLayer::hasLightBackground();
    }
}

Layer::PropertyList
SliceLayer::getProperties() const
{
    PropertyList list = SingleColourLayer::getProperties();
    list.push_back("Bin Scale");
    list.push_back("Plot Type");
    list.push_back("Scale");
    list.push_back("Normalize");
    list.push_back("Threshold");
    list.push_back("Gain");

    return list;
}

QString
SliceLayer::getPropertyLabel(const PropertyName &name) const
{
    if (name == "Plot Type") return tr("Plot Type");
    if (name == "Scale") return tr("Scale");
    if (name == "Normalize") return tr("Normalize");
    if (name == "Threshold") return tr("Threshold");
    if (name == "Gain") return tr("Gain");
    if (name == "Sampling Mode") return tr("Sampling Mode");
    if (name == "Bin Scale") return tr("Bin Scale");
    return SingleColourLayer::getPropertyLabel(name);
}

QString
SliceLayer::getPropertyIconName(const PropertyName &name) const
{
    if (name == "Normalize") return "normalise";
    return "";
}

Layer::PropertyType
SliceLayer::getPropertyType(const PropertyName &name) const
{
    if (name == "Gain") return RangeProperty;
    if (name == "Normalize") return ToggleProperty;
    if (name == "Threshold") return RangeProperty;
    if (name == "Plot Type") return ValueProperty;
    if (name == "Scale") return ValueProperty;
    if (name == "Sampling Mode") return ValueProperty;
    if (name == "Bin Scale") return ValueProperty;
    if (name == "Colour" && usesSolidColour()) return ColourMapProperty;
    return SingleColourLayer::getPropertyType(name);
}

QString
SliceLayer::getPropertyGroupName(const PropertyName &name) const
{
    if (name == "Scale" ||
        name == "Normalize" ||
        name == "Sampling Mode" ||
        name == "Threshold" ||
        name == "Gain") return tr("Scale");
    if (name == "Plot Type" ||
        name == "Bin Scale") return tr("Bins");
    return SingleColourLayer::getPropertyGroupName(name);
}

int
SliceLayer::getPropertyRangeAndValue(const PropertyName &name,
                                     int *min, int *max, int *deflt) const
{
    int val = 0;

    int garbage0, garbage1, garbage2;
    if (!min) min = &garbage0;
    if (!max) max = &garbage1;
    if (!deflt) deflt = &garbage2;

    if (name == "Gain") {

        *min = -50;
        *max = 50;
        *deflt = 0;

//        cerr << "gain is " << m_gain << ", mode is " << m_samplingMode << endl;

        val = int(lrint(log10(m_gain) * 20.0));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Threshold") {
        
        *min = -80;
        *max = 0;

        *deflt = int(lrint(AudioLevel::multiplier_to_dB(m_initialThreshold)));
        if (*deflt < *min) *deflt = *min;
        if (*deflt > *max) *deflt = *max;

        val = int(lrint(AudioLevel::multiplier_to_dB(m_threshold)));
        if (val < *min) val = *min;
        if (val > *max) val = *max;

    } else if (name == "Normalize") {
        
        val = (m_normalize ? 1 : 0);
        *deflt = 0;

    } else if (name == "Colour" && usesSolidColour()) {
            
        *min = 0;
        *max = ColourMapper::getColourMapCount() - 1;
        *deflt = int(ColourMapper::Ice);
        
        val = m_colourMap;

    } else if (name == "Scale") {

        *min = 0;
        *max = 3;
        *deflt = (int)dBScale;

        val = (int)m_energyScale;

    } else if (name == "Sampling Mode") {

        *min = 0;
        *max = 2;
        *deflt = (int)SampleMean;
        
        val = (int)m_samplingMode;

    } else if (name == "Plot Type") {
        
        *min = 0;
        *max = 3;
        *deflt = (int)PlotSteps;

        val = (int)m_plotStyle;

    } else if (name == "Bin Scale") {
        
        *min = 0;
        *max = 2;
        *deflt = (int)LinearBins;
//        *max = 1; // I don't think we really do want to offer inverted log

        val = (int)m_binScale;

    } else {
        val = SingleColourLayer::getPropertyRangeAndValue(name, min, max, deflt);
    }

    return val;
}

QString
SliceLayer::getPropertyValueLabel(const PropertyName &name,
                                  int value) const
{
    if (name == "Colour" && usesSolidColour()) {
        return ColourMapper::getColourMapLabel(value);
    }
    if (name == "Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Meter");
        case 2: return tr("Log");
        case 3: return tr("Absolute");
        }
    }
    if (name == "Sampling Mode") {
        switch (value) {
        default:
        case 0: return tr("Any");
        case 1: return tr("Mean");
        case 2: return tr("Peak");
        }
    }
    if (name == "Plot Type") {
        switch (value) {
        default:
        case 0: return tr("Lines");
        case 1: return tr("Steps");
        case 2: return tr("Blocks");
        case 3: return tr("Colours");
        }
    }
    if (name == "Bin Scale") {
        switch (value) {
        default:
        case 0: return tr("Linear");
        case 1: return tr("Log");
        case 2: return tr("Rev Log");
        }
    }
    return SingleColourLayer::getPropertyValueLabel(name, value);
}

RangeMapper *
SliceLayer::getNewPropertyRangeMapper(const PropertyName &name) const
{
    if (name == "Gain") {
        return new LinearRangeMapper(-50, 50, -25, 25, tr("dB"));
    }
    if (name == "Threshold") {
        return new LinearRangeMapper(-80, 0, -80, 0, tr("dB"));
    }
    return SingleColourLayer::getNewPropertyRangeMapper(name);
}

void
SliceLayer::setProperty(const PropertyName &name, int value)
{
    if (name == "Gain") {
        setGain(powf(10, float(value)/20.0f));
    } else if (name == "Threshold") {
        if (value == -80) setThreshold(0.0f);
        else setThreshold(float(AudioLevel::dB_to_multiplier(value)));
    } else if (name == "Colour" && usesSolidColour()) {
        setFillColourMap(value);
    } else if (name == "Scale") {
        switch (value) {
        default:
        case 0: setEnergyScale(LinearScale); break;
        case 1: setEnergyScale(MeterScale); break;
        case 2: setEnergyScale(dBScale); break;
        case 3: setEnergyScale(AbsoluteScale); break;
        }
    } else if (name == "Plot Type") {
        setPlotStyle(PlotStyle(value));
    } else if (name == "Sampling Mode") {
        switch (value) {
        default:
        case 0: setSamplingMode(NearestSample); break;
        case 1: setSamplingMode(SampleMean); break;
        case 2: setSamplingMode(SamplePeak); break;
        }
    } else if (name == "Bin Scale") {
        switch (value) {
        default:
        case 0: setBinScale(LinearBins); break;
        case 1: setBinScale(LogBins); break;
        case 2: setBinScale(InvertedLogBins); break;
        }
    } else if (name == "Normalize") {
        setNormalize(value ? true : false);
    } else {
        SingleColourLayer::setProperty(name, value);
    }
}

void
SliceLayer::setFillColourMap(int map)
{
    if (m_colourMap == map) return;
    m_colourMap = map;
    emit layerParametersChanged();
}

void
SliceLayer::setEnergyScale(EnergyScale scale)
{
    if (m_energyScale == scale) return;
    m_energyScale = scale;
    emit layerParametersChanged();
}

void
SliceLayer::setSamplingMode(SamplingMode mode)
{
    if (m_samplingMode == mode) return;
    m_samplingMode = mode;
    emit layerParametersChanged();
}

void
SliceLayer::setPlotStyle(PlotStyle style)
{
    if (m_plotStyle == style) return;
    bool colourTypeChanged = (style == PlotFilledBlocks ||
                              m_plotStyle == PlotFilledBlocks);
    m_plotStyle = style;
    if (colourTypeChanged) {
        emit layerParameterRangesChanged();
    }
    emit layerParametersChanged();
}

void
SliceLayer::setBinScale(BinScale scale)
{
    if (m_binScale == scale) return;
    m_binScale = scale;
    emit layerParametersChanged();
}

void
SliceLayer::setNormalize(bool n)
{
    if (m_normalize == n) return;
    m_normalize = n;
    emit layerParametersChanged();
}

void
SliceLayer::setThreshold(float thresh)
{
    if (m_threshold == thresh) return;
    m_threshold = thresh;
    emit layerParametersChanged();
}

void
SliceLayer::setGain(float gain)
{
    if (m_gain == gain) return;
    m_gain = gain;
    emit layerParametersChanged();
}

float
SliceLayer::getThresholdDb() const
{
    if (m_threshold == 0.0) return -80.f;
    float db = float(AudioLevel::multiplier_to_dB(m_threshold));
    return db;
}

int
SliceLayer::getDefaultColourHint(bool darkbg, bool &impose)
{
    impose = false;
    return ColourDatabase::getInstance()->getColourIndex
        (QString(darkbg ? "Bright Blue" : "Blue"));
}

void
SliceLayer::toXml(QTextStream &stream,
                  QString indent, QString extraAttributes) const
{
    QString s;
    
    s += QString("energyScale=\"%1\" "
                 "samplingMode=\"%2\" "
                 "plotStyle=\"%3\" "
                 "binScale=\"%4\" "
                 "gain=\"%5\" "
                 "threshold=\"%6\" "
                 "normalize=\"%7\" %8 ")
        .arg(m_energyScale)
        .arg(m_samplingMode)
        .arg(m_plotStyle)
        .arg(m_binScale)
        .arg(m_gain)
        .arg(m_threshold)
        .arg(m_normalize ? "true" : "false")
        .arg(QString("minbin=\"%1\" "
                     "maxbin=\"%2\"")
             .arg(m_minbin)
             .arg(m_maxbin));

    // New-style colour map attribute, by string id rather than by
    // number

    s += QString("fillColourMap=\"%1\" ")
        .arg(ColourMapper::getColourMapId(m_colourMap));

    // Old-style colour map attribute

    s += QString("colourScheme=\"%1\" ")
        .arg(ColourMapper::getBackwardCompatibilityColourMap(m_colourMap));
    
    SingleColourLayer::toXml(stream, indent, extraAttributes + " " + s);
}

void
SliceLayer::setProperties(const QXmlAttributes &attributes)
{
    bool ok = false;

    SingleColourLayer::setProperties(attributes);

    EnergyScale scale = (EnergyScale)
        attributes.value("energyScale").toInt(&ok);
    if (ok) setEnergyScale(scale);

    SamplingMode mode = (SamplingMode)
        attributes.value("samplingMode").toInt(&ok);
    if (ok) setSamplingMode(mode);

    QString colourMapId = attributes.value("fillColourMap");
    int colourMap = ColourMapper::getColourMapById(colourMapId);
    if (colourMap >= 0) {
        setFillColourMap(colourMap);
    } else {
        colourMap = attributes.value("colourScheme").toInt(&ok);
        if (ok && colourMap < ColourMapper::getColourMapCount()) {
            setFillColourMap(colourMap);
        }
    }

    PlotStyle s = (PlotStyle)
        attributes.value("plotStyle").toInt(&ok);
    if (ok) setPlotStyle(s);

    BinScale b = (BinScale)
        attributes.value("binScale").toInt(&ok);
    if (ok) setBinScale(b);

    float gain = attributes.value("gain").toFloat(&ok);
    if (ok) setGain(gain);

    float threshold = attributes.value("threshold").toFloat(&ok);
    if (ok) setThreshold(threshold);

    bool normalize = (attributes.value("normalize").trimmed() == "true");
    setNormalize(normalize);

    bool alsoOk = false;
    
    float min = attributes.value("minbin").toFloat(&ok);
    float max = attributes.value("maxbin").toFloat(&alsoOk);
    if (ok && alsoOk) setDisplayExtents(min, max);
}

bool
SliceLayer::getValueExtents(double &min, double &max, bool &logarithmic,
                            QString &unit) const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return false;
    
    min = 0;
    max = double(sliceableModel->getHeight());

    logarithmic = (m_binScale == BinScale::LogBins);
    unit = "";

    return true;
}

bool
SliceLayer::getDisplayExtents(double &min, double &max) const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return false;

    double hmax = double(sliceableModel->getHeight());
    
    min = m_minbin;
    max = m_maxbin;
    if (max <= min) {
        min = 0;
        max = hmax;
    }
    if (min < 0) min = 0;
    if (max > hmax) max = hmax;

    return true;
}

bool
SliceLayer::setDisplayExtents(double min, double max)
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return false;

    m_minbin = int(lrint(min));
    m_maxbin = int(lrint(max));

    if (m_minbin < 0) {
        m_minbin = 0;
    }
    if (m_maxbin < 0) {
        m_maxbin = 0;
    }
    if (m_minbin > sliceableModel->getHeight()) {
        m_minbin = sliceableModel->getHeight();
    }
    if (m_maxbin > sliceableModel->getHeight()) {
        m_maxbin = sliceableModel->getHeight();
    }
    if (m_maxbin < m_minbin) {
        m_maxbin = m_minbin;
    }

    emit layerParametersChanged();
    return true;
}

int
SliceLayer::getVerticalZoomSteps(int &defaultStep) const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return 0;

    defaultStep = 0;
    int h = sliceableModel->getHeight();
    return h;
}

int
SliceLayer::getCurrentVerticalZoomStep() const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return 0;

    double min, max;
    getDisplayExtents(min, max);
    return sliceableModel->getHeight() - int(lrint(max - min));
}

void
SliceLayer::setVerticalZoomStep(int step)
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return;

//    SVDEBUG << "SliceLayer::setVerticalZoomStep(" <<step <<"): before: minbin = " << m_minbin << ", maxbin = " << m_maxbin << endl;

    int dist = sliceableModel->getHeight() - step;
    if (dist < 1) dist = 1;
    double centre = m_minbin + (m_maxbin - m_minbin) / 2.0;
    int minbin = int(lrint(centre - dist/2.0));
    int maxbin = minbin + dist;
    setDisplayExtents(minbin, maxbin);
}

RangeMapper *
SliceLayer::getNewVerticalZoomRangeMapper() const
{
    auto sliceableModel =
        ModelById::getAs<DenseThreeDimensionalModel>(m_sliceableModel);
    if (!sliceableModel) return nullptr;

    return new LinearRangeMapper(0, sliceableModel->getHeight(),
                                 0, sliceableModel->getHeight(), "");
}

void
SliceLayer::zoomToRegion(const LayerGeometryProvider *v, QRect rect)
{
    double bin0 = getBinForX(v, rect.x());
    double bin1 = getBinForX(v, rect.x() + rect.width());

    // ignore y for now...

    SVDEBUG << "SliceLayer::zoomToRegion: zooming to bin range "
            << bin0 << " -> " << bin1 << endl;
    
    setDisplayExtents(floor(bin0), ceil(bin1));
}

