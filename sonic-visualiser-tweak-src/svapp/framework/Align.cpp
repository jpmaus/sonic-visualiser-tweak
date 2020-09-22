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

#include "Align.h"
#include "Document.h"

#include "data/model/WaveFileModel.h"
#include "data/model/ReadOnlyWaveFileModel.h"
#include "data/model/AggregateWaveModel.h"
#include "data/model/RangeSummarisableTimeValueModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/AlignmentModel.h"

#include "data/fileio/CSVFileReader.h"

#include "transform/TransformFactory.h"
#include "transform/ModelTransformerFactory.h"
#include "transform/FeatureExtractionModelTransformer.h"

#include <QProcess>
#include <QSettings>
#include <QApplication>

bool
Align::alignModel(Document *doc, ModelId ref, ModelId other, QString &error)
{
    QSettings settings;
    settings.beginGroup("Preferences");
    bool useProgram = settings.value("use-external-alignment", false).toBool();
    QString program = settings.value("external-alignment-program", "").toString();
    settings.endGroup();

    if (useProgram && (program != "")) {
        return alignModelViaProgram(doc, ref, other, program, error);
    } else {
        return alignModelViaTransform(doc, ref, other, error);
    }
}

QString
Align::getAlignmentTransformName()
{
    QSettings settings;
    settings.beginGroup("Alignment");
    TransformId id =
        settings.value("transform-id",
                       "vamp:match-vamp-plugin:match:path").toString();
    settings.endGroup();
    return id;
}

QString
Align::getTuningDifferenceTransformName()
{
    QSettings settings;
    settings.beginGroup("Alignment");
    bool performPitchCompensation =
        settings.value("align-pitch-aware", false).toBool();
    QString id = "";
    if (performPitchCompensation) {
        id = settings.value
            ("tuning-difference-transform-id",
             "vamp:tuning-difference:tuning-difference:tuningfreq")
            .toString();
    }
    settings.endGroup();
    return id;
}

bool
Align::canAlign() 
{
    TransformFactory *factory = TransformFactory::getInstance();
    TransformId id = getAlignmentTransformName();
    TransformId tdId = getTuningDifferenceTransformName();
    return factory->haveTransform(id) &&
        (tdId == "" || factory->haveTransform(tdId));
}

void
Align::abandonOngoingAlignment(ModelId otherId)
{
    auto other = ModelById::getAs<RangeSummarisableTimeValueModel>(otherId);
    if (!other) {
        return;
    }

    ModelId alignmentModelId = other->getAlignment();
    if (alignmentModelId.isNone()) {
        return;
    }

    SVCERR << "Align::abandonOngoingAlignment: An alignment is ongoing for model "
           << otherId << " (alignment model id " << alignmentModelId
           << "), abandoning it..." << endl;
    
    other->setAlignment({});

    for (auto pp: m_pendingProcesses) {
        if (alignmentModelId == pp.second) {
            QProcess *process = pp.first;
            m_pendingProcesses.erase(process);
            SVCERR << "Align::abandonOngoingAlignment: Killing external "
                   << "alignment process " << process << "..." << endl;
            delete process; // kills the process itself
            break;
        }
    }

    if (m_pendingAlignments.find(alignmentModelId) !=
        m_pendingAlignments.end()) {
        SVCERR << "Align::abandonOngoingAlignment: Releasing path output model "
               << m_pendingAlignments[alignmentModelId]
               << "..." << endl;
        ModelById::release(m_pendingAlignments[alignmentModelId]);
        SVCERR << "Align::abandonOngoingAlignment: Dropping alignment model "
               << alignmentModelId
               << " from pending alignments..." << endl;
        m_pendingAlignments.erase(alignmentModelId);
    }

    for (auto ptd: m_pendingTuningDiffs) {
        if (alignmentModelId == ptd.second.alignment) {
            SVCERR << "Align::abandonOngoingAlignment: Releasing preparatory model "
                   << ptd.second.preparatory << "..." << endl;
            ModelById::release(ptd.second.preparatory);
            SVCERR << "Align::abandonOngoingAlignment: Releasing pending tuning-diff model "
                   << ptd.first << "..." << endl;
            ModelById::release(ptd.first);
            SVCERR << "Align::abandonOngoingAlignment: Dropping tuning-diff model "
                   << ptd.first
                   << " from pending tuning diffs..." << endl;
            m_pendingTuningDiffs.erase(ptd.first);
            break;
        }
    }

    SVCERR << "Align::abandonOngoingAlignment: done" << endl;
}

bool
Align::alignModelViaTransform(Document *doc,
                              ModelId referenceId,
                              ModelId otherId,
                              QString &error)
{
    QMutexLocker locker (&m_mutex);

    auto reference =
        ModelById::getAs<RangeSummarisableTimeValueModel>(referenceId);
    auto other =
        ModelById::getAs<RangeSummarisableTimeValueModel>(otherId);

    if (!reference || !other) return false;

    // There may be an alignment already happening; we should stop it,
    // which we can do by discarding the output models for its
    // transforms
    abandonOngoingAlignment(otherId);
    
    // This involves creating a number of new models:
    //
    // 1. an AggregateWaveModel to provide the mixdowns of the main
    // model and the new model in its two channels, as input to the
    // MATCH plugin. We just call this one aggregateModel
    //
    // 2a. a SparseTimeValueModel which will be automatically created
    // by FeatureExtractionModelTransformer when running the
    // TuningDifference plugin to receive the relative tuning of the
    // second model (if pitch-aware alignment is enabled in the
    // preferences). We call this tuningDiffOutputModel.
    //
    // 2b. a SparseTimeValueModel which will be automatically created
    // by FeatureExtractionPluginTransformer when running the MATCH
    // plugin to perform alignment (so containing the alignment path).
    // We call this one pathOutputModel.
    //
    // 2c. a SparseTimeValueModel used solely to provide faked
    // completion information to the AlignmentModel while a
    // TuningDifference calculation is going on. We call this
    // preparatoryModel.
    //
    // 3. an AlignmentModel, which stores the path and carries out
    // alignment lookups on it. We just call this one alignmentModel.
    //
    // Models 1 and 3 are registered with the document, which will
    // eventually release them. We don't release them here except in
    // the case where an activity fails before the point where we
    // would otherwise have registered them with the document.
    //
    // Models 2a (tuningDiffOutputModel), 2b (pathOutputModel) and 2c
    // (preparatoryModel) are not registered with the document. Model
    // 2b (pathOutputModel) is not registered because we do not have a
    // stable reference to the document at the point where it is
    // created. Model 2c (preparatoryModel) is not registered because
    // it is a bodge that we are embarrassed about, so we try to
    // manage it ourselves without anyone else noticing. Model 2a is
    // not registered for symmetry with the other two. These have to
    // be released by us when finished with, but their lifespans do
    // not extend beyond the end of the alignment procedure, so this
    // should be ok.

    AggregateWaveModel::ChannelSpecList components;

    components.push_back
        (AggregateWaveModel::ModelChannelSpec(referenceId, -1));

    components.push_back
        (AggregateWaveModel::ModelChannelSpec(otherId, -1));

    auto aggregateModel = std::make_shared<AggregateWaveModel>(components);
    auto aggregateModelId = ModelById::add(aggregateModel);
    doc->addNonDerivedModel(aggregateModelId);

    auto alignmentModel = std::make_shared<AlignmentModel>
        (referenceId, otherId, ModelId());
    auto alignmentModelId = ModelById::add(alignmentModel);

    TransformId tdId = getTuningDifferenceTransformName();

    if (tdId == "") {
        
        if (beginTransformDrivenAlignment(aggregateModelId,
                                          alignmentModelId)) {
            other->setAlignment(alignmentModelId);
            doc->addNonDerivedModel(alignmentModelId);
        } else {
            error = alignmentModel->getError();
            ModelById::release(alignmentModel);
            return false;
        }

    } else {

        // Have a tuning-difference transform id, so run it
        // asynchronously first
        
        TransformFactory *tf = TransformFactory::getInstance();

        Transform transform = tf->getDefaultTransformFor
            (tdId, aggregateModel->getSampleRate());

        transform.setParameter("maxduration", 60);
        transform.setParameter("maxrange", 6);
        transform.setParameter("finetuning", false);
    
        SVDEBUG << "Align::alignModel: Tuning difference transform step size " << transform.getStepSize() << ", block size " << transform.getBlockSize() << endl;

        ModelTransformerFactory *mtf = ModelTransformerFactory::getInstance();

        QString message;
        ModelId tuningDiffOutputModelId = mtf->transform(transform,
                                                         aggregateModelId,
                                                         message);

        auto tuningDiffOutputModel =
            ModelById::getAs<SparseTimeValueModel>(tuningDiffOutputModelId);
        if (!tuningDiffOutputModel) {
            SVCERR << "Align::alignModel: ERROR: Failed to create tuning-difference output model (no Tuning Difference plugin?)" << endl;
            error = message;
            ModelById::release(alignmentModel);
            return false;
        }

        other->setAlignment(alignmentModelId);
        doc->addNonDerivedModel(alignmentModelId);
    
        connect(tuningDiffOutputModel.get(),
                SIGNAL(completionChanged(ModelId)),
                this, SLOT(tuningDifferenceCompletionChanged(ModelId)));

        TuningDiffRec rec;
        rec.input = aggregateModelId;
        rec.alignment = alignmentModelId;
        
        // This model exists only so that the AlignmentModel can get a
        // completion value from somewhere while the tuning difference
        // calculation is going on
        auto preparatoryModel = std::make_shared<SparseTimeValueModel>
            (aggregateModel->getSampleRate(), 1);
        auto preparatoryModelId = ModelById::add(preparatoryModel);
        preparatoryModel->setCompletion(0);
        rec.preparatory = preparatoryModelId;
        alignmentModel->setPathFrom(rec.preparatory);
        
        m_pendingTuningDiffs[tuningDiffOutputModelId] = rec;

        SVDEBUG << "Align::alignModelViaTransform: Made a note of pending tuning diff output model id " << tuningDiffOutputModelId << " with input " << rec.input << ", alignment model " << rec.alignment << ", preparatory model " << rec.preparatory << endl;
    }

    return true;
}

void
Align::tuningDifferenceCompletionChanged(ModelId tuningDiffOutputModelId)
{
    QMutexLocker locker(&m_mutex);

    if (m_pendingTuningDiffs.find(tuningDiffOutputModelId) ==
        m_pendingTuningDiffs.end()) {
        SVDEBUG << "NOTE: Align::tuningDifferenceCompletionChanged: Model "
                << tuningDiffOutputModelId
                << " not found in pending tuning diff map, presuming "
                << "completed or abandoned" << endl;
        return;
    }

    auto tuningDiffOutputModel =
        ModelById::getAs<SparseTimeValueModel>(tuningDiffOutputModelId);
    if (!tuningDiffOutputModel) {
        SVCERR << "WARNING: Align::tuningDifferenceCompletionChanged: Model "
               << tuningDiffOutputModelId
               << " not known as SparseTimeValueModel" << endl;
        return;
    }

    TuningDiffRec rec = m_pendingTuningDiffs[tuningDiffOutputModelId];

    auto alignmentModel = ModelById::getAs<AlignmentModel>(rec.alignment);
    if (!alignmentModel) {
        SVCERR << "WARNING: Align::tuningDifferenceCompletionChanged:"
               << "alignment model has disappeared" << endl;
        return;
    }
    
    int completion = 0;
    bool done = tuningDiffOutputModel->isReady(&completion);

    if (!done) {
        // This will be the completion the alignment model reports,
        // before the alignment actually begins. It goes up from 0 to
        // 99 (not 100!) and then back to 0 again when we start
        // calculating the actual path in the following phase
        int clamped = (completion == 100 ? 99 : completion);
        auto preparatoryModel =
            ModelById::getAs<SparseTimeValueModel>(rec.preparatory);
        if (preparatoryModel) {
            preparatoryModel->setCompletion(clamped);
        }
        return;
    }

    float tuningFrequency = 440.f;
    
    if (!tuningDiffOutputModel->isEmpty()) {
        tuningFrequency = tuningDiffOutputModel->getAllEvents()[0].getValue();
        SVCERR << "Align::tuningDifferenceCompletionChanged: Reported tuning frequency = " << tuningFrequency << endl;
    } else {
        SVCERR << "Align::tuningDifferenceCompletionChanged: No tuning frequency reported" << endl;
    }    
    
    ModelById::release(tuningDiffOutputModel);
    
    alignmentModel->setPathFrom({}); // replace preparatoryModel
    ModelById::release(rec.preparatory);
    rec.preparatory = {};
    
    m_pendingTuningDiffs.erase(tuningDiffOutputModelId);

    SVDEBUG << "Align::tuningDifferenceCompletionChanged: Erasing model "
            << tuningDiffOutputModelId << " from pending tuning diffs and "
            << "launching the alignment phase for alignment model "
            << rec.alignment << " with tuning frequency "
            << tuningFrequency << endl;
    
    beginTransformDrivenAlignment
        (rec.input, rec.alignment, tuningFrequency);
}

bool
Align::beginTransformDrivenAlignment(ModelId aggregateModelId,
                                     ModelId alignmentModelId,
                                     float tuningFrequency)
{
    TransformId id = getAlignmentTransformName();
    
    TransformFactory *tf = TransformFactory::getInstance();

    auto aggregateModel =
        ModelById::getAs<AggregateWaveModel>(aggregateModelId);
    auto alignmentModel =
        ModelById::getAs<AlignmentModel>(alignmentModelId);

    if (!aggregateModel || !alignmentModel) {
        SVCERR << "Align::alignModel: ERROR: One or other of the aggregate & alignment models has disappeared" << endl;
        return false;
    }
    
    Transform transform = tf->getDefaultTransformFor
        (id, aggregateModel->getSampleRate());

    transform.setStepSize(transform.getBlockSize()/2);
    transform.setParameter("serialise", 1);
    transform.setParameter("smooth", 0);
    transform.setParameter("zonewidth", 40);
    transform.setParameter("noise", true);
    transform.setParameter("minfreq", 500);

    int cents = 0;
    
    if (tuningFrequency != 0.f) {
        transform.setParameter("freq2", tuningFrequency);

        double centsOffset = 0.f;
        int pitch = Pitch::getPitchForFrequency(tuningFrequency, &centsOffset);
        cents = int(round((pitch - 69) * 100 + centsOffset));
        SVCERR << "frequency " << tuningFrequency << " yields cents offset " << centsOffset << " and pitch " << pitch << " -> cents " << cents << endl;
    }

    alignmentModel->setRelativePitch(cents);
    
    SVDEBUG << "Align::alignModel: Alignment transform step size " << transform.getStepSize() << ", block size " << transform.getBlockSize() << endl;

    ModelTransformerFactory *mtf = ModelTransformerFactory::getInstance();

    QString message;
    ModelId pathOutputModelId = mtf->transform
        (transform, aggregateModelId, message);

    if (pathOutputModelId.isNone()) {
        transform.setStepSize(0);
        pathOutputModelId = mtf->transform
            (transform, aggregateModelId, message);
    }

    auto pathOutputModel =
        ModelById::getAs<SparseTimeValueModel>(pathOutputModelId);

    //!!! callers will need to be updated to get error from
    //!!! alignment model after initial call
        
    if (!pathOutputModel) {
        SVCERR << "Align::alignModel: ERROR: Failed to create alignment path (no MATCH plugin?)" << endl;
        alignmentModel->setError(message);
        return false;
    }

    pathOutputModel->setCompletion(0);
    alignmentModel->setPathFrom(pathOutputModelId);

    m_pendingAlignments[alignmentModelId] = pathOutputModelId;

    connect(alignmentModel.get(), SIGNAL(completionChanged(ModelId)),
            this, SLOT(alignmentCompletionChanged(ModelId)));

    return true;
}

void
Align::alignmentCompletionChanged(ModelId alignmentModelId)
{
    QMutexLocker locker (&m_mutex);

    auto alignmentModel = ModelById::getAs<AlignmentModel>(alignmentModelId);

    if (alignmentModel && alignmentModel->isReady()) {

        if (m_pendingAlignments.find(alignmentModelId) !=
            m_pendingAlignments.end()) {
            ModelId pathOutputModelId = m_pendingAlignments[alignmentModelId];
            ModelById::release(pathOutputModelId);
            m_pendingAlignments.erase(alignmentModelId);
        }
        
        disconnect(alignmentModel.get(),
                   SIGNAL(completionChanged(ModelId)),
                   this, SLOT(alignmentCompletionChanged(ModelId)));
        emit alignmentComplete(alignmentModelId);
    }
}

bool
Align::alignModelViaProgram(Document *doc,
                            ModelId referenceId,
                            ModelId otherId,
                            QString program,
                            QString &error)
{
    // Run an external program, passing to it paths to the main
    // model's audio file and the new model's audio file. It returns
    // the path in CSV form through stdout.

    auto reference = ModelById::getAs<ReadOnlyWaveFileModel>(referenceId);
    auto other = ModelById::getAs<ReadOnlyWaveFileModel>(otherId);
    if (!reference || !other) {
        SVCERR << "ERROR: Align::alignModelViaProgram: Can't align non-read-only models via program (no local filename available)" << endl;
        return false;
    }

    while (!reference->isReady(nullptr) || !other->isReady(nullptr)) {
        qApp->processEvents();
    }
    
    QString refPath = reference->getLocalFilename();
    if (refPath == "") {
        refPath = FileSource(reference->getLocation()).getLocalFilename();
    }
    
    QString otherPath = other->getLocalFilename();
    if (otherPath == "") {
        otherPath = FileSource(other->getLocation()).getLocalFilename();
    }

    if (refPath == "" || otherPath == "") {
        error = "Failed to find local filepath for wave-file model";
        return false;
    }

    QProcess *process = nullptr;
    ModelId alignmentModelId = {};
    
    {
        QMutexLocker locker (&m_mutex);

        auto alignmentModel =
            std::make_shared<AlignmentModel>(referenceId, otherId, ModelId());

        alignmentModelId = ModelById::add(alignmentModel);
        other->setAlignment(alignmentModelId);

        process = new QProcess;
        process->setProcessChannelMode(QProcess::ForwardedErrorChannel);

        connect(process,
                SIGNAL(finished(int, QProcess::ExitStatus)),
                this,
                SLOT(alignmentProgramFinished(int, QProcess::ExitStatus)));

        m_pendingProcesses[process] = alignmentModelId;
    }

    QStringList args;
    args << refPath << otherPath;

    SVCERR << "Align::alignModelViaProgram: Starting program \""
           << program << "\" with args: ";
    for (auto a: args) {
        SVCERR << "\"" << a << "\" ";
    }
    SVCERR << endl;

    process->start(program, args);

    bool success = process->waitForStarted();

    {
        QMutexLocker locker(&m_mutex);
        
        if (!success) {
        
            SVCERR << "ERROR: Align::alignModelViaProgram: "
                   << "Program did not start" << endl;
            error = "Alignment program \"" + program + "\" did not start";
        
            m_pendingProcesses.erase(process);
            other->setAlignment({});
            ModelById::release(alignmentModelId);
            delete process;
        
        } else {
            doc->addNonDerivedModel(alignmentModelId);
        }
    }

    return success;
}

void
Align::alignmentProgramFinished(int exitCode, QProcess::ExitStatus status)
{
    QMutexLocker locker (&m_mutex);
    
    SVCERR << "Align::alignmentProgramFinished" << endl;
    
    QProcess *process = qobject_cast<QProcess *>(sender());

    if (m_pendingProcesses.find(process) == m_pendingProcesses.end()) {
        SVCERR << "ERROR: Align::alignmentProgramFinished: Process " << process
               << " not found in process model map!" << endl;
        return;
    }

    ModelId alignmentModelId = m_pendingProcesses[process];
    auto alignmentModel = ModelById::getAs<AlignmentModel>(alignmentModelId);
    if (!alignmentModel) return;
    
    if (exitCode == 0 && status == 0) {

        CSVFormat format;
        format.setModelType(CSVFormat::TwoDimensionalModel);
        format.setTimingType(CSVFormat::ExplicitTiming);
        format.setTimeUnits(CSVFormat::TimeSeconds);
        format.setColumnCount(2);
        // The output format has time in the reference file first, and
        // time in the "other" file in the second column. This is a
        // more natural approach for a command-line alignment tool,
        // but it's the opposite of what we expect for native
        // alignment paths, which map from "other" file to
        // reference. These column purpose settings reflect that.
        format.setColumnPurpose(1, CSVFormat::ColumnStartTime);
        format.setColumnPurpose(0, CSVFormat::ColumnValue);
        format.setAllowQuoting(false);
        format.setSeparator(',');

        CSVFileReader reader(process, format, alignmentModel->getSampleRate());
        if (!reader.isOK()) {
            SVCERR << "ERROR: Align::alignmentProgramFinished: Failed to parse output"
                   << endl;
            alignmentModel->setError
                (QString("Failed to parse output of program: %1")
                 .arg(reader.getError()));
            goto done;
        }

        //!!! to use ById?
        
        Model *csvOutput = reader.load();

        SparseTimeValueModel *path =
            qobject_cast<SparseTimeValueModel *>(csvOutput);
        if (!path) {
            SVCERR << "ERROR: Align::alignmentProgramFinished: Output did not convert to sparse time-value model"
                   << endl;
            alignmentModel->setError
                ("Output of program did not produce sparse time-value model");
            delete csvOutput;
            goto done;
        }
                       
        if (path->isEmpty()) {
            SVCERR << "ERROR: Align::alignmentProgramFinished: Output contained no mappings"
                   << endl;
            alignmentModel->setError
                ("Output of alignment program contained no mappings");
            delete path;
            goto done;
        }

        SVCERR << "Align::alignmentProgramFinished: Setting alignment path ("
             << path->getEventCount() << " point(s))" << endl;

        auto pathId =
            ModelById::add(std::shared_ptr<SparseTimeValueModel>(path));
        alignmentModel->setPathFrom(pathId);

        emit alignmentComplete(alignmentModelId);

        ModelById::release(pathId);
        
    } else {
        SVCERR << "ERROR: Align::alignmentProgramFinished: Aligner program "
               << "failed: exit code " << exitCode << ", status " << status
               << endl;
        alignmentModel->setError
            ("Aligner process returned non-zero exit status");
    }

done:
    m_pendingProcesses.erase(process);
    delete process;
}

