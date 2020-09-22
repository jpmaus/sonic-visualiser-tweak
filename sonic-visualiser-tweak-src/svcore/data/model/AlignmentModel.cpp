/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "AlignmentModel.h"

#include "SparseTimeValueModel.h"

//#define DEBUG_ALIGNMENT_MODEL 1

AlignmentModel::AlignmentModel(ModelId reference,
                               ModelId aligned,
                               ModelId pathSource) :
    m_reference(reference),
    m_aligned(aligned),
    m_pathSource(pathSource),
    m_path(nullptr),
    m_reversePath(nullptr),
    m_pathBegun(false),
    m_pathComplete(false),
    m_relativePitch(0)
{
    setPathFrom(pathSource);

    if (m_reference == m_aligned) {
        // Trivial alignment, e.g. of main model to itself, which we
        // record so that we can distinguish the reference model for
        // alignments from an unaligned model. No path required
        m_pathComplete = true;
    }
}

AlignmentModel::~AlignmentModel()
{
#ifdef DEBUG_ALIGNMENT_MODEL
    SVCERR << "AlignmentModel(" << this << ")::~AlignmentModel()" << endl;
#endif
}

bool
AlignmentModel::isOK() const
{
    if (m_error != "") return false;
    if (m_pathSource.isNone()) return true;
    auto pathSourceModel =
        ModelById::getAs<SparseTimeValueModel>(m_pathSource);
    if (pathSourceModel) {
        return pathSourceModel->isOK();
    }
    return true;
}

sv_frame_t
AlignmentModel::getStartFrame() const
{
    auto reference = ModelById::get(m_reference);
    auto aligned = ModelById::get(m_aligned);
    
    if (reference && aligned) {
        sv_frame_t a = reference->getStartFrame();
        sv_frame_t b = aligned->getStartFrame();
        return std::min(a, b);
    } else {
        return 0;
    }
}

sv_frame_t
AlignmentModel::getTrueEndFrame() const
{
    auto reference = ModelById::get(m_reference);
    auto aligned = ModelById::get(m_aligned);

    if (reference && aligned) {
        sv_frame_t a = reference->getEndFrame();
        sv_frame_t b = aligned->getEndFrame();
        return std::max(a, b);
    } else {
        return 0;
    }
}

sv_samplerate_t
AlignmentModel::getSampleRate() const
{
    auto reference = ModelById::get(m_reference);
    if (reference) {
        return reference->getSampleRate();
    } else {
        return 0;
    }
}

bool
AlignmentModel::isReady(int *completion) const
{
    if (!m_pathBegun && !m_pathSource.isNone()) {
        if (completion) *completion = 0;
#ifdef DEBUG_ALIGNMENT_MODEL
        SVCERR << "AlignmentModel::isReady: path not begun" << endl;
#endif
        return false;
    }
    if (m_pathComplete) {
        if (completion) *completion = 100;
#ifdef DEBUG_ALIGNMENT_MODEL
        SVCERR << "AlignmentModel::isReady: path complete" << endl;
#endif
        return true;
    }
    if (m_pathSource.isNone()) {
        // lack of raw path could mean path is complete (in which case
        // m_pathComplete true above) or else no path source has been
        // set at all yet (this case)
        if (completion) *completion = 0;
#ifdef DEBUG_ALIGNMENT_MODEL
        SVCERR << "AlignmentModel::isReady: no raw path" << endl;
#endif
        return false;
    }
    auto pathSourceModel =
        ModelById::getAs<SparseTimeValueModel>(m_pathSource);
    if (pathSourceModel) {
        return pathSourceModel->isReady(completion);
    } else {
        return true; // there is no meaningful answer here
    }
}

const ZoomConstraint *
AlignmentModel::getZoomConstraint() const
{
    return nullptr;
}

ModelId
AlignmentModel::getReferenceModel() const
{
    return m_reference;
}

ModelId
AlignmentModel::getAlignedModel() const
{
    return m_aligned;
}

sv_frame_t
AlignmentModel::toReference(sv_frame_t frame) const
{
#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::toReference(" << frame << ")" << endl;
#endif
    if (!m_path) {
        if (m_pathSource.isNone()) {
            return frame;
        }
        constructPath();
    }
    if (!m_path) {
        return frame;
    }

    return performAlignment(*m_path, frame);
}

sv_frame_t
AlignmentModel::fromReference(sv_frame_t frame) const
{
#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::fromReference(" << frame << ")" << endl;
#endif
    if (!m_reversePath) {
        if (m_pathSource.isNone()) {
            return frame;
        }
        constructReversePath();
    }
    if (!m_reversePath) {
        return frame;
    }

    return performAlignment(*m_reversePath, frame);
}

void
AlignmentModel::pathSourceChangedWithin(ModelId, sv_frame_t, sv_frame_t)
{
    if (!m_pathComplete) return;
    constructPath();
    constructReversePath();
}    

void
AlignmentModel::pathSourceCompletionChanged(ModelId)
{
    auto pathSourceModel =
        ModelById::getAs<SparseTimeValueModel>(m_pathSource);
    if (!pathSourceModel) return;
    
    m_pathBegun = true;

    if (!m_pathComplete) {

        int completion = 0;
        pathSourceModel->isReady(&completion);

#ifdef DEBUG_ALIGNMENT_MODEL
        SVCERR << "AlignmentModel::pathCompletionChanged: completion = "
               << completion << endl;
#endif

        m_pathComplete = (completion == 100);

        if (m_pathComplete) {

            constructPath();
            constructReversePath();

#ifdef DEBUG_ALIGNMENT_MODEL
            SVCERR << "AlignmentModel: path complete" << endl;
#endif
        }
    }

    emit completionChanged(getId());
}

void
AlignmentModel::constructPath() const
{
    auto alignedModel = ModelById::get(m_aligned);
    if (!alignedModel) return;
    
    auto pathSourceModel =
        ModelById::getAs<SparseTimeValueModel>(m_pathSource);
    if (!m_path) {
        if (!pathSourceModel) {
            cerr << "ERROR: AlignmentModel::constructPath: "
                 << "No raw path available (id is " << m_pathSource
                 << ")" << endl;
            return;
        }
        m_path.reset(new Path
                     (pathSourceModel->getSampleRate(),
                      pathSourceModel->getResolution()));
    } else {
        if (!pathSourceModel) return;
    }
        
    m_path->clear();

    EventVector points = pathSourceModel->getAllEvents();

    for (const auto &p: points) {
        sv_frame_t frame = p.getFrame();
        double value = p.getValue();
        sv_frame_t rframe = lrint(value * alignedModel->getSampleRate());
        m_path->add(PathPoint(frame, rframe));
    }

#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::constructPath: " << m_path->getPointCount() << " points, at least " << (2 * m_path->getPointCount() * (3 * sizeof(void *) + sizeof(int) + sizeof(PathPoint))) << " bytes" << endl;
#endif
}

void
AlignmentModel::constructReversePath() const
{
    if (!m_reversePath) {
        if (!m_path) {
            cerr << "ERROR: AlignmentModel::constructReversePath: "
                      << "No forward path available" << endl;
            return;
        }
        m_reversePath.reset(new Path
                            (m_path->getSampleRate(),
                             m_path->getResolution()));
    } else {
        if (!m_path) return;
    }
        
    m_reversePath->clear();

    Path::Points points = m_path->getPoints();
        
    for (auto p: points) {
        sv_frame_t frame = p.frame;
        sv_frame_t rframe = p.mapframe;
        m_reversePath->add(PathPoint(rframe, frame));
    }

#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::constructReversePath: " << m_reversePath->getPointCount() << " points, at least " << (2 * m_reversePath->getPointCount() * (3 * sizeof(void *) + sizeof(int) + sizeof(PathPoint))) << " bytes" << endl;
#endif
}

sv_frame_t
AlignmentModel::performAlignment(const Path &path, sv_frame_t frame) const
{
    // The path consists of a series of points, each with frame equal
    // to the frame on the source model and mapframe equal to the
    // frame on the target model.  Both should be monotonically
    // increasing.

    const Path::Points &points = path.getPoints();

    if (points.empty()) {
#ifdef DEBUG_ALIGNMENT_MODEL
        cerr << "AlignmentModel::align: No points" << endl;
#endif
        return frame;
    }        

#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::align: frame " << frame << " requested" << endl;
#endif

    PathPoint point(frame);
    Path::Points::const_iterator i = points.lower_bound(point);
    if (i == points.end()) {
#ifdef DEBUG_ALIGNMENT_MODEL
        cerr << "Note: i == points.end()" << endl;
#endif
        --i;
    }
    while (i != points.begin() && i->frame > frame) {
        --i;
    }

    sv_frame_t foundFrame = i->frame;
    sv_frame_t foundMapFrame = i->mapframe;

    sv_frame_t followingFrame = foundFrame;
    sv_frame_t followingMapFrame = foundMapFrame;

    if (++i != points.end()) {
#ifdef DEBUG_ALIGNMENT_MODEL
        cerr << "another point available" << endl;
#endif
        followingFrame = i->frame;
        followingMapFrame = i->mapframe;
    } else {
#ifdef DEBUG_ALIGNMENT_MODEL
        cerr << "no other point available" << endl;
#endif
    }        

#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "foundFrame = " << foundFrame << ", foundMapFrame = " << foundMapFrame
         << ", followingFrame = " << followingFrame << ", followingMapFrame = "
         << followingMapFrame << endl;
#endif
    
    if (foundMapFrame < 0) {
        return 0;
    }

    sv_frame_t resultFrame = foundMapFrame;

    if (followingFrame != foundFrame && frame > foundFrame) {
        double interp =
            double(frame - foundFrame) /
            double(followingFrame - foundFrame);
        resultFrame += lrint(double(followingMapFrame - foundMapFrame) * interp);
    }

#ifdef DEBUG_ALIGNMENT_MODEL
    cerr << "AlignmentModel::align: resultFrame = " << resultFrame << endl;
#endif

    return resultFrame;
}

void
AlignmentModel::setPathFrom(ModelId pathSource)
{
    m_pathSource = pathSource;
    
    auto pathSourceModel =
        ModelById::getAs<SparseTimeValueModel>(m_pathSource);
    
    if (pathSourceModel) {

        connect(pathSourceModel.get(),
                SIGNAL(modelChangedWithin(ModelId, sv_frame_t, sv_frame_t)),
                this, SLOT(pathSourceChangedWithin(ModelId, sv_frame_t, sv_frame_t)));
        
        connect(pathSourceModel.get(), SIGNAL(completionChanged(ModelId)),
                this, SLOT(pathSourceCompletionChanged(ModelId)));

        constructPath();
        constructReversePath();

        if (pathSourceModel->isReady()) {
            pathSourceCompletionChanged(m_pathSource);
        }
    }
}

void
AlignmentModel::setPath(const Path &path)
{
    m_path.reset(new Path(path));
    m_pathComplete = true;
    constructReversePath();
}
    
void
AlignmentModel::toXml(QTextStream &stream,
                      QString indent,
                      QString extraAttributes) const
{
    if (!m_path) {
        SVDEBUG << "AlignmentModel::toXml: no path" << endl;
        return;
    }

    m_path->toXml(stream, indent, "");

    Model::toXml(stream, indent,
                 QString("type=\"alignment\" reference=\"%1\" aligned=\"%2\" path=\"%3\" %4")
                 .arg(ModelById::getExportId(m_reference))
                 .arg(ModelById::getExportId(m_aligned))
                 .arg(m_path->getExportId())
                 .arg(extraAttributes));
}
