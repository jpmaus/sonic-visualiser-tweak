/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "PaneStack.h"

#include "Pane.h"
#include "widgets/PropertyStack.h"
#include "widgets/IconLoader.h"
#include "widgets/ClickableLabel.h"
#include "layer/Layer.h"
#include "ViewManager.h"
#include "AlignmentView.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPalette>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>

#include <iostream>

//#define DEBUG_PANE_STACK 1

PaneStack::PaneStack(QWidget *parent,
                     ViewManager *viewManager,
                     int options) :
    QFrame(parent),
    m_currentPane(nullptr),
    m_options(options),
    m_splitter(nullptr),
    m_autoResizeStack(nullptr),
    m_propertyStackStack(new QStackedWidget),
    m_viewManager(viewManager),
    m_propertyStackMinWidth(100),
    m_layoutStyle(PropertyStackPerPaneLayout)
{
    QHBoxLayout *layout = new QHBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);

    if (m_options & int(Option::NoUserResize)) {

        m_autoResizeStack = new QWidget;
        m_autoResizeLayout = new QVBoxLayout;
        m_autoResizeLayout->setMargin(0);
        m_autoResizeLayout->setSpacing(0);
        m_autoResizeStack->setLayout(m_autoResizeLayout);
        layout->addWidget(m_autoResizeStack);
        layout->setStretchFactor(m_autoResizeStack, 1);

    } else {
    
        m_splitter = new QSplitter;
        m_splitter->setOrientation(Qt::Vertical);
        m_splitter->setOpaqueResize(false);
        layout->addWidget(m_splitter);
        layout->setStretchFactor(m_splitter, 1);
    }

    if (m_options & int(Option::NoPropertyStacks)) {
        m_layoutStyle = HiddenPropertyStacksLayout;
    }
    
    m_propertyStackStack->hide();
    layout->addWidget(m_propertyStackStack);

    setLayout(layout);
}

Pane *
PaneStack::addPane()
{
    QFrame *frame = new QFrame;

    QGridLayout *layout = new QGridLayout;
    layout->setMargin(0);
    layout->setHorizontalSpacing(m_viewManager->scalePixelSize(2));

    AlignmentView *av = nullptr;

    if (m_options & int(Option::ShowAlignmentViews)) {
        layout->setVerticalSpacing(0);
        av = new AlignmentView(frame);
        av->setFixedHeight(ViewManager::scalePixelSize(20));
        av->setViewManager(m_viewManager);
        av->setVisible(false); // for now
        layout->addWidget(av, 0, 1);
    } else {
        layout->setVerticalSpacing(m_viewManager->scalePixelSize(2));
    }

    QPushButton *xButton = new QPushButton(frame);
    xButton->setIcon(IconLoader().load("cross"));
    xButton->setFixedSize(QSize(16, 16));
    xButton->setFlat(true);
    xButton->setVisible(!(m_options & int(Option::NoPaneAccessories)));
    if (m_panes.empty() && (m_options & int(Option::NoCloseOnFirstPane))) {
        xButton->setVisible(false);
    }
    layout->addWidget(xButton, 1, 0);
    connect(xButton, SIGNAL(clicked()), this, SLOT(paneDeleteButtonClicked()));

    ClickableLabel *currentIndicator = new ClickableLabel(frame);
    connect(currentIndicator, SIGNAL(clicked()), this, SLOT(indicatorClicked()));
    layout->addWidget(currentIndicator, 2, 0);
    layout->setRowStretch(2, 20);
    currentIndicator->setMinimumWidth(16);
    currentIndicator->setMinimumHeight(16);
    currentIndicator->setScaledContents(true);
    currentIndicator->setVisible(!(m_options & int(Option::NoPaneAccessories)));

    sv_frame_t initialCentreFrame = -1;
    if (!m_panes.empty()) {
        initialCentreFrame = m_panes[0].pane->getCentreFrame();
    }

    Pane *pane = new Pane(frame);
    if (initialCentreFrame >= 0) {
        pane->setViewManager(m_viewManager, initialCentreFrame);
    } else {
        pane->setViewManager(m_viewManager);
    }
    layout->addWidget(pane, 1, 1, 2, 1);
    layout->setColumnStretch(1, 20);

    QWidget *properties = nullptr;
    if (m_options & int(Option::NoPropertyStacks)) {
        properties = new QFrame();
    } else {
        properties = new PropertyStack(frame, pane);
        connect(properties, SIGNAL(propertyContainerSelected(View *, PropertyContainer *)),
                this, SLOT(propertyContainerSelected(View *, PropertyContainer *)));
        connect(properties, SIGNAL(viewSelected(View  *)),
                this, SLOT(viewSelected(View *)));
        connect(properties, SIGNAL(contextHelpChanged(const QString &)),
                this, SIGNAL(contextHelpChanged(const QString &)));
    }
    if (m_layoutStyle == PropertyStackPerPaneLayout) {
        layout->addWidget(properties, 1, 2, 2, 1);
    } else {
        properties->setParent(m_propertyStackStack);
        m_propertyStackStack->addWidget(properties);
    }
    layout->setColumnStretch(2, 0);

    PaneRec rec;
    rec.pane = pane;
    rec.propertyStack = properties;
    rec.xButton = xButton;
    rec.currentIndicator = currentIndicator;
    rec.frame = frame;
    rec.layout = layout;
    rec.alignmentView = av;
    m_panes.push_back(rec);

    frame->setLayout(layout);

    if (m_options & int(Option::NoUserResize)) {
        m_autoResizeLayout->addWidget(frame);
        frame->adjustSize();
    } else {
        m_splitter->addWidget(frame);
    }

    connect(pane, SIGNAL(propertyContainerAdded(PropertyContainer *)),
            this, SLOT(propertyContainerAdded(PropertyContainer *)));
    connect(pane, SIGNAL(propertyContainerRemoved(PropertyContainer *)),
            this, SLOT(propertyContainerRemoved(PropertyContainer *)));
    connect(pane, SIGNAL(paneInteractedWith()),
            this, SLOT(paneInteractedWith()));
    connect(pane, SIGNAL(rightButtonMenuRequested(QPoint)),
            this, SLOT(rightButtonMenuRequested(QPoint)));
    connect(pane, SIGNAL(dropAccepted(QStringList)),
            this, SLOT(paneDropAccepted(QStringList)));
    connect(pane, SIGNAL(dropAccepted(QString)),
            this, SLOT(paneDropAccepted(QString)));
    connect(pane, SIGNAL(doubleClickSelectInvoked(sv_frame_t)),
            this, SIGNAL(doubleClickSelectInvoked(sv_frame_t)));

    emit paneAdded(pane);
    emit paneAdded();

    if (!m_currentPane) {
        setCurrentPane(pane);
    }

    showOrHidePaneAccessories();
    relinkAlignmentViews();

    return pane;
}

void
PaneStack::relinkAlignmentViews()
{
    if (m_panes.empty()) return;
    auto av = m_panes[0].alignmentView;
    if (av) av->hide();
    for (int i = 1; in_range_for(m_panes, i); ++i) {
        av = m_panes[i].alignmentView;
        if (!av) continue;
        if (!(m_options & int(Option::ShowAlignmentViews))) {
            av->hide();
        } else {
            av->setViewAbove(m_panes[i-1].pane);
            av->setViewBelow(m_panes[i].pane);
            av->show();
        }
    }
}

void
PaneStack::unlinkAlignmentViews()
{
    for (int i = 0; in_range_for(m_panes, i); ++i) {
        auto av = m_panes[i].alignmentView;
        if (!av) continue;
        av->setViewAbove(nullptr);
        av->setViewBelow(nullptr);
    }
}

void
PaneStack::setPropertyStackMinWidth(int mw)
{
    for (std::vector<PaneRec>::iterator i = m_panes.begin();
         i != m_panes.end(); ++i) {
        i->propertyStack->setMinimumWidth(mw);
    }
    m_propertyStackMinWidth = mw;
}

void
PaneStack::setLayoutStyle(LayoutStyle style)
{
    if (m_options & int(Option::NoPropertyStacks)) {
        SVCERR << "NOTE: PaneStack::setLayoutStyle called on PaneStack with NoPropertyStacks option set - this does nothing, its style is always equivalent to HiddenPropertyStacksLayout" << endl;
        return;
    }
    
    if (style == m_layoutStyle) return;
    m_layoutStyle = style;

    std::vector<PaneRec>::iterator i;

    switch (style) {

    case HiddenPropertyStacksLayout:
    case SinglePropertyStackLayout:
        
        for (i = m_panes.begin(); i != m_panes.end(); ++i) {
            i->layout->removeWidget(i->propertyStack);
            i->propertyStack->setParent(m_propertyStackStack);
            m_propertyStackStack->addWidget(i->propertyStack);
        }
        m_propertyStackStack->setVisible(style != HiddenPropertyStacksLayout);
        break;

    case PropertyStackPerPaneLayout:

        for (i = m_panes.begin(); i != m_panes.end(); ++i) {
            m_propertyStackStack->removeWidget(i->propertyStack);
            i->propertyStack->setParent(i->frame);
            i->layout->addWidget(i->propertyStack, 1, 2, 2, 1);
            i->propertyStack->show();
        }
        m_propertyStackStack->hide();
        break;
    }
}

Pane *
PaneStack::getPane(int n)
{
    if (n < (int)m_panes.size()) {
        return m_panes[n].pane;
    } else {
        return nullptr;
    }
}

int
PaneStack::getPaneIndex(Pane *pane)
{
    for (int i = 0; i < getPaneCount(); ++i) {
        if (pane == getPane(i)) {
            return i;
        }
    }
    return -1;
}

Pane *
PaneStack::getHiddenPane(int n)
{
    return m_hiddenPanes[n].pane;
}

void
PaneStack::deletePane(Pane *pane)
{
#ifdef DEBUG_PANE_STACK
    SVCERR << "PaneStack::deletePane(" << pane << ")" << endl;
#endif

    std::vector<PaneRec>::iterator i;
    bool found = false;

    QWidget *stack = nullptr;

    for (i = m_panes.begin(); i != m_panes.end(); ++i) {
        if (i->pane == pane) {
            stack = i->propertyStack;
            m_panes.erase(i);
            found = true;
            break;
        }
    }

    if (!found) {

        for (i = m_hiddenPanes.begin(); i != m_hiddenPanes.end(); ++i) {
            if (i->pane == pane) {
                stack = i->propertyStack;
                m_hiddenPanes.erase(i);
                found = true;
                break;
            }
        }

        if (!found) {
            cerr << "WARNING: PaneStack::deletePane(" << pane << "): Pane not found in visible or hidden panes, not deleting" << endl;
            return;
        }
    }

    emit paneAboutToBeDeleted(pane);
    unlinkAlignmentViews();

#ifdef DEBUG_PANE_STACK
    SVCERR << "PaneStack::deletePane: about to delete parent " << pane->parent() << " of pane " << pane << endl;
#endif

    // The property stack associated with the parent was initially
    // created with the same parent as it, so it would be deleted when
    // we delete the pane's parent in a moment -- but it may have been
    // reparented depending on the layout. We'd better delete it
    // separately first. (This fixes a crash on opening a new layer
    // with a new unit type in it, when a long-defunct property box
    // could be signalled from the unit database to tell it that a new
    // unit had appeared.)
    delete stack;

    delete pane->parent();

    if (m_currentPane == pane) {
        if (m_panes.size() > 0) {
            setCurrentPane(m_panes[0].pane);
        } else {
            setCurrentPane(nullptr);
        }
    }

    showOrHidePaneAccessories();
    relinkAlignmentViews();

    emit paneDeleted();
}

void
PaneStack::showOrHidePaneAccessories()
{
#ifdef DEBUG_PANE_STACK
    SVCERR << "PaneStack::showOrHidePaneAccessories: count == " << getPaneCount() << endl;
#endif

    bool multi = (getPaneCount() > 1);
    for (std::vector<PaneRec>::iterator i = m_panes.begin();
         i != m_panes.end(); ++i) {
        bool visible = (multi && !(m_options & int(Option::NoPaneAccessories)));
        bool xvisible = visible;
        if (i == m_panes.begin()) {
            if (m_options & int(Option::NoCloseOnFirstPane)) {
                xvisible = false;
            }
        }
        i->xButton->setVisible(xvisible);
        i->currentIndicator->setVisible(visible);
    }
}

int
PaneStack::getPaneCount() const
{
    return int(m_panes.size());
}

int
PaneStack::getHiddenPaneCount() const
{
    return int(m_hiddenPanes.size());
}

void
PaneStack::hidePane(Pane *pane)
{
    std::vector<PaneRec>::iterator i = m_panes.begin();

    while (i != m_panes.end()) {
        if (i->pane == pane) {

            m_hiddenPanes.push_back(*i);
            m_panes.erase(i);

            QWidget *pw = dynamic_cast<QWidget *>(pane->parent());
            if (pw) pw->hide();

            if (m_currentPane == pane) {
                if (m_panes.size() > 0) {
                    setCurrentPane(m_panes[0].pane);
                } else {
                    setCurrentPane(nullptr);
                }
            }
            
            showOrHidePaneAccessories();
            emit paneHidden(pane);
            emit paneHidden();
            relinkAlignmentViews();
            return;
        }
        ++i;
    }

    SVCERR << "WARNING: PaneStack::hidePane(" << pane << "): Pane not found in visible panes" << endl;
}

void
PaneStack::showPane(Pane *pane)
{
    std::vector<PaneRec>::iterator i = m_hiddenPanes.begin();

    while (i != m_hiddenPanes.end()) {
        if (i->pane == pane) {
            m_panes.push_back(*i);
            m_hiddenPanes.erase(i);
            QWidget *pw = dynamic_cast<QWidget *>(pane->parent());
            if (pw) pw->show();

            //!!! update current pane

            showOrHidePaneAccessories();
            relinkAlignmentViews();

            return;
        }
        ++i;
    }

    SVCERR << "WARNING: PaneStack::showPane(" << pane << "): Pane not found in hidden panes" << endl;
}

void
PaneStack::setCurrentPane(Pane *pane) // may be null
{
    if (m_currentPane == pane) return;
    
    std::vector<PaneRec>::iterator i = m_panes.begin();

    // We used to do this by setting the foreground and background
    // role, but it seems the background role is ignored and the
    // background drawn transparent in Qt 4.1 -- I can't quite see why
    
    QPixmap selectedMap(1, 1);
    selectedMap.fill(QApplication::palette().color(QPalette::Foreground));
    
    QPixmap unselectedMap(1, 1);
    unselectedMap.fill(QApplication::palette().color(QPalette::Background));

    bool found = false;

    while (i != m_panes.end()) {
        if (i->pane == pane) {
            i->currentIndicator->setPixmap(selectedMap);
            if (m_layoutStyle != PropertyStackPerPaneLayout) {
                m_propertyStackStack->setCurrentWidget(i->propertyStack);
            }
            found = true;
        } else {
            i->currentIndicator->setPixmap(unselectedMap);
        }
        ++i;
    }

    if (found || pane == nullptr) {
        m_currentPane = pane;
        emit currentPaneChanged(m_currentPane);
    } else {
        SVCERR << "WARNING: PaneStack::setCurrentPane(" << pane << "): pane is not a visible pane in this stack" << endl;
    }
}

void
PaneStack::setCurrentLayer(Pane *pane, Layer *layer) // may be null
{
    setCurrentPane(pane);

    if (m_currentPane) {

        std::vector<PaneRec>::iterator i = m_panes.begin();

        while (i != m_panes.end()) {

            if (i->pane == pane) {
                PropertyStack *stack = dynamic_cast<PropertyStack *>
                    (i->propertyStack);
                if (stack) {
                    if (stack->containsContainer(layer)) {
                        stack->setCurrentIndex(stack->getContainerIndex(layer));
                        emit currentLayerChanged(pane, layer);
                    } else {
                        stack->setCurrentIndex
                            (stack->getContainerIndex
                             (pane->getPropertyContainer(0)));
                        emit currentLayerChanged(pane, nullptr);
                    }
                }
                break;
            }
            ++i;
        }
    }
}

Pane *
PaneStack::getCurrentPane() 
{
    return m_currentPane;
}

void
PaneStack::propertyContainerAdded(PropertyContainer *)
{
    sizePropertyStacks();
}

void
PaneStack::propertyContainerRemoved(PropertyContainer *)
{
    sizePropertyStacks();
}

void
PaneStack::propertyContainerSelected(View *client, PropertyContainer *pc)
{
    std::vector<PaneRec>::iterator i = m_panes.begin();

    while (i != m_panes.end()) {
        PropertyStack *stack = dynamic_cast<PropertyStack *>(i->propertyStack);
        if (stack &&
            stack->getClient() == client &&
            stack->containsContainer(pc)) {
            setCurrentPane(i->pane);
            break;
        }
        ++i;
    }

    Layer *layer = dynamic_cast<Layer *>(pc);
    if (layer) emit currentLayerChanged(m_currentPane, layer);
    else emit currentLayerChanged(m_currentPane, nullptr);
}

void
PaneStack::viewSelected(View *v)
{
    Pane *p = dynamic_cast<Pane *>(v);
    if (p) setCurrentPane(p);
}

void
PaneStack::paneInteractedWith()
{
    Pane *pane = dynamic_cast<Pane *>(sender());
    if (!pane) return;
    setCurrentPane(pane);
}

void
PaneStack::rightButtonMenuRequested(QPoint position)
{
    Pane *pane = dynamic_cast<Pane *>(sender());
    if (!pane) return;
    emit rightButtonMenuRequested(pane, position);
}

void
PaneStack::sizePropertyStacks()
{
    int maxMinWidth = 0;

    if (m_propertyStackMinWidth > 0) maxMinWidth = m_propertyStackMinWidth;

    for (int i = 0; i < (int)m_panes.size(); ++i) {
        if (!m_panes[i].propertyStack) continue;
#ifdef DEBUG_PANE_STACK
        SVDEBUG << "PaneStack::sizePropertyStacks: " << i << ": min " 
                  << m_panes[i].propertyStack->minimumSizeHint().width() << ", hint "
                  << m_panes[i].propertyStack->sizeHint().width() << ", current "
                  << m_panes[i].propertyStack->width() << endl;
#endif

        if (m_panes[i].propertyStack->sizeHint().width() > maxMinWidth) {
            maxMinWidth = m_panes[i].propertyStack->sizeHint().width();
        }
    }

#ifdef DEBUG_PANE_STACK
    SVDEBUG << "PaneStack::sizePropertyStacks: max min width " << maxMinWidth << endl;
#endif

    int setWidth = maxMinWidth;

    m_propertyStackStack->setMaximumWidth(setWidth + 10);

    for (int i = 0; i < (int)m_panes.size(); ++i) {
        if (!m_panes[i].propertyStack) continue;
        m_panes[i].propertyStack->setMinimumWidth(setWidth);
    }

    emit propertyStacksResized(setWidth);
    emit propertyStacksResized();
}
    
void
PaneStack::paneDropAccepted(QStringList uriList)
{
    Pane *pane = dynamic_cast<Pane *>(sender());
    emit dropAccepted(pane, uriList);
}
    
void
PaneStack::paneDropAccepted(QString text)
{
    Pane *pane = dynamic_cast<Pane *>(sender());
    emit dropAccepted(pane, text);
}

void
PaneStack::paneDeleteButtonClicked()
{
    QObject *s = sender();
    for (int i = 0; i < (int)m_panes.size(); ++i) {
        if (m_panes[i].xButton == s) {
            emit paneDeleteButtonClicked(m_panes[i].pane);
        }
    }
}

void
PaneStack::indicatorClicked()
{
    QObject *s = sender();

    for (int i = 0; i < (int)m_panes.size(); ++i) {
        if (m_panes[i].currentIndicator == s) {
            setCurrentPane(m_panes[i].pane);
            return;
        }
    }
}

void
PaneStack::sizePanesEqually()
{
    if (m_options & int(Option::NoUserResize)) {
        return;
    }
    
    QList<int> sizes = m_splitter->sizes();
    if (sizes.empty()) return;

    int count = sizes.size();

    int fixed = 0, variable = 0, total = 0;
    int varicount = 0;

    for (int i = 0; i < count; ++i) {
        total += sizes[i];
    }

    variable = total;

    for (int i = 0; i < count; ++i) {
        int minh = m_panes[i].pane->minimumSize().height();
        if (minh == m_panes[i].pane->maximumSize().height()) {
            fixed += minh;
            variable -= minh;
        } else {
            varicount++;
        }
    }

    if (total == 0) return;

    sizes.clear();

    int each = (varicount > 0 ? (variable / varicount) : 0);
    int remaining = total;

    for (int i = 0; i < count; ++i) {
        if (i == count - 1) {
            sizes.push_back(remaining);
        } else {
            int minh = m_panes[i].pane->minimumSize().height();
            if (minh == m_panes[i].pane->maximumSize().height()) {
                sizes.push_back(minh);
                remaining -= minh;
            } else {
                sizes.push_back(each);
                remaining -= each;
            }
        }
    }

/*
    cerr << "sizes: ";
    for (int i = 0; i < sizes.size(); ++i) {
        cerr << sizes[i] << " ";
    }
    cerr << endl;
*/

    m_splitter->setSizes(sizes);
}

