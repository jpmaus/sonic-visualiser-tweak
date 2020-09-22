/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "MainWindowBase.h"
#include "Document.h"

#include "view/Pane.h"
#include "view/PaneStack.h"
#include "data/model/ReadOnlyWaveFileModel.h"
#include "data/model/WritableWaveFileModel.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/NoteModel.h"
#include "data/model/Labeller.h"
#include "data/model/TabularModel.h"
#include "view/ViewManager.h"

#include "layer/WaveformLayer.h"
#include "layer/TimeRulerLayer.h"
#include "layer/TimeInstantLayer.h"
#include "layer/TimeValueLayer.h"
#include "layer/Colour3DPlotLayer.h"
#include "layer/SliceLayer.h"
#include "layer/SliceableLayer.h"
#include "layer/ImageLayer.h"
#include "layer/NoteLayer.h"
#include "layer/FlexiNoteLayer.h"
#include "layer/RegionLayer.h"

#include "widgets/ListInputDialog.h"
#include "widgets/CommandHistory.h"
#include "widgets/ProgressDialog.h"
#include "widgets/MIDIFileImportDialog.h"
#include "widgets/CSVFormatDialog.h"
#include "widgets/ModelDataTableDialog.h"
#include "widgets/InteractiveFileFinder.h"

#include "audio/AudioCallbackPlaySource.h"
#include "audio/AudioCallbackRecordTarget.h"
#include "audio/PlaySpeedRangeMapper.h"

#include "data/fileio/DataFileReaderFactory.h"
#include "data/fileio/PlaylistFileReader.h"
#include "data/fileio/WavFileWriter.h"
#include "data/fileio/MIDIFileWriter.h"
#include "data/fileio/CSVFileWriter.h"
#include "data/fileio/BZipFileDevice.h"
#include "data/fileio/FileSource.h"
#include "data/fileio/AudioFileReaderFactory.h"
#include "rdf/RDFImporter.h"
#include "rdf/RDFExporter.h"

#include "base/RecentFiles.h"

#include "base/XmlExportable.h"
#include "base/Profiler.h"
#include "base/Preferences.h"
#include "base/TempWriteFile.h"
#include "base/Exceptions.h"
#include "base/ResourceFinder.h"

#include "data/osc/OSCQueue.h"
#include "data/midi/MIDIInput.h"
#include "OSCScript.h"

#include "system/System.h"

#include <bqaudioio/SystemPlaybackTarget.h>
#include <bqaudioio/SystemAudioIO.h>
#include <bqaudioio/AudioFactory.h>
#include <bqaudioio/ResamplerWrapper.h>

#include <QApplication>
#include <QMessageBox>
#include <QGridLayout>
#include <QLabel>
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QInputDialog>
#include <QStatusBar>
#include <QTreeView>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QTextCodec>
#include <QProcess>
#include <QShortcut>
#include <QSettings>
#include <QDateTime>
#include <QProcess>
#include <QCheckBox>
#include <QRegExp>
#include <QScrollArea>
#include <QScreen>
#include <QSignalMapper>

#include <iostream>
#include <cstdio>
#include <errno.h>

using std::vector;
using std::map;
using std::set;

#ifdef Q_WS_X11
#define Window X11Window
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/SM/SMlib.h>

static int handle_x11_error(Display *dpy, XErrorEvent *err)
{
    char errstr[256];
    XGetErrorText(dpy, err->error_code, errstr, 256);
    if (err->error_code != BadWindow) {
        cerr << "Sonic Visualiser: X Error: "
             << errstr << " " << int(err->error_code)
             << "\nin major opcode:  "
             << int(err->request_code) << endl;
    }
    return 0;
}
#undef Window
#endif

MainWindowBase::MainWindowBase(AudioMode audioMode,
                               MIDIMode midiMode,
                               PaneStack::Options paneStackOptions) :
    m_document(nullptr),
    m_paneStack(nullptr),
    m_viewManager(nullptr),
    m_timeRulerLayer(nullptr),
    m_audioMode(audioMode),
    m_midiMode(midiMode),
    m_playSource(nullptr),
    m_recordTarget(nullptr),
    m_resamplerWrapper(nullptr),
    m_playTarget(nullptr),
    m_audioIO(nullptr),
    m_oscQueue(nullptr),
    m_oscQueueStarter(nullptr),
    m_oscScript(nullptr),
    m_midiInput(nullptr),
    m_recentFiles("RecentFiles", 20),
    m_recentTransforms("RecentTransforms", 20),
    m_documentModified(false),
    m_openingAudioFile(false),
    m_abandoning(false),
    m_labeller(nullptr),
    m_lastPlayStatusSec(0),
    m_initialDarkBackground(false),
    m_defaultFfwdRwdStep(2, 0),
    m_audioRecordMode(RecordCreateAdditionalModel),
    m_statusLabel(nullptr),
    m_iconsVisibleInMenus(true),
    m_menuShortcutMapper(nullptr)
{
    Profiler profiler("MainWindowBase::MainWindowBase");

    SVDEBUG << "MainWindowBase::MainWindowBase" << endl;

    qRegisterMetaType<sv_frame_t>("sv_frame_t");
    qRegisterMetaType<sv_samplerate_t>("sv_samplerate_t");
    qRegisterMetaType<ModelId>("ModelId");

#ifdef Q_WS_X11
    XSetErrorHandler(handle_x11_error);
#endif

    connect(this, SIGNAL(hideSplash()), this, SLOT(emitHideSplash()));
    
    connect(CommandHistory::getInstance(), SIGNAL(commandExecuted()),
            this, SLOT(documentModified()));
    connect(CommandHistory::getInstance(), SIGNAL(documentRestored()),
            this, SLOT(documentRestored()));
    
    SVDEBUG << "MainWindowBase: Creating view manager" << endl;

    m_viewManager = new ViewManager();
    connect(m_viewManager, SIGNAL(selectionChanged()),
            this, SLOT(updateMenuStates()));
    connect(m_viewManager, SIGNAL(inProgressSelectionChanged()),
            this, SLOT(inProgressSelectionChanged()));

    SVDEBUG << "MainWindowBase: Calculating view font size" << endl;

    // set a sensible default font size for views -- cannot do this
    // in Preferences, which is in base and not supposed to use QtGui
    int viewFontSize = int(QApplication::font().pointSize() * 0.9);
    QSettings settings;
    settings.beginGroup("Preferences");
    viewFontSize = settings.value("view-font-size", viewFontSize).toInt();
    settings.setValue("view-font-size", viewFontSize);
    settings.endGroup();

    SVDEBUG << "MainWindowBase: View font size is " << viewFontSize << endl;

#ifdef NOT_DEFINED // This no longer works correctly on any platform AFAICS
    Preferences::BackgroundMode mode =
            Preferences::getInstance()->getBackgroundMode();
    m_initialDarkBackground = m_viewManager->getGlobalDarkBackground();
    if (mode != Preferences::BackgroundFromTheme) {
        m_viewManager->setGlobalDarkBackground
                (mode == Preferences::DarkBackground);
    }
#endif

    m_paneStack = new PaneStack(nullptr, m_viewManager, paneStackOptions);
    connect(m_paneStack, SIGNAL(currentPaneChanged(Pane *)),
            this, SLOT(currentPaneChanged(Pane *)));
    connect(m_paneStack, SIGNAL(currentLayerChanged(Pane *, Layer *)),
            this, SLOT(currentLayerChanged(Pane *, Layer *)));
    connect(m_paneStack, SIGNAL(rightButtonMenuRequested(Pane *, QPoint)),
            this, SLOT(rightButtonMenuRequested(Pane *, QPoint)));
    connect(m_paneStack, SIGNAL(contextHelpChanged(const QString &)),
            this, SLOT(contextHelpChanged(const QString &)));
    connect(m_paneStack, SIGNAL(paneAdded(Pane *)),
            this, SLOT(paneAdded(Pane *)));
    connect(m_paneStack, SIGNAL(paneHidden(Pane *)),
            this, SLOT(paneHidden(Pane *)));
    connect(m_paneStack, SIGNAL(paneAboutToBeDeleted(Pane *)),
            this, SLOT(paneAboutToBeDeleted(Pane *)));
    connect(m_paneStack, SIGNAL(dropAccepted(Pane *, QStringList)),
            this, SLOT(paneDropAccepted(Pane *, QStringList)));
    connect(m_paneStack, SIGNAL(dropAccepted(Pane *, QString)),
            this, SLOT(paneDropAccepted(Pane *, QString)));
    connect(m_paneStack, SIGNAL(paneDeleteButtonClicked(Pane *)),
            this, SLOT(paneDeleteButtonClicked(Pane *)));

    SVDEBUG << "MainWindowBase: Creating play source" << endl;
    
    m_playSource = new AudioCallbackPlaySource
            (m_viewManager, QApplication::applicationName());

    if (m_audioMode == AUDIO_PLAYBACK_NOW_RECORD_LATER ||
            m_audioMode == AUDIO_PLAYBACK_AND_RECORD) {
        SVDEBUG << "MainWindowBase: Creating record target" << endl;
        m_recordTarget = new AudioCallbackRecordTarget
                (m_viewManager, QApplication::applicationName());
        connect(m_recordTarget,
                SIGNAL(recordDurationChanged(sv_frame_t, sv_samplerate_t)),
                this,
                SLOT(recordDurationChanged(sv_frame_t, sv_samplerate_t)));
    }

    connect(m_playSource, SIGNAL(sampleRateMismatch(sv_samplerate_t, sv_samplerate_t, bool)),
            this,           SLOT(sampleRateMismatch(sv_samplerate_t, sv_samplerate_t, bool)));
    connect(m_playSource, SIGNAL(channelCountIncreased(int)),
            this,           SLOT(audioChannelCountIncreased(int)));
    connect(m_playSource, SIGNAL(audioOverloadPluginDisabled()),
            this,           SLOT(audioOverloadPluginDisabled()));
    connect(m_playSource, SIGNAL(audioTimeStretchMultiChannelDisabled()),
            this,           SLOT(audioTimeStretchMultiChannelDisabled()));

    connect(m_viewManager, SIGNAL(monitoringLevelsChanged(float, float)),
            this, SLOT(monitoringLevelsChanged(float, float)));

    connect(m_viewManager, SIGNAL(playbackFrameChanged(sv_frame_t)),
            this, SLOT(playbackFrameChanged(sv_frame_t)));

    connect(m_viewManager, SIGNAL(globalCentreFrameChanged(sv_frame_t)),
            this, SLOT(globalCentreFrameChanged(sv_frame_t)));

    connect(m_viewManager, SIGNAL(viewCentreFrameChanged(View *, sv_frame_t)),
            this, SLOT(viewCentreFrameChanged(View *, sv_frame_t)));

    connect(m_viewManager, SIGNAL(viewZoomLevelChanged(View *, ZoomLevel, bool)),
            this, SLOT(viewZoomLevelChanged(View *, ZoomLevel, bool)));

    connect(Preferences::getInstance(),
            SIGNAL(propertyChanged(PropertyContainer::PropertyName)),
            this,
            SLOT(preferenceChanged(PropertyContainer::PropertyName)));

    SVDEBUG << "MainWindowBase: Creating labeller" << endl;

    Labeller::ValueType labellerType = Labeller::ValueFromTwoLevelCounter;
    settings.beginGroup("MainWindow");

    labellerType = (Labeller::ValueType)
            settings.value("labellertype", (int)labellerType).toInt();
    int cycle = settings.value("labellercycle", 4).toInt();

    settings.endGroup();

    m_labeller = new Labeller(labellerType);
    m_labeller->setCounterCycleSize(cycle);

    if (m_midiMode == MIDI_LISTEN) {
        SVDEBUG << "MainWindowBase: Creating MIDI input" << endl;
        m_midiInput = new MIDIInput(QApplication::applicationName(), this);
    }

    QTimer::singleShot(1500, this, SIGNAL(hideSplash()));

    SVDEBUG << "MainWindowBase: Constructor done" << endl;
}

MainWindowBase::~MainWindowBase()
{
    SVDEBUG << "MainWindowBase::~MainWindowBase" << endl;

    // We have to delete the breakfastquay::SystemPlaybackTarget or
    // breakfastquay::SystemAudioIO object (whichever we have -- it
    // depends on whether we handle recording or not) before we delete
    // the ApplicationPlaybackSource and ApplicationRecordTarget that
    // they refer to.

    deleteAudioIO();

    // Then delete the Application objects.
    delete m_playSource;
    delete m_recordTarget;
    
    delete m_viewManager;
    delete m_midiInput;

    if (m_oscScript) {
        disconnect(m_oscScript, nullptr, nullptr, nullptr);
        m_oscScript->abandon();
        m_oscScript->wait(1000);
        if (m_oscScript->isRunning()) {
            m_oscScript->terminate();
            m_oscScript->wait(1000);
        }
        delete m_oscScript;
    }

    if (m_oscQueueStarter) {
        disconnect(m_oscQueueStarter, nullptr, nullptr, nullptr);
        m_oscQueueStarter->wait(1000);
        if (m_oscQueueStarter->isRunning()) {
            m_oscQueueStarter->terminate();
            m_oscQueueStarter->wait(1000);
        }
        delete m_oscQueueStarter;
        delete m_oscQueue;
    }
    
    Profiles::getInstance()->dump();
}

void
MainWindowBase::emitHideSplash()
{
    SVDEBUG << "MainWindowBase: Hiding splash screen" << endl;
    emit hideSplash(this);
}

void
MainWindowBase::finaliseMenus()
{
    SVDEBUG << "MainWindowBase::finaliseMenus called" << endl;

    delete m_menuShortcutMapper;
    m_menuShortcutMapper = nullptr;

    foreach (QShortcut *sc, m_appShortcuts) {
        delete sc;
    }
    m_appShortcuts.clear();

    QMenuBar *mb = menuBar();

    // This used to find all children of QMenu type, and call
    // finaliseMenu on those. But it seems we are getting hold of some
    // menus that way that are not actually active in the menu bar and
    // are not returned in their parent menu's actions() list, and if
    // we finalise those, we end up with duplicate shortcuts in the
    // app shortcut mapper. So we should do this by descending the
    // menu tree through only those menus accessible via actions()
    // from their parents instead.

    QList<QMenu *> menus = mb->findChildren<QMenu *>
            (QString(), Qt::FindDirectChildrenOnly);

    foreach (QMenu *menu, menus) {
        if (menu) finaliseMenu(menu);
    }

    SVDEBUG << "MainWindowBase::finaliseMenus done" << endl;
}

void
MainWindowBase::finaliseMenu(QMenu *menu)
{
    foreach (QAction *a, menu->actions()) {
        a->setIconVisibleInMenu(m_iconsVisibleInMenus);
    }

#ifdef Q_OS_MAC
    // See https://bugreports.qt-project.org/browse/QTBUG-38256 and
    // our issue #890 http://code.soundsoftware.ac.uk/issues/890 --
    // single-key shortcuts that are associated only with a menu
    // action (and not with a toolbar button) do not work with Qt 5.x
    // under OS/X.
    //
    // Apparently Cocoa never handled them as a matter of course, but
    // earlier versions of Qt picked them up as widget shortcuts and
    // handled them anyway. That behaviour was removed to fix a crash
    // when invoking a menu while its window was overridden by a modal
    // dialog (https://bugreports.qt-project.org/browse/QTBUG-30657).
    //
    // This workaround restores the single-key shortcut behaviour by
    // searching in menus for single-key shortcuts that are associated
    // only with the menu and not with a toolbar button, and
    // augmenting them with global application shortcuts that invoke
    // the relevant actions, testing whether the actions are enabled
    // on invocation.
    //
    // (Previously this acted on all single-key shortcuts in menus,
    // and it removed the shortcut from the action when it created
    // each new global one, in order to avoid an "ambiguous shortcut"
    // error in the case where the action was also associated with a
    // toolbar button. But that has the unwelcome side-effect of
    // removing the shortcut hint from the menu entry. So now we leave
    // the shortcut in the menu action as well as creating a global
    // one, and we only act on shortcuts that have no toolbar button,
    // i.e. that will not otherwise work. The downside is that if this
    // bug is fixed in a future Qt release, we will start getting
    // "ambiguous shortcut" errors from the menu entry actions and
    // will need to update the code.)

    // Update: The bug was fixed in Qt 5.4 for shortcuts with no
    // modifier, and I believe it is fixed in Qt 5.5 for shortcuts
    // with Shift modifiers. The below reflects that

#if (QT_VERSION < QT_VERSION_CHECK(5, 5, 0))

    if (!m_menuShortcutMapper) {
        m_menuShortcutMapper = new QSignalMapper(this);
        connect(m_menuShortcutMapper, SIGNAL(mapped(QObject *)),
                this, SLOT(menuActionMapperInvoked(QObject *)));
    }

    foreach (QAction *a, menu->actions()) {

        if (a->isSeparator()) {
            continue;
        } else if (a->menu()) {
            finaliseMenu(a->menu());
        } else {

            QWidgetList ww = a->associatedWidgets();
            bool hasButton = false;
            foreach (QWidget *w, ww) {
                if (qobject_cast<QAbstractButton *>(w)) {
                    hasButton = true;
                    break;
                }
            }
            if (hasButton) continue;
            QKeySequence sc = a->shortcut();

            // Note that the set of "single-key shortcuts" that aren't
            // working and that we need to handle here includes those
            // with the Shift modifier mask as well as those with no
            // modifier at all
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
            // Nothing needed
            if (false) {
#elif (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
            if (sc.count() == 1 &&
                    (sc[0] & Qt::KeyboardModifierMask) == Qt::ShiftModifier) {
#else
            if (sc.count() == 1 &&
                    ((sc[0] & Qt::KeyboardModifierMask) == Qt::NoModifier ||
                     (sc[0] & Qt::KeyboardModifierMask) == Qt::ShiftModifier)) {
#endif
                QShortcut *newSc = new QShortcut(sc, a->parentWidget());
                QObject::connect(newSc, SIGNAL(activated()),
                                 m_menuShortcutMapper, SLOT(map()));
                m_menuShortcutMapper->setMapping(newSc, a);
                m_appShortcuts.push_back(newSc);
            }
        }
    }
#endif
#endif
}

void
MainWindowBase::menuActionMapperInvoked(QObject *o)
{
    QAction *a = qobject_cast<QAction *>(o);
    if (a && a->isEnabled()) {
        a->trigger();
    }
}

void
MainWindowBase::resizeConstrained(QSize size)
{
    QScreen *screen = QApplication::primaryScreen();
    QRect available = screen->availableGeometry();
    QSize actual(std::min(size.width(), available.width()),
                 std::min(size.height(), available.height()));
    resize(actual);
}

void
MainWindowBase::startOSCQueue(bool withNetworkPort)
{
    m_oscQueueStarter = new OSCQueueStarter(this, withNetworkPort);
    connect(m_oscQueueStarter, SIGNAL(finished()), this, SLOT(oscReady()));
    m_oscQueueStarter->start();
}

void
MainWindowBase::oscReady()
{
    if (m_oscQueue && m_oscQueue->isOK()) {
        connect(m_oscQueue, SIGNAL(messagesAvailable()), this, SLOT(pollOSC()));
        QTimer *oscTimer = new QTimer(this);
        connect(oscTimer, SIGNAL(timeout()), this, SLOT(pollOSC()));
        oscTimer->start(1000);

        if (m_oscQueue->hasPort()) {
            SVDEBUG << "Finished setting up OSC interface" << endl;
        } else {
            SVDEBUG << "Finished setting up internal-only OSC queue" << endl;
        }

        if (m_oscScriptFile != QString()) {
            startOSCScript();
        }
    }
}

void
MainWindowBase::startOSCScript()
{
    m_oscScript = new OSCScript(m_oscScriptFile, m_oscQueue);
    connect(m_oscScript, SIGNAL(finished()),
            this, SLOT(oscScriptFinished()));
    m_oscScriptFile = QString();
    m_oscScript->start();
}

void
MainWindowBase::cueOSCScript(QString fileName)
{
    m_oscScriptFile = fileName;
    if (m_oscQueue && m_oscQueue->isOK()) {
        startOSCScript();
    }
}

void
MainWindowBase::oscScriptFinished()
{
    delete m_oscScript;
    m_oscScript = 0;
}

QString
MainWindowBase::getOpenFileName(FileFinder::FileType type)
{
    FileFinder *ff = FileFinder::getInstance();

    if (type == FileFinder::AnyFile) {
        if (!getMainModelId().isNone() &&
                m_paneStack != nullptr &&
                m_paneStack->getCurrentPane() != nullptr) { // can import a layer
            return ff->getOpenFileName(FileFinder::AnyFile, m_sessionFile);
        } else {
            return ff->getOpenFileName(FileFinder::SessionOrAudioFile,
                                       m_sessionFile);
        }
    }

    QString lastPath = m_sessionFile;

    if (type == FileFinder::AudioFile) {
        lastPath = m_audioFile;
    }

    return ff->getOpenFileName(type, lastPath);
}

QString
MainWindowBase::getSaveFileName(FileFinder::FileType type)
{
    QString lastPath = m_sessionFile;

    if (type == FileFinder::AudioFile) {
        lastPath = m_audioFile;
    }

    FileFinder *ff = FileFinder::getInstance();
    return ff->getSaveFileName(type, lastPath);
}

void
MainWindowBase::registerLastOpenedFilePath(FileFinder::FileType type, QString path)
{
    FileFinder *ff = FileFinder::getInstance();
    ff->registerLastOpenedFilePath(type, path);
}

QString
MainWindowBase::getDefaultSessionTemplate() const
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    QString templateName = settings.value("sessiontemplate", "").toString();
    if (templateName == "") templateName = "default";
    return templateName;
}

void
MainWindowBase::setDefaultSessionTemplate(QString n) 
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    settings.setValue("sessiontemplate", n);
}    

void
MainWindowBase::updateMenuStates()
{
    Pane *currentPane = nullptr;
    Layer *currentLayer = nullptr;

    if (m_paneStack) currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentLayer = currentPane->getSelectedLayer();

    bool havePrevPane = false, haveNextPane = false;
    bool havePrevLayer = false, haveNextLayer = false;

    if (currentPane) {
        for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
            if (m_paneStack->getPane(i) == currentPane) {
                if (i > 0) havePrevPane = true;
                if (i < m_paneStack->getPaneCount()-1) haveNextPane = true;
                break;
            }
        }
        // the prev/next layer commands actually include the pane
        // itself as one of the selectables -- so we always have a
        // prev and next layer, as long as we have a pane with at
        // least one layer in it
        if (currentPane->getLayerCount() > 0) {
            havePrevLayer = true;
            haveNextLayer = true;
        }
    }

    bool haveCurrentPane =
            (currentPane != nullptr);
    bool haveCurrentLayer =
            (haveCurrentPane &&
             (currentLayer != nullptr));
    bool haveMainModel =
            (!getMainModelId().isNone());
    bool havePlayTarget =
            (m_playTarget != nullptr || m_audioIO != nullptr);
    bool haveSelection =
            (m_viewManager &&
             !m_viewManager->getSelections().empty());
    bool haveCurrentEditableLayer =
            (haveCurrentLayer &&
             currentLayer->isLayerEditable());
    bool haveCurrentTimeInstantsLayer =
            (haveCurrentLayer &&
             dynamic_cast<TimeInstantLayer *>(currentLayer));
    bool haveCurrentDurationLayer =
            (haveCurrentLayer &&
             (dynamic_cast<NoteLayer *>(currentLayer) ||
              dynamic_cast<FlexiNoteLayer *>(currentLayer) ||
              dynamic_cast<RegionLayer *>(currentLayer)));
    bool haveCurrentColour3DPlot =
            (haveCurrentLayer &&
             dynamic_cast<Colour3DPlotLayer *>(currentLayer));
    bool haveClipboardContents =
            (m_viewManager &&
             !m_viewManager->getClipboard().empty());
    bool haveTabularLayer =
            (haveCurrentLayer &&
             ModelById::isa<TabularModel>(currentLayer->getModel()));

    emit canAddPane(haveMainModel);
    emit canDeleteCurrentPane(haveCurrentPane);
    emit canZoom(haveMainModel && haveCurrentPane);
    emit canScroll(haveMainModel && haveCurrentPane);
    emit canAddLayer(haveMainModel && haveCurrentPane);
    emit canImportMoreAudio(haveMainModel);
    emit canReplaceMainAudio(haveMainModel);
    emit canImportLayer(haveMainModel && haveCurrentPane);
    emit canExportAudio(haveMainModel);
    emit canChangeSessionTemplate(haveMainModel);
    emit canExportLayer(haveMainModel &&
                        (haveCurrentEditableLayer || haveCurrentColour3DPlot));
    emit canExportImage(haveMainModel && haveCurrentPane);
    emit canDeleteCurrentLayer(haveCurrentLayer);
    emit canRenameLayer(haveCurrentLayer);
    emit canEditLayer(haveCurrentEditableLayer);
    emit canEditLayerTabular(haveCurrentEditableLayer || haveTabularLayer);
    emit canMeasureLayer(haveCurrentLayer);
    emit canSelect(haveMainModel && haveCurrentPane);
    emit canPlay(haveMainModel && havePlayTarget);
    emit canFfwd(haveMainModel);
    emit canRewind(haveMainModel);
    emit canPaste(haveClipboardContents);
    emit canInsertInstant(haveCurrentPane);
    emit canInsertInstantsAtBoundaries(haveCurrentPane && haveSelection);
    emit canInsertItemAtSelection(haveCurrentPane && haveSelection && haveCurrentDurationLayer);
    emit canRenumberInstants(haveCurrentTimeInstantsLayer && haveSelection);
    emit canSubdivideInstants(haveCurrentTimeInstantsLayer && haveSelection);
    emit canWinnowInstants(haveCurrentTimeInstantsLayer && haveSelection);
    emit canPlaySelection(haveMainModel && havePlayTarget && haveSelection);
    emit canClearSelection(haveSelection);
    emit canEditSelection(haveSelection && haveCurrentEditableLayer);
    emit canSave(m_sessionFile != "" && m_documentModified);
    emit canSaveAs(haveMainModel); // possibly used only in Tony, not SV
    emit canSelectPreviousPane(havePrevPane);
    emit canSelectNextPane(haveNextPane);
    emit canSelectPreviousLayer(havePrevLayer);
    emit canSelectNextLayer(haveNextLayer);

    // This is quite subtle -- whereas we can play back only if a
    // system play target or I/O exists, we can record even if no
    // record source (i.e. audioIO) exists because we can record into
    // an empty session before the audio device has been
    // opened.
    //
    // However, if there is no record *target* then recording was
    // actively disabled via the audio mode setting.
    //
    // If we have a play target instead of an audioIO, then if the
    // audio mode is AUDIO_PLAYBACK_NOW_RECORD_LATER, we are still
    // expecting to open the IO on demand, but if it is
    // AUDIO_PLAYBACK_AND_RECORD then we must have tried to open the
    // device and failed to find any capture source.
    //
    bool recordDisabled = (m_recordTarget == nullptr);
    bool recordDeviceFailed =
            (m_audioMode == AUDIO_PLAYBACK_AND_RECORD &&
             (m_playTarget != nullptr && m_audioIO == nullptr));
    emit canRecord(!recordDisabled && !recordDeviceFailed);
}

void
MainWindowBase::updateWindowTitle()
{
    QString title;

    if (m_sessionFile != "") {
        if (m_originalLocation != "" &&
                m_originalLocation != m_sessionFile) { // session + location
            title = tr("%1: %2 [%3]")
                    .arg(QApplication::applicationName())
                    .arg(QFileInfo(m_sessionFile).fileName())
                    .arg(m_originalLocation);
        } else { // session only
            title = tr("%1: %2")
                    .arg(QApplication::applicationName())
                    .arg(QFileInfo(m_sessionFile).fileName());
        }
    } else {
        if (m_originalLocation != "") { // location only
            title = tr("%1: %2")
                    .arg(QApplication::applicationName())
                    .arg(m_originalLocation);
        } else { // neither
            title = QApplication::applicationName();
        }
    }
    
    if (m_documentModified) {
        title = tr("%1 (modified)").arg(title);
    }

    setWindowTitle(title);
}

void
MainWindowBase::documentModified()
{
    //    SVDEBUG << "MainWindowBase::documentModified" << endl;

    m_documentModified = true;
    updateWindowTitle();
    updateMenuStates();
}

void
MainWindowBase::documentRestored()
{
    //    SVDEBUG << "MainWindowBase::documentRestored" << endl;

    m_documentModified = false;
    updateWindowTitle();
    updateMenuStates();
}

void
MainWindowBase::playLoopToggled()
{
    QAction *action = dynamic_cast<QAction *>(sender());
    
    if (action) {
        m_viewManager->setPlayLoopMode(action->isChecked());
    } else {
        m_viewManager->setPlayLoopMode(!m_viewManager->getPlayLoopMode());
    }
}

void
MainWindowBase::playSelectionToggled()
{
    QAction *action = dynamic_cast<QAction *>(sender());
    
    if (action) {
        m_viewManager->setPlaySelectionMode(action->isChecked());
    } else {
        m_viewManager->setPlaySelectionMode(!m_viewManager->getPlaySelectionMode());
    }
}

void
MainWindowBase::playSoloToggled()
{
    QAction *action = dynamic_cast<QAction *>(sender());
    
    if (action) {
        m_viewManager->setPlaySoloMode(action->isChecked());
    } else {
        m_viewManager->setPlaySoloMode(!m_viewManager->getPlaySoloMode());
    }

    if (m_viewManager->getPlaySoloMode()) {
        currentPaneChanged(m_paneStack->getCurrentPane());
    } else {
        m_viewManager->setPlaybackModel({});
        if (m_playSource) {
            m_playSource->clearSoloModelSet();
        }
    }
}

void
MainWindowBase::currentPaneChanged(Pane *p)
{
    updateMenuStates();
    updateVisibleRangeDisplay(p);

    if (!p) return;

    if (!(m_viewManager &&
          m_playSource &&
          m_viewManager->getPlaySoloMode())) {
        if (m_viewManager) {
            m_viewManager->setPlaybackModel(ModelId());
        }
        return;
    }

    ModelId prevPlaybackModel = m_viewManager->getPlaybackModel();

    // What we want here is not the currently playing frame (unless we
    // are about to clear out the audio playback buffers -- which may
    // or may not be possible, depending on the audio driver).  What
    // we want is the frame that was last committed to the soundcard
    // buffers, as the audio driver will continue playing up to that
    // frame before switching to whichever one we decide we want to
    // switch to, regardless of our efforts.

    sv_frame_t frame = m_playSource->getCurrentBufferedFrame();

    cerr << "currentPaneChanged: current frame (in ref model) = " << frame << endl;

    View::ModelSet soloModels = p->getModels();
    
    View::ModelSet sources;
    for (ModelId modelId: sources) {
        // If a model in this pane is derived from something else,
        // then we want to play that model as well -- if the model
        // that's derived from it is not something that is itself
        // individually playable (e.g. a waveform)
        if (auto model = ModelById::get(modelId)) {
            if (!ModelById::isa<RangeSummarisableTimeValueModel>(modelId) &&
                    !model->getSourceModel().isNone()) {
                sources.insert(model->getSourceModel());
            }
        }
    }
    for (ModelId modelId: sources) {
        soloModels.insert(modelId);
    }

    //!!! Need an "atomic" way of telling the play source that the
    //playback model has changed, and changing it on ViewManager --
    //the play source should be making the setPlaybackModel call to
    //ViewManager

    ModelId newPlaybackModel;
    
    for (ModelId modelId: soloModels) {
        if (ModelById::isa<RangeSummarisableTimeValueModel>(modelId)) {
            m_viewManager->setPlaybackModel(modelId);
            newPlaybackModel = modelId;
        }
    }

    m_playSource->setSoloModelSet(soloModels);

    if (!prevPlaybackModel.isNone() && !newPlaybackModel.isNone() &&
            prevPlaybackModel != newPlaybackModel) {
        if (m_playSource->isPlaying()) {
            m_playSource->play(frame);
        }
    }
}

void
MainWindowBase::currentLayerChanged(Pane *p, Layer *)
{
    updateMenuStates();
    updateVisibleRangeDisplay(p);
}

sv_frame_t
MainWindowBase::getModelsStartFrame() const
{
    sv_frame_t startFrame = 0;
    if (!m_paneStack) return startFrame;
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        sv_frame_t thisStart = m_paneStack->getPane(i)->getModelsStartFrame();
        if (i == 0 || thisStart < startFrame) {
            startFrame = thisStart;
        }
    }
    return startFrame;
}

sv_frame_t
MainWindowBase::getModelsEndFrame() const
{
    sv_frame_t endFrame = 0;
    if (!m_paneStack) return endFrame;
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        sv_frame_t thisEnd = m_paneStack->getPane(i)->getModelsEndFrame();
        if (i == 0 || thisEnd > endFrame) {
            endFrame = thisEnd;
        }
    }
    return endFrame;
}

void
MainWindowBase::selectAll()
{
    m_viewManager->setSelection(Selection(getModelsStartFrame(),
                                          getModelsEndFrame()));
}

void
MainWindowBase::selectToStart()
{
    m_viewManager->setSelection(Selection(getModelsStartFrame(),
                                          m_viewManager->getGlobalCentreFrame()));
}

void
MainWindowBase::selectToEnd()
{
    m_viewManager->setSelection(Selection(m_viewManager->getGlobalCentreFrame(),
                                          getModelsEndFrame()));
}

void
MainWindowBase::selectVisible()
{
    auto model = getMainModel();
    if (!model) return;

    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    sv_frame_t startFrame, endFrame;

    if (currentPane->getStartFrame() < 0) {
        startFrame = 0;
    } else {
        startFrame = currentPane->getStartFrame();
    }

    if (currentPane->getEndFrame() > model->getEndFrame()) {
        endFrame = model->getEndFrame();
    } else {
        endFrame = currentPane->getEndFrame();
    }

    m_viewManager->setSelection(Selection(startFrame, endFrame));
}

void
MainWindowBase::clearSelection()
{
    m_viewManager->clearSelections();
}

void
MainWindowBase::cut()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    Layer *layer = currentPane->getSelectedLayer();
    if (!layer) return;

    Clipboard &clipboard = m_viewManager->getClipboard();
    clipboard.clear();

    MultiSelection::SelectionList selections = m_viewManager->getSelections();

    CommandHistory::getInstance()->startCompoundOperation(tr("Cut"), true);

    for (MultiSelection::SelectionList::iterator i = selections.begin();
         i != selections.end(); ++i) {
        layer->copy(currentPane, *i, clipboard);
        layer->deleteSelection(*i);
    }

    CommandHistory::getInstance()->endCompoundOperation();
}

void
MainWindowBase::copy()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    Layer *layer = currentPane->getSelectedLayer();
    if (!layer) return;

    Clipboard &clipboard = m_viewManager->getClipboard();
    clipboard.clear();

    MultiSelection::SelectionList selections = m_viewManager->getSelections();

    for (MultiSelection::SelectionList::iterator i = selections.begin();
         i != selections.end(); ++i) {
        layer->copy(currentPane, *i, clipboard);
    }
}

void
MainWindowBase::paste()
{
    pasteRelative(0);
}

void
MainWindowBase::pasteAtPlaybackPosition()
{
    sv_frame_t pos = getFrame();
    Clipboard &clipboard = m_viewManager->getClipboard();
    if (!clipboard.empty()) {
        sv_frame_t firstEventFrame = clipboard.getPoints()[0].getFrame();
        sv_frame_t offset = 0;
        if (firstEventFrame < 0) {
            offset = pos - firstEventFrame;
        } else if (firstEventFrame < pos) {
            offset = pos - firstEventFrame;
        } else {
            offset = -(firstEventFrame - pos);
        }
        pasteRelative(offset);
    }
}

void
MainWindowBase::pasteRelative(sv_frame_t offset)
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    Layer *layer = currentPane->getSelectedLayer();

    Clipboard &clipboard = m_viewManager->getClipboard();

    bool inCompound = false;

    if (!layer || !layer->isLayerEditable()) {
        
        CommandHistory::getInstance()->startCompoundOperation
                (tr("Paste"), true);

        // no suitable current layer: create one of the most
        // appropriate sort
        LayerFactory::LayerType type =
                LayerFactory::getInstance()->getLayerTypeForClipboardContents(clipboard);
        layer = m_document->createEmptyLayer(type);

        if (!layer) {
            CommandHistory::getInstance()->endCompoundOperation();
            return;
        }

        m_document->addLayerToView(currentPane, layer);
        m_paneStack->setCurrentLayer(currentPane, layer);

        inCompound = true;
    }

    layer->paste(currentPane, clipboard, offset, true);

    if (inCompound) CommandHistory::getInstance()->endCompoundOperation();
}

void
MainWindowBase::deleteSelected()
{
    if (m_paneStack->getCurrentPane() &&
            m_paneStack->getCurrentPane()->getSelectedLayer()) {
        
        Layer *layer = m_paneStack->getCurrentPane()->getSelectedLayer();

        if (m_viewManager) {

            if (m_viewManager->getToolMode() == ViewManager::MeasureMode) {

                layer->deleteCurrentMeasureRect();

            } else {

                MultiSelection::SelectionList selections =
                        m_viewManager->getSelections();

                for (MultiSelection::SelectionList::iterator i = selections.begin();
                     i != selections.end(); ++i) {
                    layer->deleteSelection(*i);
                }
            }
        }
    }
}

// FrameTimer method

sv_frame_t
MainWindowBase::getFrame() const
{
    if (m_playSource && m_playSource->isPlaying()) {
        return m_playSource->getCurrentPlayingFrame();
    } else {
        return m_viewManager->getPlaybackFrame();
    }
}    

void
MainWindowBase::insertInstant()
{
    insertInstantAt(getFrame());
}

void
MainWindowBase::insertInstantsAtBoundaries()
{
    MultiSelection::SelectionList selections = m_viewManager->getSelections();
    for (MultiSelection::SelectionList::iterator i = selections.begin();
         i != selections.end(); ++i) {
        sv_frame_t start = i->getStartFrame();
        sv_frame_t end = i->getEndFrame();
        if (start != end) {
            insertInstantAt(start);
            insertInstantAt(end);
        }
    }
}

void
MainWindowBase::insertInstantAt(sv_frame_t frame)
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) {
        return;
    }

    frame = pane->alignFromReference(frame);

    Layer *layer = dynamic_cast<TimeInstantLayer *>
            (pane->getSelectedLayer());

    if (!layer) {
        for (int i = pane->getLayerCount(); i > 0; --i) {
            layer = dynamic_cast<TimeInstantLayer *>(pane->getLayer(i - 1));
            if (layer) break;
        }

        if (!layer) {
            CommandHistory::getInstance()->startCompoundOperation
                    (tr("Add Point"), true);
            layer = m_document->createEmptyLayer(LayerFactory::TimeInstants);
            if (layer) {
                m_document->addLayerToView(pane, layer);
                m_paneStack->setCurrentLayer(pane, layer);
            }
            CommandHistory::getInstance()->endCompoundOperation();
        }
    }

    if (layer) {

        ModelId model = layer->getModel();
        auto sodm = ModelById::getAs<SparseOneDimensionalModel>(model);

        if (sodm) {
            Event point(frame, "");
            Event prevPoint(0);
            bool havePrevPoint = false;

            ChangeEventsCommand *command =
                    new ChangeEventsCommand(model.untyped, tr("Add Point"));

            if (m_labeller) {

                if (m_labeller->requiresPrevPoint()) {
                    
                    if (sodm->getNearestEventMatching
                            (frame,
                             [](Event) { return true; },
                             EventSeries::Backward,
                             prevPoint)) {
                        havePrevPoint = true;
                    }
                }

                m_labeller->setSampleRate(sodm->getSampleRate());

                Labeller::Relabelling relabelling = m_labeller->label
                        (point, havePrevPoint ? &prevPoint : nullptr);

                if (relabelling.first == Labeller::AppliesToPreviousEvent) {
                    command->remove(prevPoint);
                    command->add(relabelling.second);
                } else {
                    point = relabelling.second;
                }
            }
            
            command->add(point);

            command->setName(tr("Add Point at %1 s")
                             .arg(RealTime::frame2RealTime
                                  (frame,
                                   sodm->getSampleRate())
                                  .toText(false).c_str()));

            Command *c = command->finish();
            if (c) {
                CommandHistory::getInstance()->addCommand(c, false);
            }
        }
    }
}

void
MainWindowBase::insertItemAtSelection()
{
    MultiSelection::SelectionList selections = m_viewManager->getSelections();
    for (MultiSelection::SelectionList::iterator i = selections.begin();
         i != selections.end(); ++i) {
        sv_frame_t start = i->getStartFrame();
        sv_frame_t end = i->getEndFrame();
        if (start < end) {
            insertItemAt(start, end - start);
        }
    }
}

void
MainWindowBase::insertItemAt(sv_frame_t frame, sv_frame_t duration)
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) {
        return;
    }

    // ugh!

    sv_frame_t alignedStart = pane->alignFromReference(frame);
    sv_frame_t alignedEnd = pane->alignFromReference(frame + duration);
    if (alignedStart >= alignedEnd) return;
    sv_frame_t alignedDuration = alignedEnd - alignedStart;

    Command *c = nullptr;

    QString name = tr("Add Item at %1 s")
            .arg(RealTime::frame2RealTime
                 (alignedStart,
                  getMainModel()->getSampleRate())
                 .toText(false).c_str());

    Layer *layer = pane->getSelectedLayer();
    if (!layer) return;

    ModelId modelId = layer->getModel();
    
    auto rm = ModelById::getAs<RegionModel>(modelId);
    if (rm) {
        Event point(alignedStart,
                    rm->getValueMaximum() + 1,
                    alignedDuration,
                    "");
        ChangeEventsCommand *command = new ChangeEventsCommand
                (modelId.untyped, name);
        command->add(point);
        c = command->finish();
    }

    if (c) {
        CommandHistory::getInstance()->addCommand(c, false);
        return;
    }

    auto nm = ModelById::getAs<NoteModel>(modelId);
    if (nm) {
        Event point(alignedStart,
                    nm->getValueMinimum(),
                    alignedDuration,
                    1.f,
                    "");
        ChangeEventsCommand *command = new ChangeEventsCommand
                (modelId.untyped, name);
        command->add(point);
        c = command->finish();
    }

    if (c) {
        CommandHistory::getInstance()->addCommand(c, false);
        return;
    }
}

void
MainWindowBase::renumberInstants()
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) return;

    Layer *layer = dynamic_cast<TimeInstantLayer *>(pane->getSelectedLayer());
    if (!layer) return;

    MultiSelection ms(m_viewManager->getSelection());

    ModelId modelId = layer->getModel();
    
    auto sodm = ModelById::getAs<SparseOneDimensionalModel>(modelId);
    if (!sodm) return;

    if (!m_labeller) return;

    Labeller labeller(*m_labeller);
    labeller.setSampleRate(sodm->getSampleRate());

    Command *c = labeller.labelAll(modelId.untyped, &ms, sodm->getAllEvents());
    if (c) CommandHistory::getInstance()->addCommand(c, false);
}

void
MainWindowBase::subdivideInstantsBy(int n)
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) return;

    Layer *layer = dynamic_cast<TimeInstantLayer *>(pane->getSelectedLayer());
    if (!layer) return;

    MultiSelection ms(m_viewManager->getSelection());

    ModelId modelId = layer->getModel();
    
    auto sodm = ModelById::getAs<SparseOneDimensionalModel>(modelId);
    if (!sodm) return;

    if (!m_labeller) return;

    Labeller labeller(*m_labeller);
    labeller.setSampleRate(sodm->getSampleRate());

    Command *c = labeller.subdivide(modelId.untyped, &ms, sodm->getAllEvents(), n);
    if (c) CommandHistory::getInstance()->addCommand(c, false);
}

void
MainWindowBase::winnowInstantsBy(int n)
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) return;

    Layer *layer = dynamic_cast<TimeInstantLayer *>(pane->getSelectedLayer());
    if (!layer) return;

    MultiSelection ms(m_viewManager->getSelection());

    ModelId modelId = layer->getModel();
    
    auto sodm = ModelById::getAs<SparseOneDimensionalModel>(modelId);
    if (!sodm) return;

    if (!m_labeller) return;

    Labeller labeller(*m_labeller);
    labeller.setSampleRate(sodm->getSampleRate());

    Command *c = labeller.winnow(modelId.untyped, &ms, sodm->getAllEvents(), n);
    if (c) CommandHistory::getInstance()->addCommand(c, false);
}

MainWindowBase::FileOpenStatus
MainWindowBase::openPath(QString fileOrUrl, AudioFileOpenMode mode)
{
    ProgressDialog dialog(tr("Opening file or URL..."), true, 2000, this);
    connect(&dialog, SIGNAL(showing()), this, SIGNAL(hideSplash()));
    return open(FileSource(fileOrUrl, &dialog), mode);
}

MainWindowBase::FileOpenStatus
MainWindowBase::open(FileSource source, AudioFileOpenMode mode)
{
    FileOpenStatus status;

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    bool canImportLayer = (getMainModel() != nullptr &&
            m_paneStack != nullptr &&
            m_paneStack->getCurrentPane() != nullptr);

    bool rdf = (source.getExtension().toLower() == "rdf" ||
                source.getExtension().toLower() == "n3" ||
                source.getExtension().toLower() == "ttl");

    bool audio = AudioFileReaderFactory::getKnownExtensions().contains
            (source.getExtension().toLower());

    bool rdfSession = false;
    if (rdf) {
        RDFImporter::RDFDocumentType rdfType =
                RDFImporter::identifyDocumentType
                (QUrl::fromLocalFile(source.getLocalFilename()).toString());
        if (rdfType == RDFImporter::AudioRefAndAnnotations ||
                rdfType == RDFImporter::AudioRef) {
            rdfSession = true;
        } else if (rdfType == RDFImporter::NotRDF) {
            rdf = false;
        }
    }

    try {
        if (rdf) {
            if (rdfSession) {
                bool cancel = false;
                if (!canImportLayer || shouldCreateNewSessionForRDFAudio(&cancel)) {
                    return openSession(source);
                } else if (cancel) {
                    return FileOpenCancelled;
                } else {
                    return openLayer(source);
                }
            } else {
                if ((status = openSession(source)) != FileOpenFailed) {
                    return status;
                } else if (!canImportLayer) {
                    return FileOpenWrongMode;
                } else if ((status = openLayer(source)) != FileOpenFailed) {
                    return status;
                } else {
                    return FileOpenFailed;
                }
            }
        }

        if (audio && (status = openAudio(source, mode)) != FileOpenFailed) {
            return status;
        } else if ((status = openSession(source)) != FileOpenFailed) {
            return status;
        } else if ((status = openPlaylist(source, mode)) != FileOpenFailed) {
            return status;
        } else if (!canImportLayer) {
            return FileOpenWrongMode;
        } else if ((status = openImage(source)) != FileOpenFailed) {
            return status;
        } else if ((status = openLayer(source)) != FileOpenFailed) {
            return status;
        } else {
            return FileOpenFailed;
        }
    } catch (const InsufficientDiscSpace &e) {
        emit hideSplash();
        m_openingAudioFile = false;
        SVCERR << "MainWindowBase: Caught InsufficientDiscSpace in file open" << endl;
        QMessageBox::critical
                (this, tr("Not enough disc space"),
                 tr("<b>Not enough disc space</b><p>There doesn't appear to be enough spare disc space to accommodate any necessary temporary files.</p><p>Please clear some space and try again.</p>").arg(e.what()));
        return FileOpenFailed;
    } catch (const std::bad_alloc &e) { // reader may have rethrown this after cleaning up
        emit hideSplash();
        m_openingAudioFile = false;
        SVCERR << "MainWindowBase: Caught bad_alloc in file open" << endl;
        QMessageBox::critical
                (this, tr("Not enough memory"),
                 tr("<b>Not enough memory</b><p>There doesn't appear to be enough memory to accommodate any necessary temporary data.</p>"));
        return FileOpenFailed;
    }
}

MainWindowBase::FileOpenStatus
MainWindowBase::openAudio(FileSource source,
                          AudioFileOpenMode mode,
                          QString templateName)
{
    SVDEBUG << "MainWindowBase::openAudio(" << source.getLocation() << ") with mode " << mode << " and template " << templateName << endl;

    if (templateName == "") {
        templateName = getDefaultSessionTemplate();
        SVDEBUG << "(Default template is: \"" << templateName << "\")" << endl;
    }

    //    cerr << "template is: \"" << templateName << "\"" << endl;

    if (!source.isAvailable()) {
        if (source.wasCancelled()) {
            return FileOpenCancelled;
        } else {
            return FileOpenFailed;
        }
    }

    source.waitForData();

    m_openingAudioFile = true;

    sv_samplerate_t rate = 0;

    SVDEBUG << "Checking whether to preserve incoming audio file's sample rate"
            << endl;
    
    if (Preferences::getInstance()->getFixedSampleRate() != 0) {
        rate = Preferences::getInstance()->getFixedSampleRate();
        SVDEBUG << "No: preferences specify fixed rate of " << rate << endl;
    } else if (Preferences::getInstance()->getResampleOnLoad()) {
        if (getMainModel()) {
            if (mode == ReplaceSession || mode == ReplaceMainModel) {
                SVDEBUG << "Preferences specify resampling additional models to match main model, but we are opening this file to replace the main model according to the open mode: therefore..." << endl;
            } else {
                rate = getMainModel()->getSampleRate();
                SVDEBUG << "No: preferences specify resampling to match main model, whose rate is currently " << rate << endl;
            }
        }
    }

    if (rate == 0) {
        SVDEBUG << "Yes, preserving incoming file rate" << endl;
    }
    
    auto newModel = std::make_shared<ReadOnlyWaveFileModel>(source, rate);
    if (!newModel->isOK()) {
        m_openingAudioFile = false;
        if (source.wasCancelled()) {
            return FileOpenCancelled;
        } else {
            return FileOpenFailed;
        }
    }

    auto newModelId = ModelById::add(newModel);
    auto status = addOpenedAudioModel
            (source, newModelId, mode, templateName, true);
    m_openingAudioFile = false;
    return status;
}

MainWindowBase::FileOpenStatus
MainWindowBase::addOpenedAudioModel(FileSource source,
                                    ModelId newModel,
                                    AudioFileOpenMode mode,
                                    QString templateName,
                                    bool registerSource)
{
    if (mode == AskUser) {
        if (getMainModel()) {

            QSettings settings;
            settings.beginGroup("MainWindow");
            int lastMode = settings.value("lastaudioopenmode", 0).toBool();
            settings.endGroup();
            int imode = 0;
            
            QStringList items;
            items << tr("Close the current session and start a new one")
                  << tr("Replace the main audio file in this session")
                  << tr("Add the audio file to this session");

            bool ok = false;
            QString item = ListInputDialog::getItem
                    (this, tr("Select target for import"),
                     tr("<b>Select a target for import</b><p>You already have an audio file loaded.<br>What would you like to do with the new audio file?"),
                     items, lastMode, &ok);
            
            if (!ok || item.isEmpty()) {
                ModelById::release(newModel);
                m_openingAudioFile = false;
                return FileOpenCancelled;
            }
            
            for (int i = 0; i < items.size(); ++i) {
                if (item == items[i]) imode = i;
            }

            settings.beginGroup("MainWindow");
            settings.setValue("lastaudioopenmode", imode);
            settings.endGroup();

            mode = (AudioFileOpenMode)imode;

        } else {
            // no main model: make a new session
            mode = ReplaceSession;
        }
    }

    if (mode == ReplaceCurrentPane) {

        Pane *pane = m_paneStack->getCurrentPane();
        if (pane) {
            if (getMainModel()) {
                View::ModelSet models(pane->getModels());
                if (models.find(getMainModelId()) != models.end()) {
                    // Current pane contains main model: replace that
                    mode = ReplaceMainModel;
                }
                // Otherwise the current pane has a non-default model,
                // which we will deal with later
            } else {
                // We have no main model, so start a new session with
                // optional template
                mode = ReplaceSession;
            }
        } else {
            // We seem to have no current pane!  Oh well
            mode = CreateAdditionalModel;
        }
    }

    if (mode == CreateAdditionalModel && getMainModelId().isNone()) {
        SVDEBUG << "Mode is CreateAdditionalModel but we have no main model, switching to ReplaceSession mode" << endl;
        mode = ReplaceSession;
    }

    bool loadedTemplate = false;

    if (mode == ReplaceSession) {

        if (!checkSaveModified()) {
            m_openingAudioFile = false;
            return FileOpenCancelled;
        }

        SVDEBUG << "SV looking for template " << templateName << endl;
        if (templateName != "") {
            FileOpenStatus tplStatus = openSessionTemplate(templateName);
            if (tplStatus == FileOpenCancelled) {
                SVDEBUG << "Template load cancelled" << endl;
                m_openingAudioFile = false;
                return FileOpenCancelled;
            }
            if (tplStatus != FileOpenFailed) {
                SVDEBUG << "Template load succeeded" << endl;
                loadedTemplate = true;
            }
        }

        if (!loadedTemplate) {
            SVDEBUG << "No template found: closing session, creating new empty document" << endl;
            closeSession();
            createDocument();
        }

        SVDEBUG << "Now switching to ReplaceMainModel mode" << endl;
        mode = ReplaceMainModel;
    }

    emit activity(tr("Import audio file \"%1\"").arg(source.getLocation()));
    
    if (mode == ReplaceMainModel) {

        ModelId prevMain = getMainModelId();
        if (!prevMain.isNone()) {
            m_playSource->removeModel(prevMain);
        }

        SVDEBUG << "SV about to call setMainModel(" << newModel << "): prevMain is " << prevMain << endl;

        m_document->setMainModel(newModel);

        setupMenus();

        m_originalLocation = source.getLocation();

        if (loadedTemplate || (m_sessionFile == "")) {
            CommandHistory::getInstance()->clear();
            CommandHistory::getInstance()->documentSaved();
            m_documentModified = false;
        } else {
            if (m_documentModified) {
                m_documentModified = false;
            }
        }

        if (!source.isRemote() && registerSource) {
            m_audioFile = source.getLocalFilename();
        }

        updateWindowTitle();
        
    } else if (mode == CreateAdditionalModel) {

        SVCERR << "Mode is CreateAdditionalModel" << endl;
        
        CommandHistory::getInstance()->startCompoundOperation
                (tr("Import \"%1\"").arg(source.getBasename()), true);

        m_document->addNonDerivedModel(newModel);

        AddPaneCommand *command = new AddPaneCommand(this);
        CommandHistory::getInstance()->addCommand(command);

        Pane *pane = command->getPane();

        if (m_timeRulerLayer) {
            SVCERR << "Have time ruler, adding it" << endl;
            m_document->addLayerToView(pane, m_timeRulerLayer);
        } else {
            SVCERR << "Do not have time ruler" << endl;
        }

        Layer *newLayer = m_document->createImportedLayer(newModel);

        if (newLayer) {
            m_document->addLayerToView(pane, newLayer);
        }
        
        CommandHistory::getInstance()->endCompoundOperation();

    } else if (mode == ReplaceCurrentPane) {

        // We know there is a current pane, otherwise we would have
        // reset the mode to CreateAdditionalModel above; and we know
        // the current pane does not contain the main model, otherwise
        // we would have reset it to ReplaceMainModel.  But we don't
        // know whether the pane contains a waveform model at all.
        
        Pane *pane = m_paneStack->getCurrentPane();
        Layer *replace = nullptr;

        for (int i = 0; i < pane->getLayerCount(); ++i) {
            Layer *layer = pane->getLayer(i);
            if (dynamic_cast<WaveformLayer *>(layer)) {
                replace = layer;
                break;
            }
        }

        CommandHistory::getInstance()->startCompoundOperation
                (tr("Import \"%1\"").arg(source.getBasename()), true);

        m_document->addNonDerivedModel(newModel);

        if (replace) {
            m_document->removeLayerFromView(pane, replace);
        }

        Layer *newLayer = m_document->createImportedLayer(newModel);

        if (newLayer) {
            m_document->addLayerToView(pane, newLayer);
        }
        
        CommandHistory::getInstance()->endCompoundOperation();
    }

    updateMenuStates();

    if (registerSource) {
        m_recentFiles.addFile(source.getLocation());
    }
    if (!source.isRemote() && registerSource) {
        // for file dialog
        registerLastOpenedFilePath(FileFinder::AudioFile,
                                   source.getLocalFilename());
    }
    
    m_openingAudioFile = false;

    currentPaneChanged(m_paneStack->getCurrentPane());

    emit audioFileLoaded();

    return FileOpenSucceeded;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openPlaylist(FileSource source, AudioFileOpenMode mode)
{
    SVDEBUG << "MainWindowBase::openPlaylist(" << source.getLocation() << ")" << endl;

    std::set<QString> extensions;
    PlaylistFileReader::getSupportedExtensions(extensions);
    QString extension = source.getExtension().toLower();
    if (extensions.find(extension) == extensions.end()) return FileOpenFailed;

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    PlaylistFileReader reader(source.getLocalFilename());
    if (!reader.isOK()) return FileOpenFailed;

    PlaylistFileReader::Playlist playlist = reader.load();

    bool someSuccess = false;

    for (PlaylistFileReader::Playlist::const_iterator i = playlist.begin();
         i != playlist.end(); ++i) {

        ProgressDialog dialog(tr("Opening playlist..."), true, 2000, this);
        connect(&dialog, SIGNAL(showing()), this, SIGNAL(hideSplash()));
        FileOpenStatus status = openAudio(FileSource(*i, &dialog), mode);

        if (status == FileOpenCancelled) {
            return FileOpenCancelled;
        }

        if (status == FileOpenSucceeded) {
            someSuccess = true;
            mode = CreateAdditionalModel;
        }
    }

    if (someSuccess) return FileOpenSucceeded;
    else return FileOpenFailed;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openLayer(FileSource source)
{
    SVDEBUG << "MainWindowBase::openLayer(" << source.getLocation() << ")" << endl;

    Pane *pane = m_paneStack->getCurrentPane();
    
    if (!pane) {
        // shouldn't happen, as the menu action should have been disabled
        cerr << "WARNING: MainWindowBase::openLayer: no current pane" << endl;
        return FileOpenWrongMode;
    }

    if (!getMainModel()) {
        // shouldn't happen, as the menu action should have been disabled
        cerr << "WARNING: MainWindowBase::openLayer: No main model -- hence no default sample rate available" << endl;
        return FileOpenWrongMode;
    }

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    QString path = source.getLocalFilename();

    RDFImporter::RDFDocumentType rdfType =
            RDFImporter::identifyDocumentType(QUrl::fromLocalFile(path).toString());

    //    cerr << "RDF type:  (in layer) " << (int) rdfType << endl;

    if (rdfType != RDFImporter::NotRDF) {
        qDebug() << "RDF import ... ";
        return openLayersFromRDF(source);

    } else if (source.getExtension().toLower() == "svl" ||
               (source.getExtension().toLower() == "xml" &&
                (SVFileReader::identifyXmlFile(source.getLocalFilename())
                 == SVFileReader::SVLayerFile))) {

        PaneCallback callback(this);
        QFile file(path);
        
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            cerr << "ERROR: MainWindowBase::openLayer("
                 << source.getLocation()
                 << "): Failed to open file for reading" << endl;
            return FileOpenFailed;
        }
        
        SVFileReader reader(m_document, callback, source.getLocation());
        connect
                (&reader, SIGNAL(modelRegenerationFailed(QString, QString, QString)),
                 this, SLOT(modelRegenerationFailed(QString, QString, QString)));
        connect
                (&reader, SIGNAL(modelRegenerationWarning(QString, QString, QString)),
                 this, SLOT(modelRegenerationWarning(QString, QString, QString)));
        reader.setCurrentPane(pane);
        
        QXmlInputSource inputSource(&file);
        reader.parse(inputSource);
        
        if (!reader.isOK()) {
            cerr << "ERROR: MainWindowBase::openLayer("
                 << source.getLocation()
                 << "): Failed to read XML file: "
                 << reader.getErrorString() << endl;
            return FileOpenFailed;
        }

        emit activity(tr("Import layer XML file \"%1\"").arg(source.getLocation()));

        m_recentFiles.addFile(source.getLocation());

        if (!source.isRemote()) {
            registerLastOpenedFilePath(FileFinder::LayerFile, path); // for file dialog
        }

        return FileOpenSucceeded;

    } else {
        
        try {

            MIDIFileImportDialog midiDlg(this);
            qDebug() << "MIDI section ...";
            Model *newModelPtr = DataFileReaderFactory::loadNonCSV
                    (path, &midiDlg, getMainModel()->getSampleRate());

            if (!newModelPtr) {
                CSVFormatDialog *dialog =
                        new CSVFormatDialog(this,
                                            path,
                                            getMainModel()->getSampleRate(),
                                            5);
                if (dialog->exec() == QDialog::Accepted) {
                    newModelPtr = DataFileReaderFactory::loadCSV
                            (path, dialog->getFormat(),
                             getMainModel()->getSampleRate());
                }
                delete dialog;
            }

            if (newModelPtr) {

                SVDEBUG << "MainWindowBase::openLayer: Have model" << endl;

                emit activity(tr("Import MIDI file \"%1\"").arg(source.getLocation()));

                ModelId modelId =
                        ModelById::add(std::shared_ptr<Model>(newModelPtr));
                
                Layer *newLayer = m_document->createImportedLayer(modelId);

                if (newLayer) {

                    m_document->addLayerToView(pane, newLayer);
                    m_paneStack->setCurrentLayer(pane, newLayer);

                    m_recentFiles.addFile(source.getLocation());
                    
                    if (!source.isRemote()) {
                        registerLastOpenedFilePath
                                (FileFinder::LayerFile,
                                 path); // for file dialog
                    }

                    return FileOpenSucceeded;
                }
            }
        } catch (DataFileReaderFactory::Exception e) {
            if (e == DataFileReaderFactory::ImportCancelled) {
                return FileOpenCancelled;
            }
        }
    }
    
    return FileOpenFailed;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openImage(FileSource source)
{
    SVDEBUG << "MainWindowBase::openImage(" << source.getLocation() << ")" << endl;

    Pane *pane = m_paneStack->getCurrentPane();
    
    if (!pane) {
        // shouldn't happen, as the menu action should have been disabled
        cerr << "WARNING: MainWindowBase::openImage: no current pane" << endl;
        return FileOpenWrongMode;
    }

    if (!getMainModel()) {
        return FileOpenWrongMode;
    }

    bool newLayer = false;

    ImageLayer *il = dynamic_cast<ImageLayer *>(pane->getSelectedLayer());
    if (!il) {
        for (int i = pane->getLayerCount()-1; i >= 0; --i) {
            il = dynamic_cast<ImageLayer *>(pane->getLayer(i));
            if (il) break;
        }
    }
    if (!il) {
        il = dynamic_cast<ImageLayer *>
                (m_document->createEmptyLayer(LayerFactory::Image));
        if (!il) return FileOpenFailed;
        newLayer = true;
    }

    // We don't put the image file in Recent Files

    cerr << "openImage: trying location \"" << source.getLocation() << "\" in image layer" << endl;

    if (!il->addImage(m_viewManager->getGlobalCentreFrame(), source.getLocation())) {
        if (newLayer) {
            m_document->deleteLayer(il); // also releases its model
        }
        return FileOpenFailed;
    } else {
        if (newLayer) {
            m_document->addLayerToView(pane, il);
        }
        m_paneStack->setCurrentLayer(pane, il);
    }

    return FileOpenSucceeded;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openDirOfAudio(QString dirPath)
{
    QDir dir(dirPath);
    QStringList files = dir.entryList(QDir::Files | QDir::Readable);
    files.sort();

    FileOpenStatus status = FileOpenFailed;
    bool first = true;
    bool cancelled = false;

    foreach (QString file, files) {

        FileSource source(dir.filePath(file));
        if (!source.isAvailable()) {
            continue;
        }

        if (AudioFileReaderFactory::getKnownExtensions().contains
                (source.getExtension().toLower())) {
            
            AudioFileOpenMode mode = CreateAdditionalModel;
            if (first) mode = ReplaceSession;
            
            switch (openAudio(source, mode)) {
            case FileOpenSucceeded:
                status = FileOpenSucceeded;
                first = false;
                break;
            case FileOpenFailed:
                break;
            case FileOpenCancelled:
                cancelled = true;
                break;
            case FileOpenWrongMode:
                break;
            }
        }

        if (cancelled) break;
    }

    return status;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openSessionPath(QString fileOrUrl)
{
    ProgressDialog dialog(tr("Opening session..."), true, 2000, this);
    connect(&dialog, SIGNAL(showing()), this, SIGNAL(hideSplash()));
    return openSession(FileSource(fileOrUrl, &dialog));
}

MainWindowBase::FileOpenStatus
MainWindowBase::openSession(FileSource source)
{
    SVDEBUG << "MainWindowBase::openSession(" << source.getLocation() << ")" << endl;

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    QString sessionExt =
            InteractiveFileFinder::getInstance()->getApplicationSessionExtension();

    if (source.getExtension().toLower() != sessionExt) {

        RDFImporter::RDFDocumentType rdfType =
                RDFImporter::identifyDocumentType
                (QUrl::fromLocalFile(source.getLocalFilename()).toString());

        //        cerr << "RDF type: " << (int)rdfType << endl;

        if (rdfType == RDFImporter::AudioRefAndAnnotations ||
                rdfType == RDFImporter::AudioRef) {
            return openSessionFromRDF(source);
        } else if (rdfType != RDFImporter::NotRDF) {
            return FileOpenFailed;
        }

        if (source.getExtension().toLower() == "xml") {
            if (SVFileReader::identifyXmlFile(source.getLocalFilename()) ==
                    SVFileReader::SVSessionFile) {
                cerr << "This XML file looks like a session file, attempting to open it as a session" << endl;
            } else {
                return FileOpenFailed;
            }
        } else {
            return FileOpenFailed;
        }
    }

    QXmlInputSource *inputSource = nullptr;
    BZipFileDevice *bzFile = nullptr;
    QFile *rawFile = nullptr;

    if (source.getExtension().toLower() == sessionExt) {
        bzFile = new BZipFileDevice(source.getLocalFilename());
        if (!bzFile->open(QIODevice::ReadOnly)) {
            delete bzFile;
            return FileOpenFailed;
        }
        inputSource = new QXmlInputSource(bzFile);
    } else {
        rawFile = new QFile(source.getLocalFilename());
        inputSource = new QXmlInputSource(rawFile);
    }

    if (!checkSaveModified()) {
        if (bzFile) bzFile->close();
        delete inputSource;
        delete bzFile;
        delete rawFile;
        return FileOpenCancelled;
    }

    QString error;
    closeSession();
    createDocument();

    PaneCallback callback(this);
    m_viewManager->clearSelections();

    SVFileReader reader(m_document, callback, source.getLocation());
    connect
            (&reader, SIGNAL(modelRegenerationFailed(QString, QString, QString)),
             this, SLOT(modelRegenerationFailed(QString, QString, QString)));
    connect
            (&reader, SIGNAL(modelRegenerationWarning(QString, QString, QString)),
             this, SLOT(modelRegenerationWarning(QString, QString, QString)));

    reader.parse(*inputSource);
    
    if (!reader.isOK()) {
        error = tr("SV XML file read error:\n%1").arg(reader.getErrorString());
    }
    
    if (bzFile) bzFile->close();

    delete inputSource;
    delete bzFile;
    delete rawFile;

    bool ok = (error == "");

    if (ok) {

        emit activity(tr("Import session file \"%1\"").arg(source.getLocation()));

        if (!source.isRemote() && !m_document->isIncomplete()) {
            // Setting the session file path enables the Save (as
            // opposed to Save As...) option. We can't do this if we
            // don't have a local path to save to, but we also don't
            // want to do it if we failed to find an audio file or
            // similar on load, as the audio reference would then end
            // up being lost from any saved or auto-saved-on-exit copy
            m_sessionFile = source.getLocalFilename();
        } else {
            QMessageBox::warning
                    (this,
                     tr("Incomplete session loaded"),
                     tr("Some of the audio content referred to by the original session file could not be loaded.\nIf you save this session, it will be saved without any reference to that audio, and information may be lost."),
                     QMessageBox::Ok);
        }

        updateWindowTitle();
        setupMenus();
        findTimeRulerLayer();

        CommandHistory::getInstance()->clear();
        CommandHistory::getInstance()->documentSaved();
        m_documentModified = false;
        updateMenuStates();

        m_recentFiles.addFile(source.getLocation());

        if (!source.isRemote()) {
            // for file dialog
            registerLastOpenedFilePath(FileFinder::SessionFile,
                                       source.getLocalFilename());
        }

        m_originalLocation = source.getLocation();
        
        emit sessionLoaded();

        updateWindowTitle();
    }
    
    return ok ? FileOpenSucceeded : FileOpenFailed;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openSessionTemplate(QString templateName)
{
    // Template in the user's template directory takes
    // priority over a bundled one; we don't unbundle, but
    // open directly from the bundled file (where applicable)
    ResourceFinder rf;
    QString tfile = rf.getResourcePath("templates", templateName + ".svt");
    if (tfile != "") {
        cerr << "SV loading template file " << tfile << endl;
        return openSessionTemplate(FileSource("file:" + tfile));
    } else {
        return FileOpenFailed;
    }
}

MainWindowBase::FileOpenStatus
MainWindowBase::openSessionTemplate(FileSource source)
{
    cerr << "MainWindowBase::openSessionTemplate(" << source.getLocation() << ")" << endl;

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    QXmlInputSource *inputSource = nullptr;
    QFile *file = nullptr;

    file = new QFile(source.getLocalFilename());
    inputSource = new QXmlInputSource(file);

    if (!checkSaveModified()) {
        delete inputSource;
        delete file;
        return FileOpenCancelled;
    }

    QString error;
    closeSession();
    createDocument();

    PaneCallback callback(this);
    m_viewManager->clearSelections();

    SVFileReader reader(m_document, callback, source.getLocation());
    connect
            (&reader, SIGNAL(modelRegenerationFailed(QString, QString, QString)),
             this, SLOT(modelRegenerationFailed(QString, QString, QString)));
    connect
            (&reader, SIGNAL(modelRegenerationWarning(QString, QString, QString)),
             this, SLOT(modelRegenerationWarning(QString, QString, QString)));

    reader.parse(*inputSource);
    
    if (!reader.isOK()) {
        error = tr("SV XML file read error:\n%1").arg(reader.getErrorString());
    }
    
    delete inputSource;
    delete file;

    bool ok = (error == "");

    if (ok) {

        emit activity(tr("Open session template \"%1\"").arg(source.getLocation()));

        setupMenus();
        findTimeRulerLayer();

        CommandHistory::getInstance()->clear();
        CommandHistory::getInstance()->documentSaved();
        m_documentModified = false;
        updateMenuStates();

        emit sessionLoaded();
    }

    updateWindowTitle();

    return ok ? FileOpenSucceeded : FileOpenFailed;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openSessionFromRDF(FileSource source)
{
    SVDEBUG << "MainWindowBase::openSessionFromRDF(" << source.getLocation() << ")" << endl;

    if (!source.isAvailable()) return FileOpenFailed;
    source.waitForData();

    if (!checkSaveModified()) {
        return FileOpenCancelled;
    }
    
    closeSession();
    createDocument();

    FileOpenStatus status = openLayersFromRDF(source);

    setupMenus();
    findTimeRulerLayer();

    CommandHistory::getInstance()->clear();
    CommandHistory::getInstance()->documentSaved();
    m_documentModified = false;
    updateWindowTitle();

    emit sessionLoaded();

    return status;
}

MainWindowBase::FileOpenStatus
MainWindowBase::openLayersFromRDF(FileSource source)
{
    sv_samplerate_t rate = 0;

    SVDEBUG << "MainWindowBase::openLayersFromRDF" << endl;

    ProgressDialog dialog(tr("Importing from RDF..."), true, 2000, this);
    connect(&dialog, SIGNAL(showing()), this, SIGNAL(hideSplash()));

    if (getMainModel()) {
        rate = getMainModel()->getSampleRate();
    } else if (Preferences::getInstance()->getResampleOnLoad()) {
        if (getMainModel()) {
            rate = getMainModel()->getSampleRate();
        }
    }

    RDFImporter importer
            (QUrl::fromLocalFile(source.getLocalFilename()).toString(), rate);

    if (!importer.isOK()) {
        if (importer.getErrorString() != "") {
            QMessageBox::critical
                    (this, tr("Failed to import RDF"),
                     tr("<b>Failed to import RDF</b><p>Importing data from RDF document at \"%1\" failed: %2</p>")
                     .arg(source.getLocation()).arg(importer.getErrorString()));
        }
        return FileOpenFailed;
    }

    std::vector<ModelId> modelIds = importer.getDataModels(&dialog);

    dialog.setMessage(tr("Importing from RDF..."));

    if (modelIds.empty()) {
        QMessageBox::critical
                (this, tr("Failed to import RDF"),
                 tr("<b>Failed to import RDF</b><p>No suitable data models found for import from RDF document at \"%1\"</p>").arg(source.getLocation()));
        return FileOpenFailed;
    }

    emit activity(tr("Import RDF document \"%1\"").arg(source.getLocation()));
    
    std::set<ModelId> added;

    for (auto modelId: modelIds) {
        
        if (ModelById::isa<WaveFileModel>(modelId)) {

            Pane *pane = addPaneToStack();
            Layer *layer = nullptr;

            if (m_timeRulerLayer) {
                m_document->addLayerToView(pane, m_timeRulerLayer);
            }

            if (!getMainModel()) {
                m_document->setMainModel(modelId);
                layer = m_document->createMainModelLayer(LayerFactory::Waveform);
            } else {
                layer = m_document->createImportedLayer(modelId);
            }

            m_document->addLayerToView(pane, layer);

            added.insert(modelId);

            for (auto otherId: modelIds) {

                if (otherId == modelId) continue;

                bool isDependent = false;
                if (auto dm = ModelById::get(otherId)) {
                    if (dm->getSourceModel() == modelId) {
                        isDependent = true;
                    }
                }
                if (!isDependent) continue;

                layer = m_document->createImportedLayer(otherId);

                if (layer->isLayerOpaque() ||
                        dynamic_cast<Colour3DPlotLayer *>(layer)) {

                    // these always go in a new pane, with nothing
                    // else going in the same pane

                    Pane *singleLayerPane = addPaneToStack();
                    if (m_timeRulerLayer) {
                        m_document->addLayerToView(singleLayerPane, m_timeRulerLayer);
                    }
                    m_document->addLayerToView(singleLayerPane, layer);

                } else if (layer->getLayerColourSignificance() ==
                           Layer::ColourHasMeaningfulValue) {

                    // these can go in a pane with something else, but
                    // only if none of the something elses also have
                    // this quality

                    bool needNewPane = false;
                    for (int i = 0; i < pane->getLayerCount(); ++i) {
                        Layer *otherLayer = pane->getLayer(i);
                        if (otherLayer &&
                                (otherLayer->getLayerColourSignificance() ==
                                 Layer::ColourHasMeaningfulValue)) {
                            needNewPane = true;
                            break;
                        }
                    }
                    if (needNewPane) {
                        pane = addPaneToStack();
                    }

                    m_document->addLayerToView(pane, layer);

                } else {

                    if (pane->getLayerCount() > 4) {
                        pane = addPaneToStack();
                    }

                    m_document->addLayerToView(pane, layer);
                }

                added.insert(otherId);
            }
        }
    }

    for (auto modelId : modelIds) {
        
        if (added.find(modelId) == added.end()) {
            
            Layer *layer = m_document->createImportedLayer(modelId);
            if (!layer) return FileOpenFailed;

            Pane *singleLayerPane = addPaneToStack();
            if (m_timeRulerLayer) {
                m_document->addLayerToView(singleLayerPane, m_timeRulerLayer);
            }
            m_document->addLayerToView(singleLayerPane, layer);
        }
    }

    m_recentFiles.addFile(source.getLocation());
    return FileOpenSucceeded;
}

class AudioLogCallback : public breakfastquay::AudioFactory::LogCallback
{
public:
    void log(std::string message) const override {
        SVDEBUG << message << endl;
    }
};

void
MainWindowBase::createAudioIO()
{
    if (m_playTarget || m_audioIO) return;

    static AudioLogCallback audioLogCallback;
    breakfastquay::AudioFactory::setLogCallback(&audioLogCallback);

    if (m_audioMode == AUDIO_NONE) return;

    QSettings settings;
    settings.beginGroup("Preferences");
    QString implementation = settings.value
            ("audio-target", "").toString();
    QString suffix;
    if (implementation != "") suffix = "-" + implementation;
    QString recordDevice = settings.value
            ("audio-record-device" + suffix, "").toString();
    QString playbackDevice = settings.value
            ("audio-playback-device" + suffix, "").toString();
    settings.endGroup();

    if (implementation == "auto") {
        implementation = "";
    }
    
    breakfastquay::AudioFactory::Preference preference;
    preference.implementation = implementation.toStdString();
    preference.recordDevice = recordDevice.toStdString();
    preference.playbackDevice = playbackDevice.toStdString();

    SVCERR << "createAudioIO: Preferred implementation = \""
           << preference.implementation << "\"" << endl;
    SVCERR << "createAudioIO: Preferred playback device = \""
           << preference.playbackDevice << "\"" << endl;
    SVCERR << "createAudioIO: Preferred record device = \""
           << preference.recordDevice << "\"" << endl;

    if (!m_resamplerWrapper) {
        m_resamplerWrapper = new breakfastquay::ResamplerWrapper(m_playSource);
        m_playSource->setResamplerWrapper(m_resamplerWrapper);
    }

    std::string errorString;
    
    if (m_audioMode == AUDIO_PLAYBACK_AND_RECORD) {
        m_audioIO = breakfastquay::AudioFactory::
                createCallbackIO(m_recordTarget, m_resamplerWrapper,
                                 preference, errorString);
        if (m_audioIO) {
            SVCERR << "MainWindowBase::createAudioIO: Suspending on creation" << endl;
            m_audioIO->suspend(); // start in suspended state
            m_playSource->setSystemPlaybackTarget(m_audioIO);
        } else {
            // Failed to create audio I/O; this may just mean there is
            // no record device, so fall through to see what happens
            // next. We only report complete failure if we end up with
            // neither m_audioIO nor m_playTarget.
        }
    }

    if (!m_audioIO) {
        m_playTarget = breakfastquay::AudioFactory::
                createCallbackPlayTarget(m_resamplerWrapper,
                                         preference, errorString);
        if (m_playTarget) {
            SVCERR << "MainWindowBase::createAudioIO: Suspending on creation" << endl;
            m_playTarget->suspend(); // start in suspended state
            m_playSource->setSystemPlaybackTarget(m_playTarget);
        }
    }

    if (!m_playTarget && !m_audioIO) {
        emit hideSplash();
        QString message;
        QString error = errorString.c_str();
        QString firstBit, secondBit;
        if (implementation == "") {
            if (error == "") {
                firstBit = tr("<b>No audio available</b><p>Could not open an audio device.</p>");
            } else {
                firstBit = tr("<b>No audio available</b><p>Could not open audio device: %1</p>").arg(error);
            }
            if (m_audioMode == AUDIO_PLAYBACK_NOW_RECORD_LATER ||
                    m_audioMode == AUDIO_PLAYBACK_AND_RECORD) {
                secondBit = tr("<p>Automatic audio device detection failed. Audio playback and recording will not be available during this session.</p>");
            } else {
                secondBit = tr("<p>Automatic audio device detection failed. Audio playback will not be available during this session.</p>");
            }
        } else {
            QString driverName = breakfastquay::AudioFactory::
                    getImplementationDescription(implementation.toStdString())
                    .c_str();
            if (error == "") {
                firstBit = tr("<b>No audio available</b><p>Failed to open your preferred audio driver (\"%1\").</p>").arg(driverName);
            } else {
                firstBit = tr("<b>No audio available</b><p>Failed to open your preferred audio driver (\"%1\"): %2.</p>").arg(driverName).arg(error);
            }
            if (m_audioMode == AUDIO_PLAYBACK_NOW_RECORD_LATER ||
                    m_audioMode == AUDIO_PLAYBACK_AND_RECORD) {
                secondBit = tr("<p>Audio playback and recording will not be available during this session.</p>");
            } else {
                secondBit = tr("<p>Audio playback will not be available during this session.</p>");
            }
        }
        SVDEBUG << "createAudioIO: ERROR: Failed to open audio device \""
                << implementation << "\": error is: " << error << endl;
        QMessageBox::warning(this, tr("Couldn't open audio device"),
                             firstBit + secondBit, QMessageBox::Ok);
    }
}

void
MainWindowBase::deleteAudioIO()
{
    // First prevent this trying to call target.
    if (m_playSource) {
        m_playSource->setSystemPlaybackTarget(nullptr);
        m_playSource->setResamplerWrapper(nullptr);
    }

    // Then delete the breakfastquay::System object.
    // Only one of these two exists!
    delete m_audioIO;
    delete m_playTarget;

    // And the breakfastquay resampler wrapper. We need to
    // delete/recreate this if the channel count changes, which is one
    // of the use cases for recreateAudioIO() calling this
    delete m_resamplerWrapper;

    m_audioIO = nullptr;
    m_playTarget = nullptr;
    m_resamplerWrapper = nullptr;
}

void
MainWindowBase::recreateAudioIO()
{
    deleteAudioIO();
    createAudioIO();
}

void
MainWindowBase::audioChannelCountIncreased(int)
{
    SVCERR << "MainWindowBase::audioChannelCountIncreased" << endl;
    recreateAudioIO();

    if (m_recordTarget &&
            m_recordTarget->isRecording() &&
            m_audioIO) {
        SVCERR << "MainWindowBase::audioChannelCountIncreased: we were recording already, so resuming IO now" << endl;
        m_audioIO->resume();
    }
}

ModelId
MainWindowBase::getMainModelId() const
{
    if (!m_document) return {};
    return m_document->getMainModel();
}

std::shared_ptr<WaveFileModel>
MainWindowBase::getMainModel() const
{
    return ModelById::getAs<WaveFileModel>(getMainModelId());
}

void
MainWindowBase::createDocument()
{
    m_document = new Document;

    connect(m_document, SIGNAL(layerAdded(Layer *)),
            this, SLOT(layerAdded(Layer *)));
    connect(m_document, SIGNAL(layerRemoved(Layer *)),
            this, SLOT(layerRemoved(Layer *)));
    connect(m_document, SIGNAL(layerAboutToBeDeleted(Layer *)),
            this, SLOT(layerAboutToBeDeleted(Layer *)));
    connect(m_document, SIGNAL(layerInAView(Layer *, bool)),
            this, SLOT(layerInAView(Layer *, bool)));

    connect(m_document, SIGNAL(modelAdded(ModelId )),
            this, SLOT(modelAdded(ModelId )));
    connect(m_document, SIGNAL(mainModelChanged(ModelId)),
            this, SLOT(mainModelChanged(ModelId)));

    connect(m_document, SIGNAL(modelGenerationFailed(QString, QString)),
            this, SLOT(modelGenerationFailed(QString, QString)));
    connect(m_document, SIGNAL(modelRegenerationWarning(QString, QString, QString)),
            this, SLOT(modelRegenerationWarning(QString, QString, QString)));
    connect(m_document, SIGNAL(alignmentComplete(ModelId)),
            this, SLOT(alignmentComplete(ModelId)));
    connect(m_document, SIGNAL(alignmentFailed(QString)),
            this, SLOT(alignmentFailed(QString)));

    m_document->setAutoAlignment(m_viewManager->getAlignMode());

    emit replacedDocument();
}

bool
MainWindowBase::saveSessionFile(QString path)
{
    try {

        TempWriteFile temp(path);

        BZipFileDevice bzFile(temp.getTemporaryFilename());
        if (!bzFile.open(QIODevice::WriteOnly)) {
            cerr << "Failed to open session file \""
                 << temp.getTemporaryFilename()
                 << "\" for writing: "
                 << bzFile.errorString() << endl;
            return false;
        }

        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        QTextStream out(&bzFile);
        out.setCodec(QTextCodec::codecForName("UTF-8"));
        toXml(out, false);
        out.flush();

        QApplication::restoreOverrideCursor();

        if (!bzFile.isOK()) {
            QMessageBox::critical(this, tr("Failed to write file"),
                                  tr("<b>Save failed</b><p>Failed to write to file \"%1\": %2")
                                  .arg(path).arg(bzFile.errorString()));
            bzFile.close();
            return false;
        }

        bzFile.close();
        temp.moveToTarget();
        return true;

    } catch (FileOperationFailed &f) {
        
        QMessageBox::critical(this, tr("Failed to write file"),
                              tr("<b>Save failed</b><p>Failed to write to file \"%1\": %2")
                              .arg(path).arg(f.what()));
        return false;
    }
}

bool
MainWindowBase::saveSessionTemplate(QString path)
{
    try {

        TempWriteFile temp(path);

        QFile file(temp.getTemporaryFilename());
        if (!file.open(QIODevice::WriteOnly)) {
            cerr << "Failed to open session template file \""
                 << temp.getTemporaryFilename()
                 << "\" for writing: "
                 << file.errorString() << endl;
            return false;
        }
        
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

        QTextStream out(&file);
        out.setCodec(QTextCodec::codecForName("UTF-8"));
        toXml(out, true);
        out.flush();

        QApplication::restoreOverrideCursor();

        file.close();
        temp.moveToTarget();
        return true;

    } catch (FileOperationFailed &f) {
        
        QMessageBox::critical(this, tr("Failed to write file"),
                              tr("<b>Save failed</b><p>Failed to write to file \"%1\": %2")
                              .arg(path).arg(f.what()));
        return false;
    }
}

bool
MainWindowBase::exportLayerTo(Layer *layer, QString path, QString &error)
{
    if (QFileInfo(path).suffix() == "") path += ".svl";

    QString suffix = QFileInfo(path).suffix().toLower();

    auto model = ModelById::get(layer->getModel());
    qDebug() << "JPMAUS Model Title: " <<  model->getTitle();
    if (!model) {
        error = tr("Internal error: unknown model");
        return false;
    }

    if (suffix == "xml" || suffix == "svl") {

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            error = tr("Failed to open file %1 for writing").arg(path);
        } else {
            QTextStream out(&file);
            out.setCodec(QTextCodec::codecForName("UTF-8"));
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                << "<!DOCTYPE sonic-visualiser>\n"
                << "<sv>\n"
                << "  <data>\n";
            // JPMAUS model contains Tempo details
            model->toXml(out, "    ");

            out << "  </data>\n"
                << "  <display>\n";

            layer->toXml(out, "    ");

            out << "  </display>\n"
                << "</sv>\n";
        }

    } else if (suffix == "mid" || suffix == "midi") {

        auto nm = ModelById::getAs<NoteModel>(layer->getModel());

        if (!nm) {
            error = tr("Can't export non-note layers to MIDI");
            // JPMAUS they not returning anythig!
            return false;
        } else {
            // JPMAUS Extract Tempo from Layer. // Tempo is stored in Label.

            EventVector events = nm->getAllEvents();
            float average=0;
            int count=0;
            for (auto event : events){
                qDebug() << "JPMAUS Event: " << event.getLabel();
                if(event.hasLabel()){
                    QString tempoStr = event.getLabel();
                    if(tempoStr.endsWith("bpm", Qt::CaseInsensitive)){
                        tempoStr.remove("bpm",Qt::CaseInsensitive);
                    }
                    count++;
                    average += tempoStr.toFloat();
                }
            }
            float tempo=140.0;
            if( count > 0)
                tempo = average/count;
            qDebug() << "JPMAUS finaly stored Tempo = " << tempo;
            // End Tempo
            MIDIFileWriter writer(path, nm.get(), nm->getSampleRate(), tempo);
            //MIDIFileWriter writer(path, nm.get(), nm->getSampleRate(), 120.0f);
            writer.write();
            if (!writer.isOK()) {
                error = writer.getError();
            }
        }

    } else if (suffix == "ttl" || suffix == "n3") {

        if (!RDFExporter::canExportModel(model.get())) {
            error = tr("Sorry, cannot export this layer type to RDF (supported types are: region, note, text, time instants, time values)");
        } else {
            RDFExporter exporter(path, model.get());
            exporter.write();
            if (!exporter.isOK()) {
                error = exporter.getError();
            }
        }

    } else {

        CSVFileWriter writer(path, model.get(),
                             ((suffix == "csv") ? "," : "\t"));
        writer.write();

        if (!writer.isOK()) {
            error = writer.getError();
        }
    }

    return (error == "");
}

void
MainWindowBase::toXml(QTextStream &out, bool asTemplate)
{
    QString indent("  ");

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<!DOCTYPE sonic-visualiser>\n";
    out << "<sv>\n";

    if (asTemplate) {
        m_document->toXmlAsTemplate(out, "", "");
    } else {
        m_document->toXml(out, "", "");
    }

    out << "<display>\n";

    out << QString("  <window width=\"%1\" height=\"%2\"/>\n")
           .arg(width()).arg(height());

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);

        if (pane) {
            pane->toXml(out, indent);
        }
    }

    out << "</display>\n";

    m_viewManager->getSelection().toXml(out);

    out << "</sv>\n";
}

Pane *
MainWindowBase::addPaneToStack()
{
    cerr << "MainWindowBase::addPaneToStack()" << endl;
    AddPaneCommand *command = new AddPaneCommand(this);
    CommandHistory::getInstance()->addCommand(command);
    Pane *pane = command->getPane();
    return pane;
}

void
MainWindowBase::zoomIn()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->zoom(true);
}

void
MainWindowBase::zoomOut()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->zoom(false);
}

void
MainWindowBase::zoomToFit()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    auto model = getMainModel();
    if (!model) return;
    
    sv_frame_t start = model->getStartFrame();
    sv_frame_t end = model->getEndFrame();
    if (m_playSource) end = std::max(end, m_playSource->getPlayEndFrame());
    int pixels = currentPane->width();

    int sw = currentPane->getVerticalScaleWidth();
    if (pixels > sw * 2) pixels -= sw * 2;
    else pixels = 1;
    if (pixels > 4) pixels -= 4;

    ZoomLevel zoomLevel = ZoomLevel::fromRatio(pixels, end - start);
    currentPane->setZoomLevel(zoomLevel);
    currentPane->setCentreFrame((start + end) / 2);
}

void
MainWindowBase::zoomDefault()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    QSettings settings;
    settings.beginGroup("MainWindow");
    int zoom = settings.value("zoom-default", 1024).toInt();
    settings.endGroup();
    if (currentPane) {
        currentPane->setZoomLevel(ZoomLevel(ZoomLevel::FramesPerPixel, zoom));
    }
}

void
MainWindowBase::scrollLeft()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(false, false);
}

void
MainWindowBase::jumpLeft()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(false, true);
}

void
MainWindowBase::peekLeft()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(false, false, false);
}

void
MainWindowBase::scrollRight()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(true, false);
}

void
MainWindowBase::jumpRight()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(true, true);
}

void
MainWindowBase::peekRight()
{
    Pane *currentPane = m_paneStack->getCurrentPane();
    if (currentPane) currentPane->scroll(true, false, false);
}

void
MainWindowBase::showNoOverlays()
{
    m_viewManager->setOverlayMode(ViewManager::NoOverlays);
}

void
MainWindowBase::showMinimalOverlays()
{
    m_viewManager->setOverlayMode(ViewManager::StandardOverlays);
}

void
MainWindowBase::showAllOverlays()
{
    m_viewManager->setOverlayMode(ViewManager::AllOverlays);
}

void
MainWindowBase::findTimeRulerLayer()
{
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;
        for (int j = 0; j < pane->getLayerCount(); ++j) {
            Layer *layer = pane->getLayer(j);
            if (!dynamic_cast<TimeRulerLayer *>(layer)) continue;
            m_timeRulerLayer = layer;
            return;
        }
    }
    if (m_timeRulerLayer) {
        SVCERR << "WARNING: Time ruler layer was not reset to 0 before session template loaded?" << endl;
        delete m_timeRulerLayer;
        m_timeRulerLayer = nullptr;
    }
}

void
MainWindowBase::toggleTimeRulers()
{
    bool haveRulers = false;
    bool someHidden = false;

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

        Pane *pane = m_paneStack->getPane(i);
        if (!pane) continue;

        for (int j = 0; j < pane->getLayerCount(); ++j) {

            Layer *layer = pane->getLayer(j);
            if (!dynamic_cast<TimeRulerLayer *>(layer)) continue;

            haveRulers = true;
            if (layer->isLayerDormant(pane)) someHidden = true;
        }
    }

    if (haveRulers) {

        bool show = someHidden;

        for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {

            Pane *pane = m_paneStack->getPane(i);
            if (!pane) continue;

            for (int j = 0; j < pane->getLayerCount(); ++j) {

                Layer *layer = pane->getLayer(j);
                if (!dynamic_cast<TimeRulerLayer *>(layer)) continue;

                layer->showLayer(pane, show);
            }
        }
    }
}

void
MainWindowBase::toggleZoomWheels()
{
    if (m_viewManager->getZoomWheelsEnabled()) {
        m_viewManager->setZoomWheelsEnabled(false);
    } else {
        m_viewManager->setZoomWheelsEnabled(true);
    }
}

void
MainWindowBase::togglePropertyBoxes()
{
    if (m_paneStack->getLayoutStyle() == PaneStack::HiddenPropertyStacksLayout) {
        if (Preferences::getInstance()->getPropertyBoxLayout() ==
                Preferences::VerticallyStacked) {
            m_paneStack->setLayoutStyle(PaneStack::PropertyStackPerPaneLayout);
        } else {
            m_paneStack->setLayoutStyle(PaneStack::SinglePropertyStackLayout);
        }
    } else {
        m_paneStack->setLayoutStyle(PaneStack::HiddenPropertyStacksLayout);
    }
}

QLabel *
MainWindowBase::getStatusLabel() const
{
    if (!m_statusLabel) {
        m_statusLabel = new QLabel();
        statusBar()->addWidget(m_statusLabel, 1);
    }

    QList<QFrame *> frames = statusBar()->findChildren<QFrame *>();
    foreach (QFrame *f, frames) {
        f->setFrameStyle(QFrame::NoFrame);
    }

    return m_statusLabel;
}

void
MainWindowBase::toggleStatusBar()
{
    QSettings settings;
    settings.beginGroup("MainWindow");
    bool sb = settings.value("showstatusbar", true).toBool();

    if (sb) {
        statusBar()->hide();
    } else {
        statusBar()->show();
    }

    settings.setValue("showstatusbar", !sb);

    settings.endGroup();
}

void
MainWindowBase::toggleCentreLine()
{
    if (m_viewManager->shouldShowCentreLine()) {
        m_viewManager->setShowCentreLine(false);
    } else {
        m_viewManager->setShowCentreLine(true);
    }
}

void
MainWindowBase::preferenceChanged(PropertyContainer::PropertyName name)
{
    if (name == "Property Box Layout") {
        if (m_paneStack->getLayoutStyle() != PaneStack::HiddenPropertyStacksLayout) {
            if (Preferences::getInstance()->getPropertyBoxLayout() ==
                    Preferences::VerticallyStacked) {
                m_paneStack->setLayoutStyle(PaneStack::PropertyStackPerPaneLayout);
            } else {
                m_paneStack->setLayoutStyle(PaneStack::SinglePropertyStackLayout);
            }
        }
    } else if (name == "Background Mode" && m_viewManager) {
        Preferences::BackgroundMode mode =
                Preferences::getInstance()->getBackgroundMode();
        if (mode == Preferences::BackgroundFromTheme) {
            m_viewManager->setGlobalDarkBackground(m_initialDarkBackground);
        } else if (mode == Preferences::DarkBackground) {
            m_viewManager->setGlobalDarkBackground(true);
        } else {
            m_viewManager->setGlobalDarkBackground(false);
        }
    }
}

void
MainWindowBase::play()
{
    if ((m_recordTarget && m_recordTarget->isRecording()) ||
            (m_playSource && m_playSource->isPlaying())) {
        stop();
        QAction *action = qobject_cast<QAction *>(sender());
        if (action) action->setChecked(false);
    } else {
        if (m_audioIO) m_audioIO->resume();
        else if (m_playTarget) m_playTarget->resume();
        playbackFrameChanged(m_viewManager->getPlaybackFrame());
        m_playSource->play(m_viewManager->getPlaybackFrame());
    }
}

void
MainWindowBase::record()
{
    QAction *action = qobject_cast<QAction *>(sender());
    
    if (m_audioMode == AUDIO_NONE || m_audioMode == AUDIO_PLAYBACK_ONLY) {
        if (action) action->setChecked(false);
        return;
    }

    if (!m_recordTarget) {
        if (action) action->setChecked(false);
        return;
    }

    if (m_audioMode == AUDIO_PLAYBACK_NOW_RECORD_LATER) {
        SVDEBUG << "MainWindowBase::record: upgrading from "
                << "AUDIO_PLAYBACK_NOW_RECORD_LATER to "
                << "AUDIO_PLAYBACK_AND_RECORD" << endl;
        m_audioMode = AUDIO_PLAYBACK_AND_RECORD;
        deleteAudioIO();
    }

    if (!m_audioIO) {
        SVDEBUG << "MainWindowBase::record: about to create audio IO" << endl;
        createAudioIO();
    }

    if (!m_audioIO) {
        if (!m_playTarget) {
            // Don't need to report this, createAudioIO should have
            if (action) action->setChecked(false);
            return;
        } else {
            // Need to report this: if the play target exists instead
            // of the audio IO, then that means we failed to open a
            // capture device. The record control should be disabled
            // in that situation, so if it happens here, that must
            // mean this is the first time we ever tried to open the
            // audio device, hence the need to report the problem here
            QMessageBox::critical
                    (this, tr("No record device available"),
                     tr("<b>No record device available</b><p>Failed to find or open an audio device for recording. Only playback will be available.</p>"));
            if (action) action->setChecked(false);
            updateMenuStates();
            return;
        }
    }
    
    if (m_recordTarget->isRecording()) {
        stop();
        return;
    }
    
    if (m_audioRecordMode == RecordReplaceSession) {
        if (!checkSaveModified()) {
            if (action) action->setChecked(false);
            return;
        }
    }

    if (m_viewManager) m_viewManager->setGlobalCentreFrame(0);
    
    SVCERR << "MainWindowBase::record: about to resume" << endl;
    m_audioIO->resume();

    WritableWaveFileModel *modelPtr = m_recordTarget->startRecording();
    if (!modelPtr) {
        SVCERR << "ERROR: MainWindowBase::record: Recording failed" << endl;
        QMessageBox::critical
                (this, tr("Recording failed"),
                 tr("<b>Recording failed</b><p>Failed to switch to record mode (some internal problem?)</p>"));
        if (action) action->setChecked(false);
        return;
    }

    if (!modelPtr->isOK()) {
        SVCERR << "MainWindowBase::record: Model not OK, stopping and suspending" << endl;
        m_recordTarget->stopRecording();
        m_audioIO->suspend();
        if (action) action->setChecked(false);
        delete modelPtr;
        return;
    }

    SVCERR << "MainWindowBase::record: Model is OK, continuing..." << endl;

    QString location = modelPtr->getLocation();
    
    auto modelId = ModelById::add(std::shared_ptr<Model>(modelPtr));

    if (m_audioRecordMode == RecordReplaceSession || !getMainModel()) {

        //!!! duplication with openAudio here
        
        QString templateName = getDefaultSessionTemplate();
        bool loadedTemplate = false;
        
        if (templateName != "") {
            FileOpenStatus tplStatus = openSessionTemplate(templateName);
            if (tplStatus == FileOpenCancelled) {
                SVCERR << "MainWindowBase::record: Session template open cancelled, stopping and suspending" << endl;
                m_recordTarget->stopRecording();
                m_audioIO->suspend();
                ModelById::release(modelId);
                return;
            }
            if (tplStatus != FileOpenFailed) {
                loadedTemplate = true;
            }
        }

        if (!loadedTemplate) {
            closeSession();
            createDocument();
        }
        
        ModelId prevMain = getMainModelId();
        if (!prevMain.isNone()) {
            m_playSource->removeModel(prevMain);
        }
        
        m_document->setMainModel(modelId);
        setupMenus();
        findTimeRulerLayer();

        m_originalLocation = location;
        
        if (loadedTemplate || (m_sessionFile == "")) {
            CommandHistory::getInstance()->clear();
            CommandHistory::getInstance()->documentSaved();
        }

        m_documentModified = false;
        updateWindowTitle();

    } else {

        CommandHistory::getInstance()->startCompoundOperation
                (tr("Import Recorded Audio"), true);

        m_document->addNonDerivedModel(modelId);

        AddPaneCommand *command = new AddPaneCommand(this);
        CommandHistory::getInstance()->addCommand(command);

        Pane *pane = command->getPane();

        if (m_timeRulerLayer) {
            m_document->addLayerToView(pane, m_timeRulerLayer);
        }

        Layer *newLayer = m_document->createImportedLayer(modelId);

        if (newLayer) {
            m_document->addLayerToView(pane, newLayer);
        }
        
        CommandHistory::getInstance()->endCompoundOperation();
    }

    updateMenuStates();
    m_recentFiles.addFile(location);
    currentPaneChanged(m_paneStack->getCurrentPane());
    
    emit audioFileLoaded();
}

void
MainWindowBase::ffwd()
{
    if (!getMainModel()) return;

    sv_frame_t playbackFrame = m_viewManager->getPlaybackFrame();
    sv_frame_t frame = playbackFrame + 1;

    Pane *pane = m_paneStack->getCurrentPane();
    Layer *layer = getSnapLayer();
    sv_samplerate_t sr = getMainModel()->getSampleRate();

    if (!pane || !layer) {

        frame = RealTime::realTime2Frame
                (RealTime::frame2RealTime(frame, sr) + m_defaultFfwdRwdStep, sr);
        if (frame > getMainModel()->getEndFrame()) {
            frame = getMainModel()->getEndFrame();
        }

    } else {

        sv_frame_t pframe = pane->alignFromReference(frame);
        int resolution = 0;
        bool success = false;

        while (layer->snapToFeatureFrame(pane, pframe, resolution,
                                         Layer::SnapRight, -1)) {
            if (pane->alignToReference(pframe) > playbackFrame) {
                success = true;
                break;
            } else {
                ++pframe;
            }
        }

        if (success) {
            frame = pane->alignToReference(pframe);
        } else {
            frame = getMainModel()->getEndFrame();
        }
    }

    if (frame < 0) frame = 0;

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }
    
    m_viewManager->setPlaybackFrame(frame);

    if (frame >= getMainModel()->getEndFrame() &&
            m_playSource &&
            m_playSource->isPlaying() &&
            !m_viewManager->getPlayLoopMode()) {
        stop();
    }
}

void
MainWindowBase::ffwdEnd()
{
    if (!getMainModel()) return;

    if (m_playSource &&
            m_playSource->isPlaying() &&
            !m_viewManager->getPlayLoopMode()) {
        stop();
    }

    sv_frame_t frame = getMainModel()->getEndFrame();

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }

    m_viewManager->setPlaybackFrame(frame);
}

void
MainWindowBase::ffwdSimilar()
{
    if (!getMainModel()) return;

    Layer *layer = getSnapLayer();
    if (!layer) { ffwd(); return; }

    Pane *pane = m_paneStack->getCurrentPane();
    sv_frame_t frame = m_viewManager->getPlaybackFrame();

    int resolution = 0;
    if (pane) frame = pane->alignFromReference(frame);
    if (layer->snapToSimilarFeature(m_paneStack->getCurrentPane(),
                                    frame, resolution, Layer::SnapRight)) {
        if (pane) frame = pane->alignToReference(frame);
    } else {
        frame = getMainModel()->getEndFrame();
    }

    if (frame < 0) frame = 0;

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }
    
    m_viewManager->setPlaybackFrame(frame);

    if (frame == getMainModel()->getEndFrame() &&
            m_playSource &&
            m_playSource->isPlaying() &&
            !m_viewManager->getPlayLoopMode()) {
        stop();
    }
}

void
MainWindowBase::rewind()
{
    if (!getMainModel()) return;

    sv_frame_t playbackFrame = m_viewManager->getPlaybackFrame();
    sv_frame_t frame = playbackFrame;
    if (frame > 0) --frame;

    Pane *pane = m_paneStack->getCurrentPane();
    Layer *layer = getSnapLayer();
    sv_samplerate_t sr = getMainModel()->getSampleRate();
    
    // when rewinding during playback, we want to allow a period
    // following a rewind target point at which the rewind will go to
    // the prior point instead of the immediately neighbouring one
    if (m_playSource && m_playSource->isPlaying()) {
        RealTime ct = RealTime::frame2RealTime(frame, sr);
        ct = ct - RealTime::fromSeconds(0.15);
        if (ct < RealTime::zeroTime) ct = RealTime::zeroTime;
        frame = RealTime::realTime2Frame(ct, sr);
    }

    if (!pane || !layer) {
        
        frame = RealTime::realTime2Frame
                (RealTime::frame2RealTime(frame, sr) - m_defaultFfwdRwdStep, sr);
        if (frame < getMainModel()->getStartFrame()) {
            frame = getMainModel()->getStartFrame();
        }

    } else {

        sv_frame_t pframe = pane->alignFromReference(frame);
        int resolution = 0;
        bool success = false;

        while (layer->snapToFeatureFrame(pane, pframe, resolution,
                                         Layer::SnapLeft, -1)) {
            if (pane->alignToReference(pframe) < playbackFrame ||
                    pframe <= 0) {
                success = true;
                break;
            } else {
                --pframe;
            }
        }

        if (success) {
            frame = pane->alignToReference(pframe);
        } else {
            frame = getMainModel()->getStartFrame();
        }
    }

    if (frame < 0) frame = 0;

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }

    m_viewManager->setPlaybackFrame(frame);
}

void
MainWindowBase::rewindStart()
{
    if (!getMainModel()) return;

    sv_frame_t frame = getMainModel()->getStartFrame();

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }

    m_viewManager->setPlaybackFrame(frame);
}

void
MainWindowBase::rewindSimilar()
{
    if (!getMainModel()) return;

    Layer *layer = getSnapLayer();
    if (!layer) { rewind(); return; }

    Pane *pane = m_paneStack->getCurrentPane();
    sv_frame_t frame = m_viewManager->getPlaybackFrame();

    int resolution = 0;
    if (pane) frame = pane->alignFromReference(frame);
    if (layer->snapToSimilarFeature(m_paneStack->getCurrentPane(),
                                    frame, resolution, Layer::SnapLeft)) {
        if (pane) frame = pane->alignToReference(frame);
    } else {
        frame = getMainModel()->getStartFrame();
    }

    if (frame < 0) frame = 0;

    if (m_viewManager->getPlaySelectionMode()) {
        frame = m_viewManager->constrainFrameToSelection(frame);
    }
    
    m_viewManager->setPlaybackFrame(frame);
}

Layer *
MainWindowBase::getSnapLayer() const
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (!pane) return nullptr;

    Layer *layer = pane->getSelectedLayer();

    if (!dynamic_cast<TimeInstantLayer *>(layer) &&
            !dynamic_cast<TimeValueLayer *>(layer) &&
            !dynamic_cast<RegionLayer *>(layer) &&
            !dynamic_cast<TimeRulerLayer *>(layer)) {

        layer = nullptr;

        for (int i = pane->getLayerCount(); i > 0; --i) {
            Layer *l = pane->getLayer(i-1);
            if (dynamic_cast<TimeRulerLayer *>(l)) {
                layer = l;
                break;
            }
        }
    }

    return layer;
}

void
MainWindowBase::stop()
{
    if (m_recordTarget &&
            m_recordTarget->isRecording()) {
        m_recordTarget->stopRecording();
    }

    if (!m_playSource) return;
    
    m_playSource->stop();

    SVCERR << "MainWindowBase::stop: suspending" << endl;
    
    if (m_audioIO) m_audioIO->suspend();
    else if (m_playTarget) m_playTarget->suspend();
    
    if (m_paneStack && m_paneStack->getCurrentPane()) {
        updateVisibleRangeDisplay(m_paneStack->getCurrentPane());
    } else {
        m_myStatusMessage = "";
        getStatusLabel()->setText("");
    }
}

MainWindowBase::AddPaneCommand::AddPaneCommand(MainWindowBase *mw) :
    m_mw(mw),
    m_pane(nullptr),
    m_prevCurrentPane(nullptr),
    m_added(false)
{
}

MainWindowBase::AddPaneCommand::~AddPaneCommand()
{
    if (m_pane && !m_added) {
        m_mw->m_paneStack->deletePane(m_pane);
    }
}

QString
MainWindowBase::AddPaneCommand::getName() const
{
    return tr("Add Pane");
}

void
MainWindowBase::AddPaneCommand::execute()
{
    if (!m_pane) {
        m_prevCurrentPane = m_mw->m_paneStack->getCurrentPane();
        m_pane = m_mw->m_paneStack->addPane();

        connect(m_pane, SIGNAL(contextHelpChanged(const QString &)),
                m_mw, SLOT(contextHelpChanged(const QString &)));
    } else {
        m_mw->m_paneStack->showPane(m_pane);
    }

    m_mw->m_paneStack->setCurrentPane(m_pane);
    m_added = true;
}

void
MainWindowBase::AddPaneCommand::unexecute()
{
    m_mw->m_paneStack->hidePane(m_pane);
    m_mw->m_paneStack->setCurrentPane(m_prevCurrentPane);
    m_added = false;
}

MainWindowBase::RemovePaneCommand::RemovePaneCommand(MainWindowBase *mw, Pane *pane) :
    m_mw(mw),
    m_pane(pane),
    m_prevCurrentPane(nullptr),
    m_added(true)
{
}

MainWindowBase::RemovePaneCommand::~RemovePaneCommand()
{
    if (m_pane && !m_added) {
        m_mw->m_paneStack->deletePane(m_pane);
    }
}

QString
MainWindowBase::RemovePaneCommand::getName() const
{
    return tr("Remove Pane");
}

void
MainWindowBase::RemovePaneCommand::execute()
{
    m_prevCurrentPane = m_mw->m_paneStack->getCurrentPane();
    m_mw->m_paneStack->hidePane(m_pane);
    m_added = false;
}

void
MainWindowBase::RemovePaneCommand::unexecute()
{
    m_mw->m_paneStack->showPane(m_pane);
    m_mw->m_paneStack->setCurrentPane(m_prevCurrentPane);
    m_added = true;
}

void
MainWindowBase::deleteCurrentPane()
{
    CommandHistory::getInstance()->startCompoundOperation
            (tr("Delete Pane"), true);

    Pane *pane = m_paneStack->getCurrentPane();
    if (pane) {
        while (pane->getLayerCount() > 0) {
            Layer *layer = pane->getLayer(0);
            if (layer) {
                m_document->removeLayerFromView(pane, layer);
            } else {
                break;
            }
        }

        RemovePaneCommand *command = new RemovePaneCommand(this, pane);
        CommandHistory::getInstance()->addCommand(command);
    }

    CommandHistory::getInstance()->endCompoundOperation();

    updateMenuStates();
}

void
MainWindowBase::deleteCurrentLayer()
{
    Pane *pane = m_paneStack->getCurrentPane();
    if (pane) {
        Layer *layer = pane->getSelectedLayer();
        if (layer) {
            m_document->removeLayerFromView(pane, layer);
        }
    }
    updateMenuStates();
}

void
MainWindowBase::editCurrentLayer()
{
    Layer *layer = nullptr;
    Pane *pane = m_paneStack->getCurrentPane();
    if (pane) layer = pane->getSelectedLayer();
    if (!layer) return;

    auto tabular = ModelById::getAs<TabularModel>(layer->getModel());
    if (!tabular) {
        //!!! how to prevent this function from being active if not
        //appropriate model type?  or will we ultimately support
        //tabular display for all editable models?
        SVDEBUG << "NOTE: Not a tabular model" << endl;
        return;
    }

    if (m_layerDataDialogMap.find(layer) != m_layerDataDialogMap.end()) {
        if (!m_layerDataDialogMap[layer].isNull()) {
            m_layerDataDialogMap[layer]->show();
            m_layerDataDialogMap[layer]->raise();
            return;
        }
    }

    QString title = layer->getLayerPresentationName();

    ModelDataTableDialog *dialog = new ModelDataTableDialog
            (layer->getModel(), title, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    connectLayerEditDialog(dialog);

    m_layerDataDialogMap[layer] = dialog;
    m_viewDataDialogMap[pane].insert(dialog);

    dialog->show();
}

void
MainWindowBase::connectLayerEditDialog(ModelDataTableDialog *dialog)
{
    connect(m_viewManager,
            SIGNAL(globalCentreFrameChanged(sv_frame_t)),
            dialog,
            SLOT(userScrolledToFrame(sv_frame_t)));

    connect(m_viewManager,
            SIGNAL(playbackFrameChanged(sv_frame_t)),
            dialog,
            SLOT(playbackScrolledToFrame(sv_frame_t)));

    connect(dialog,
            SIGNAL(scrollToFrame(sv_frame_t)),
            m_viewManager,
            SLOT(setGlobalCentreFrame(sv_frame_t)));

    connect(dialog,
            SIGNAL(scrollToFrame(sv_frame_t)),
            m_viewManager,
            SLOT(setPlaybackFrame(sv_frame_t)));
}    

void
MainWindowBase::previousPane()
{
    if (!m_paneStack) return;

    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        if (m_paneStack->getPane(i) == currentPane) {
            if (i == 0) return;
            m_paneStack->setCurrentPane(m_paneStack->getPane(i-1));
            updateMenuStates();
            return;
        }
    }
}

void
MainWindowBase::nextPane()
{
    if (!m_paneStack) return;

    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        if (m_paneStack->getPane(i) == currentPane) {
            if (i == m_paneStack->getPaneCount()-1) return;
            m_paneStack->setCurrentPane(m_paneStack->getPane(i+1));
            updateMenuStates();
            return;
        }
    }
}

void
MainWindowBase::previousLayer()
{
    if (!m_paneStack) return;

    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    int count = currentPane->getLayerCount();
    if (count == 0) return;

    Layer *currentLayer = currentPane->getSelectedLayer();

    if (!currentLayer) {
        // The pane itself is current
        m_paneStack->setCurrentLayer
                (currentPane, currentPane->getFixedOrderLayer(count-1));
    } else {
        for (int i = 0; i < count; ++i) {
            if (currentPane->getFixedOrderLayer(i) == currentLayer) {
                if (i == 0) {
                    m_paneStack->setCurrentLayer
                            (currentPane, nullptr); // pane
                } else {
                    m_paneStack->setCurrentLayer
                            (currentPane, currentPane->getFixedOrderLayer(i-1));
                }
                break;
            }
        }
    }

    updateMenuStates();
}

void
MainWindowBase::nextLayer()
{
    if (!m_paneStack) return;

    Pane *currentPane = m_paneStack->getCurrentPane();
    if (!currentPane) return;

    int count = currentPane->getLayerCount();
    if (count == 0) return;

    Layer *currentLayer = currentPane->getSelectedLayer();

    if (!currentLayer) {
        // The pane itself is current
        m_paneStack->setCurrentLayer
                (currentPane, currentPane->getFixedOrderLayer(0));
    } else {
        for (int i = 0; i < count; ++i) {
            if (currentPane->getFixedOrderLayer(i) == currentLayer) {
                if (i == currentPane->getLayerCount()-1) {
                    m_paneStack->setCurrentLayer
                            (currentPane, nullptr); // pane
                } else {
                    m_paneStack->setCurrentLayer
                            (currentPane, currentPane->getFixedOrderLayer(i+1));
                }
                break;
            }
        }
    }

    updateMenuStates();
}

void
MainWindowBase::playbackFrameChanged(sv_frame_t frame)
{
    if (!(m_playSource && m_playSource->isPlaying()) || !getMainModel()) return;

    updatePositionStatusDisplays();

    RealTime now = RealTime::frame2RealTime
            (frame, getMainModel()->getSampleRate());

    if (now.sec == m_lastPlayStatusSec) return;

    RealTime then = RealTime::frame2RealTime
            (m_playSource->getPlayEndFrame(), getMainModel()->getSampleRate());

    QString nowStr;
    QString thenStr;
    QString remainingStr;

    if (then.sec > 10) {
        nowStr = now.toSecText().c_str();
        thenStr = then.toSecText().c_str();
        remainingStr = (then - now).toSecText().c_str();
        m_lastPlayStatusSec = now.sec;
    } else {
        nowStr = now.toText(true).c_str();
        thenStr = then.toText(true).c_str();
        remainingStr = (then - now).toText(true).c_str();
    }

    m_myStatusMessage = tr("Playing: %1 of %2 (%3 remaining)")
            .arg(nowStr).arg(thenStr).arg(remainingStr);

    getStatusLabel()->setText(m_myStatusMessage);
}

void
MainWindowBase::recordDurationChanged(sv_frame_t frame, sv_samplerate_t rate)
{
    RealTime duration = RealTime::frame2RealTime(frame, rate);
    QString durStr = duration.toSecText().c_str();
    
    m_myStatusMessage = tr("Recording: %1").arg(durStr);

    getStatusLabel()->setText(m_myStatusMessage);
}

void
MainWindowBase::globalCentreFrameChanged(sv_frame_t )
{
    if ((m_playSource && m_playSource->isPlaying()) || !getMainModel()) return;
    Pane *p = nullptr;
    if (!m_paneStack || !(p = m_paneStack->getCurrentPane())) return;
    if (!p->getFollowGlobalPan()) return;
    updateVisibleRangeDisplay(p);
}

void
MainWindowBase::viewCentreFrameChanged(View *v, sv_frame_t frame)
{
    //    SVDEBUG << "MainWindowBase::viewCentreFrameChanged(" << v << "," << frame << ")" << endl;

    if (m_viewDataDialogMap.find(v) != m_viewDataDialogMap.end()) {
        for (DataDialogSet::iterator i = m_viewDataDialogMap[v].begin();
             i != m_viewDataDialogMap[v].end(); ++i) {
            (*i)->userScrolledToFrame(frame);
        }
    }
    if ((m_playSource && m_playSource->isPlaying()) || !getMainModel()) return;
    Pane *p = nullptr;
    if (!m_paneStack || !(p = m_paneStack->getCurrentPane())) return;
    if (v == p) updateVisibleRangeDisplay(p);
}

void
MainWindowBase::viewZoomLevelChanged(View *v, ZoomLevel, bool )
{
    if ((m_playSource && m_playSource->isPlaying()) || !getMainModel()) return;
    Pane *p = nullptr;
    if (!m_paneStack || !(p = m_paneStack->getCurrentPane())) return;
    if (v == p) updateVisibleRangeDisplay(p);
}

void
MainWindowBase::layerAdded(Layer *)
{
    //    SVDEBUG << "MainWindowBase::layerAdded(" << layer << ")" << endl;
    updateMenuStates();
}

void
MainWindowBase::layerRemoved(Layer *)
{
    //    SVDEBUG << "MainWindowBase::layerRemoved(" << layer << ")" << endl;
    updateMenuStates();
}

void
MainWindowBase::layerAboutToBeDeleted(Layer *layer)
{
    //    SVDEBUG << "MainWindowBase::layerAboutToBeDeleted(" << layer << ")" << endl;

    removeLayerEditDialog(layer);

    if (m_timeRulerLayer && (layer == m_timeRulerLayer)) {
        //        cerr << "(this is the time ruler layer)" << endl;
        m_timeRulerLayer = nullptr;
    }
}

void
MainWindowBase::layerInAView(Layer *layer, bool inAView)
{
    //    SVDEBUG << "MainWindowBase::layerInAView(" << layer << "," << inAView << ")" << endl;

    if (!inAView) removeLayerEditDialog(layer);

    // Check whether we need to add or remove model from play source
    ModelId modelId = layer->getModel();
    if (!modelId.isNone()) {
        if (inAView) {
            m_playSource->addModel(modelId);
        } else {
            bool found = false;
            for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
                Pane *pane = m_paneStack->getPane(i);
                if (!pane) continue;
                for (int j = 0; j < pane->getLayerCount(); ++j) {
                    Layer *pl = pane->getLayer(j);
                    if (pl &&
                            !dynamic_cast<TimeRulerLayer *>(pl) &&
                            (pl->getModel() == modelId)) {
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (!found) {
                m_playSource->removeModel(modelId);
            }
        }
    }

    updateMenuStates();
}

void
MainWindowBase::removeLayerEditDialog(Layer *layer)
{
    if (m_layerDataDialogMap.find(layer) != m_layerDataDialogMap.end()) {

        ModelDataTableDialog *dialog = m_layerDataDialogMap[layer];

        for (ViewDataDialogMap::iterator vi = m_viewDataDialogMap.begin();
             vi != m_viewDataDialogMap.end(); ++vi) {
            vi->second.erase(dialog);
        }

        m_layerDataDialogMap.erase(layer);
        delete dialog;
    }
}

void
MainWindowBase::modelAdded(ModelId model)
{
    //    SVDEBUG << "MainWindowBase::modelAdded(" << model << ")" << endl;
    std::cerr << "\nAdding model " << model << " to playsource " << std::endl;
    m_playSource->addModel(model);
}

void
MainWindowBase::mainModelChanged(ModelId modelId)
{
    //    SVDEBUG << "MainWindowBase::mainModelChanged(" << model << ")" << endl;
    updateDescriptionLabel();
    auto model = ModelById::getAs<WaveFileModel>(modelId);
    if (model) m_viewManager->setMainModelSampleRate(model->getSampleRate());
    if (model && !(m_playTarget || m_audioIO) && (m_audioMode != AUDIO_NONE)) {
        createAudioIO();
    }
}

void
MainWindowBase::paneDeleteButtonClicked(Pane *pane)
{
    bool found = false;
    for (int i = 0; i < m_paneStack->getPaneCount(); ++i) {
        if (m_paneStack->getPane(i) == pane) {
            found = true;
            break;
        }
    }
    if (!found) {
        SVDEBUG << "MainWindowBase::paneDeleteButtonClicked: Unknown pane "
                << pane << endl;
        return;
    }

    CommandHistory::getInstance()->startCompoundOperation
            (tr("Delete Pane"), true);

    while (pane->getLayerCount() > 0) {
        Layer *layer = pane->getLayer(pane->getLayerCount() - 1);
        if (layer) {
            m_document->removeLayerFromView(pane, layer);
        } else {
            break;
        }
    }

    RemovePaneCommand *command = new RemovePaneCommand(this, pane);
    CommandHistory::getInstance()->addCommand(command);

    CommandHistory::getInstance()->endCompoundOperation();

    updateMenuStates();
}

void
MainWindowBase::alignmentComplete(ModelId alignmentModelId)
{
    cerr << "MainWindowBase::alignmentComplete(" << alignmentModelId << ")" << endl;
}

void
MainWindowBase::pollOSC()
{
    if (!m_oscQueue || m_oscQueue->isEmpty()) return;
    SVDEBUG << "MainWindowBase::pollOSC: have " << m_oscQueue->getMessagesAvailable() << " messages" << endl;

    if (m_openingAudioFile) return;

    OSCMessage message = m_oscQueue->readMessage();

    if (message.getTarget() != 0) {
        return; //!!! for now -- this class is target 0, others not handled yet
    }

    handleOSCMessage(message);
}

void
MainWindowBase::inProgressSelectionChanged()
{
    Pane *currentPane = nullptr;
    if (m_paneStack) currentPane = m_paneStack->getCurrentPane();
    if (currentPane) {
        //cerr << "JTEST: mouse event on selection pane" << endl;
        updateVisibleRangeDisplay(currentPane);
    }
}

void
MainWindowBase::contextHelpChanged(const QString &s)
{
    QLabel *lab = getStatusLabel();

    if (s == "" && m_myStatusMessage != "") {
        if (lab->text() != m_myStatusMessage) {
            lab->setText(m_myStatusMessage);
        }
        return;
    }

    lab->setText(s);
}

void
MainWindowBase::openHelpUrl(QString url)
{
    // This method mostly lifted from Qt Assistant source code

    QProcess *process = new QProcess(this);
    connect(process, SIGNAL(finished(int)), process, SLOT(deleteLater()));

    QStringList args;

#ifdef Q_OS_MAC
    args.append(url);
    process->start("open", args);
#else
#ifdef Q_OS_WIN32
    std::string pfiles;
    (void)getEnvUtf8("ProgramFiles", pfiles);
    QString command =
            QString::fromStdString(pfiles) +
            QString("\\Internet Explorer\\IEXPLORE.EXE");

    args.append(url);
    process->start(command, args);
#else
    if (!qgetenv("KDE_FULL_SESSION").isEmpty()) {
        args.append("exec");
        args.append(url);
        process->start("kfmclient", args);
    } else if (!qgetenv("BROWSER").isEmpty()) {
        args.append(url);
        process->start(qgetenv("BROWSER"), args);
    } else {
        args.append(url);
        process->start("firefox", args);
    }
#endif
#endif
}

void
MainWindowBase::openLocalFolder(QString path)
{
    QDir d(path);
    if (d.exists()) {
        QStringList args;
        QString path = d.canonicalPath();
#if defined Q_OS_WIN32
        // Although the Win32 API is quite happy to have
        // forward slashes as directory separators, Windows
        // Explorer is not
        path = path.replace('/', '\\');
        args << path;
        QProcess::execute("c:/windows/explorer.exe", args);
#else
        args << path;
        QProcess process;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("LD_LIBRARY_PATH", "");
        process.setProcessEnvironment(env);
        process.start(
            #if defined Q_OS_MAC
                    "/usr/bin/open",
            #else
                    "/usr/bin/xdg-open",
            #endif
                    args);
        process.waitForFinished();
#endif
    }
}

