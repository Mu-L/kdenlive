/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "timelinetabs.hpp"
#include "assets/model/assetparametermodel.hpp"
#include "audiomixer/mixermanager.hpp"
#include "bin/projectclip.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "mainwindow.h"
#include "monitor/monitor.h"
#include "monitor/monitormanager.h"
#include "monitor/monitorproxy.h"
#include "project/projectmanager.h"
#include "timelinecontroller.h"
#include "timelinewidget.h"

#include <KMessageBox>
#include <KXMLGUIFactory>
#include <QInputDialog>
#include <QMenu>
#include <QPainter>
#include <QQmlContext>
#include <QQmlEngine>

TimelineContainer::TimelineContainer(QWidget *parent)
    : QWidget(parent)
{
}

QSize TimelineContainer::sizeHint() const
{
    return QSize(800, pCore->window()->height() / 2);
}

TimelineTabs::TimelineTabs(QWidget *parent)
    : QTabWidget(parent)
    , m_activeTimeline(nullptr)
{
    setTabBarAutoHide(true);
    setTabsClosable(false);
    setDocumentMode(true);
    setMovable(true);
    QToolButton *pb = new QToolButton(this);
    pb->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    pb->setAutoRaise(true);
    pb->setToolTip(i18n("Add Timeline Sequence"));
    pb->setWhatsThis(
        i18n("Add Timeline Sequence. This will create a new timeline for editing. Each timeline corresponds to a Sequence Clip in the Project Bin"));
    connect(pb, &QToolButton::clicked, [=]() { pCore->triggerAction(QStringLiteral("add_playlist_clip")); });
    setCornerWidget(pb);
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    connect(this, &TimelineTabs::tabCloseRequested, this, &TimelineTabs::closeTimelineByIndex);
    connect(tabBar(), &QTabBar::tabBarDoubleClicked, this, &TimelineTabs::onTabBarDoubleClicked);
    connect(pCore.get(), &Core::saveTimelinePreview, this, &TimelineTabs::saveTimelinePreview);
}

TimelineTabs::~TimelineTabs()
{
    // clear source
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        timeline->setSource(QUrl());
    };
}

void TimelineTabs::updateWindowTitle()
{
    // Show current timeline name in Window title if we have multiple sequences but only one opened
    if (m_activeTimeline == nullptr || pCore->currentDoc()->closing) {
        return;
    }
    if (count() == 1 && pCore->projectItemModel()->sequenceCount() > 1) {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description(KLocalizedString::removeAcceleratorMarker(tabText(0))));
        m_activeTimeline->model()->updateVisibleSequenceName(tabText(0));
    } else {
        pCore->window()->setWindowTitle(pCore->currentDoc()->description());
        m_activeTimeline->model()->updateVisibleSequenceName(QString());
    }
}

bool TimelineTabs::raiseTimeline(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            if (i != currentIndex()) {
                setCurrentIndex(i);
            }
            return true;
        }
    }
    return false;
}

int TimelineTabs::getTimelineIndex(const QUuid &uuid)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            return i;
        }
    }
    return -1;
}

void TimelineTabs::setModified(const QUuid &uuid, bool modified)
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (timeline->getUuid() == uuid) {
            setTabIcon(i, modified ? QIcon::fromTheme(QStringLiteral("document-save")) : QIcon());
            break;
        }
    }
}

TimelineWidget *TimelineTabs::addTimeline(const QUuid uuid, int ix, const QString &tabName, std::shared_ptr<TimelineItemModel> timelineModel,
                                          MonitorProxy *proxy, bool openInMonitor)
{
    QMutexLocker lk(&m_lock);
    if (count() == 1 && m_activeTimeline) {
        m_activeTimeline->model()->updateVisibleSequenceName(QString());
    }
    disconnect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    TimelineWidget *newTimeline = new TimelineWidget(uuid, this);
    newTimeline->setTimelineMenu(m_timelineClipMenu, m_timelineCompositionMenu, m_timelineMenu, m_guideMenu, m_timelineRulerMenu, m_editGuideAction,
                                 m_headerMenu, m_thumbsMenu, m_timelineSubtitleClipMenu);
    newTimeline->setModel(timelineModel, proxy);
    int newIndex = 0;
    if (ix == -1 || ix >= count()) {
        newIndex = addTab(newTimeline, tabName);
    } else {
        newIndex = insertTab(ix, newTimeline, tabName);
    }
    setCurrentIndex(newIndex);
    setTabsClosable(count() > 1);
    lk.unlock();
    doConnectCurrent(newIndex, openInMonitor);
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    return newTimeline;
}

void TimelineTabs::connectCurrent(int ix)
{
    doConnectCurrent(ix, true);
}

void TimelineTabs::doConnectCurrent(int ix, bool openInMonitor)
{
    QMutexLocker lk(&m_lock);
    QUuid previousTab = QUuid();
    if (m_activeTimeline && m_activeTimeline->model()) {
        previousTab = m_activeTimeline->getUuid();
        pCore->window()->disableMulticam();
        if (openInMonitor && !pCore->currentDoc()->loading) {
            if (pCore->isMediaCapturing()) {
                pCore->switchCapture();
            } else if (pCore->isMediaMonitoring()) {
                pCore->setAudioMonitoring(false);
            }
            int pos = pCore->getMonitorPosition();
            m_activeTimeline->model()->updateDuration();
            std::pair<int, int> durations = m_activeTimeline->model()->durations();
            qDebug() << "::::: GOT SEQUENCES DURATIONS: " << durations;
            if (durations.second > 0) {
                int previousIndex = getTimelineIndex(previousTab);
                const QString seqName = KLocalizedString::removeAcceleratorMarker(tabText(previousIndex));
                // A sequence was made shorter, this will resize its instance in other sequences. Warn user
                if (KMessageBox::questionTwoActions(this,
                                                    i18n("The timeline sequence <b>%1</b> was shortened.<br/>Resize all instances in other timelines ?<br>Not "
                                                         "resizing will temporarily keep the current duration in all other sequences.",
                                                         seqName),
                                                    {}, KGuiItem(i18nc("@action:button", "Resize")),
                                                    KGuiItem(i18nc("@action:button", "Don't Resize"))) == KMessageBox::PrimaryAction) {
                    durations.second = 0;
                }
            }
            pCore->bin()->updateSequenceClip(previousTab, durations, pos);
        }
        pCore->window()->disconnectTimeline(m_activeTimeline);
        disconnectTimeline(m_activeTimeline);
    } else {
        qDebug() << "==== NO PREVIOUS TIMELINE";
    }
    if (ix < 0 || ix >= count() || pCore->currentDoc()->closing) {
        m_activeTimeline = nullptr;
        qDebug() << "==== ABORTING NO TIMELINE AVAILABLE";
        return;
    }
    m_activeTimeline = static_cast<TimelineWidget *>(widget(ix));
    if (m_activeTimeline->model() == nullptr || m_activeTimeline->model()->m_closing) {
        // Closing app
        qDebug() << "++++++++++++\n\nCLOSING APP\n\n+++++++++++++";
        return;
    }
    if (openInMonitor) {
        pCore->window()->connectTimeline();
        connectTimeline(m_activeTimeline);
        updateWindowTitle();
        if (!m_activeTimeline->model()->isLoading) {
            pCore->bin()->sequenceActivated();
        }
        // Wait a few milliseconds to allow for the qml view to display
        pCore->monitorManager()->projectMonitor()->refreshMonitorTimer.start();
    } else {
        connectTimeline(m_activeTimeline);
    }
}

void TimelineTabs::renameTab(const QUuid &uuid, const QString &name)
{
    qDebug() << "==== READY TO RENAME!!!!!!!!!";
    for (int i = 0; i < count(); i++) {
        if (static_cast<TimelineWidget *>(widget(i))->getUuid() == uuid) {
            tabBar()->setTabText(i, name);
            pCore->projectManager()->setTimelineProperty(uuid, QStringLiteral("kdenlive:clipname"), name);
            updateWindowTitle();
            break;
        }
    }
}

void TimelineTabs::closeTimelineByIndex(int ix)
{
    TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
    if (timeline == m_activeTimeline) {
        Q_EMIT timeline->model()->requestClearAssetView(-1);
        pCore->clearTimeRemap();
        pCore->mixer()->unsetModel();
        pCore->window()->disableMulticam();
        m_activeTimeline->model()->updateDuration();
        // timeline->controller()->saveSequenceProperties();
    }
    const QString seqName = KLocalizedString::removeAcceleratorMarker(tabText(ix));
    std::shared_ptr<TimelineItemModel> model = timeline->model();
    const QUuid uuid = timeline->getUuid();
    const QString id = pCore->projectItemModel()->getSequenceId(uuid);
    Fun undo = [uuid, id, model]() { return pCore->projectManager()->openTimeline(id, -1, uuid, -1, false, model); };
    Fun redo = [this, ix, uuid]() {
        pCore->projectManager()->closeTimeline(uuid, false, false);
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(ix));
        removeTab(ix);
        timeline->blockSignals(true);
        if (timeline == m_activeTimeline) {
            pCore->window()->disconnectTimeline(timeline);
            disconnectTimeline(timeline);
        }
        if (m_activeTimeline == timeline) {
            m_activeTimeline = nullptr;
        }
        delete timeline;
        updateWindowTitle();
        return true;
    };
    redo();
    pCore->pushUndo(undo, redo, i18n("Close %1", seqName));
}

TimelineWidget *TimelineTabs::getCurrentTimeline() const
{
    return m_activeTimeline;
}

void TimelineTabs::closeTimelineTab(const QUuid uuid)
{
    QMutexLocker lk(&m_lock);
    int currentCount = count();
    disconnect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
    bool closing = pCore->currentDoc()->closing;
    for (int i = 0; i < currentCount; i++) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(i));
        if (uuid == timeline->getUuid()) {
            removeTab(i);
            timeline->blockSignals(true);
            if (timeline == m_activeTimeline) {
                Q_EMIT showSubtitle(-1);
                pCore->window()->disconnectTimeline(timeline, closing);
                disconnectTimeline(timeline);
                m_activeTimeline = nullptr;
            }
            delete timeline;
            // pCore->projectManager()->closeTimeline(uuid);
            setTabsClosable(count() > 1);
            if (currentCount == 2) {
                updateWindowTitle();
            }
            break;
        }
    }
    lk.unlock();
    if (closing) {
        // We are closing document, no need to reconnect
        return;
    }
    connect(this, &TimelineTabs::currentChanged, this, &TimelineTabs::connectCurrent);
}

void TimelineTabs::connectTimeline(TimelineWidget *timeline)
{
    int position = pCore->currentDoc()->getSequenceProperty(timeline->getUuid(), QStringLiteral("position"), QString::number(0)).toInt();
    pCore->monitorManager()->projectMonitor()->getControllerProxy()->setCursorPosition(position);
    connect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor, Qt::DirectConnection);
    connect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    connect(this, &TimelineTabs::fitZoom, timeline, &TimelineWidget::slotFitZoom);
    connect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    connect(timeline->controller(), &TimelineController::showMixModel, this, &TimelineTabs::showMixModel);
    connect(timeline->controller(), &TimelineController::updateZoom, this, [&](double value) { Q_EMIT updateZoom(getCurrentTimeline()->zoomForScale(value)); });
    connect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
    connect(timeline->controller(), &TimelineController::showSubtitle, this, &TimelineTabs::showSubtitle);
    connect(timeline->controller(), &TimelineController::updateAssetPosition, this, &TimelineTabs::updateAssetPosition);
    connect(timeline->controller(), &TimelineController::centerView, timeline, &TimelineWidget::slotCenterView);

    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, m_activeTimeline, &TimelineWidget::zoneUpdated);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, m_activeTimeline, &TimelineWidget::zoneUpdatedWithUndo);
    connect(m_activeTimeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    connect(pCore->monitorManager()->projectMonitor(), &Monitor::addTimelineEffect, m_activeTimeline->controller(),
            &TimelineController::addEffectToCurrentClip);
    timeline->rootContext()->setContextProperty("proxy", pCore->monitorManager()->projectMonitor()->getControllerProxy());
    QQmlEngine::setObjectOwnership(pCore->monitorManager()->projectMonitor()->getControllerProxy(), QQmlEngine::CppOwnership);
    Q_EMIT timeline->controller()->selectionChanged();
    timeline->setEnabled(true);
    timeline->setMouseTracking(true);
}

void TimelineTabs::disconnectTimeline(TimelineWidget *timeline)
{
    timeline->setEnabled(false);
    timeline->setMouseTracking(false);
    timeline->rootContext()->setContextProperty("proxy", QVariant());
    disconnect(timeline, &TimelineWidget::focusProjectMonitor, pCore->monitorManager(), &MonitorManager::focusProjectMonitor);
    disconnect(this, &TimelineTabs::changeZoom, timeline, &TimelineWidget::slotChangeZoom);
    disconnect(this, &TimelineTabs::fitZoom, timeline, &TimelineWidget::slotFitZoom);
    disconnect(timeline->controller(), &TimelineController::showTransitionModel, this, &TimelineTabs::showTransitionModel);
    disconnect(timeline->controller(), &TimelineController::showMixModel, this, &TimelineTabs::showMixModel);
    disconnect(timeline->controller(), &TimelineController::showItemEffectStack, this, &TimelineTabs::showItemEffectStack);
    disconnect(timeline->controller(), &TimelineController::showSubtitle, this, &TimelineTabs::showSubtitle);
    disconnect(timeline->controller(), &TimelineController::updateAssetPosition, this, &TimelineTabs::updateAssetPosition);

    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdated, timeline, &TimelineWidget::zoneUpdated);
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::zoneUpdatedWithUndo, timeline, &TimelineWidget::zoneUpdatedWithUndo);
    disconnect(timeline, &TimelineWidget::zoneMoved, pCore->monitorManager()->projectMonitor(), &Monitor::slotLoadClipZone);
    disconnect(pCore->monitorManager()->projectMonitor(), &Monitor::addTimelineEffect, timeline->controller(), &TimelineController::addEffectToCurrentClip);
}

void TimelineTabs::buildClipMenu()
{
    // Timeline clip menu
    delete m_timelineClipMenu;
    m_timelineClipMenu = new QMenu(this);
    KActionCollection *coll = pCore->window()->actionCollection();
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("edit_copy")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("paste_effects")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("delete_effects")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("group_clip")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("ungroup_clip")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("edit_item_duration")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("clip_split")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("clip_switch")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("delete_timeline_clip")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("extract_clip")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("save_to_bin")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("send_sequence")));

    QMenu *markerMenu = static_cast<QMenu *>(pCore->window()->factory()->container(QStringLiteral("marker_menu"), pCore->window()));
    m_timelineClipMenu->addMenu(markerMenu);
    QMenu *alignMenu = new QMenu(i18n("Align to Reference"), this);
    m_timelineClipMenu->addMenu(alignMenu);
    alignMenu->addAction(coll->action(QStringLiteral("set_audio_align_ref")));
    alignMenu->addAction(coll->action(QStringLiteral("align_audio")));
    alignMenu->addAction(coll->action(QStringLiteral("set_timecode_ref")));
    alignMenu->addAction(coll->action(QStringLiteral("align_timecode")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("edit_item_speed")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("edit_item_remap")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("clip_in_project_tree")));
    m_timelineClipMenu->addAction(coll->action(QStringLiteral("cut_timeline_clip")));
}

void TimelineTabs::setTimelineMenu(QMenu *compositionMenu, QMenu *timelineMenu, QMenu *guideMenu, QMenu *timelineRulerMenu, QAction *editGuideAction,
                                   QMenu *headerMenu, QMenu *thumbsMenu, QMenu *subtitleClipMenu)
{
    buildClipMenu();
    m_timelineCompositionMenu = compositionMenu;
    m_timelineMenu = timelineMenu;
    m_timelineRulerMenu = timelineRulerMenu;
    m_guideMenu = guideMenu;
    m_headerMenu = headerMenu;
    m_thumbsMenu = thumbsMenu;
    m_headerMenu->addMenu(m_thumbsMenu);
    m_timelineSubtitleClipMenu = subtitleClipMenu;
    m_editGuideAction = editGuideAction;
}

const QStringList TimelineTabs::openedSequences()
{
    QStringList result;
    for (int i = 0; i < count(); i++) {
        result << static_cast<TimelineWidget *>(widget(i))->getUuid().toString();
    }
    return result;
}

TimelineWidget *TimelineTabs::getTimeline(const QUuid uuid) const
{
    for (int i = 0; i < count(); i++) {
        TimelineWidget *tl = static_cast<TimelineWidget *>(widget(i));
        if (tl->getUuid() == uuid) {
            return tl;
        }
    }
    return nullptr;
}

void TimelineTabs::slotNextSequence()
{
    int max = count();
    int focus = currentIndex() + 1;
    if (focus >= max) {
        focus = 0;
    }
    setCurrentIndex(focus);
}

void TimelineTabs::slotPreviousSequence()
{
    int focus = currentIndex() - 1;
    if (focus < 0) {
        focus = count() - 1;
    }
    setCurrentIndex(focus);
}

void TimelineTabs::onTabBarDoubleClicked(int index)
{
    if (index == -1) {
        // No action when double clicking in empty space
        return;
    }
    const QString currentTabName = KLocalizedString::removeAcceleratorMarker(tabText(index));
    bool ok = false;
    const QString newName = QInputDialog::getText(this, i18n("Rename Sequence"), i18n("Rename Sequence"), QLineEdit::Normal, currentTabName, &ok);
    if (ok && !newName.isEmpty()) {
        TimelineWidget *timeline = static_cast<TimelineWidget *>(widget(index));
        if (timeline) {
            const QString id = pCore->projectItemModel()->getSequenceId(timeline->getUuid());
            std::shared_ptr<ProjectClip> clip = pCore->projectItemModel()->getClipByBinID(id);
            if (clip) {
                clip->rename(newName, 0);
            }
        }
    }
}

void TimelineTabs::saveTimelinePreview(const QString &path)
{
    int imageWidth = 600;
    int imageHeight = size().height() * imageWidth / size().width();
    QPixmap bitmap(size());
    QPainter painter(&bitmap);
    render(&painter);
    painter.end();
    QPixmap scaled = bitmap.scaled(imageWidth, imageHeight);
    scaled.save(path);
}
