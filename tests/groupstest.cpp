/*
    SPDX-FileCopyrightText: 2018-2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
    SPDX-FileCopyrightText: 2017-2019 Nicolas Carion <french.ebook.lover@gmail.com>
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
#include "catch.hpp"
#include "test_utils.hpp"
// test specific headers
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic push
#endif
#include "bin/model/markerlistmodel.hpp"
#include "bin/projectclip.h"
#include "bin/projectfolder.h"
#include "bin/projectitemmodel.h"
#include "core.h"
#include "doc/docundostack.hpp"
#include "doc/kdenlivedoc.h"
#include "fakeit.hpp"
#include "project/projectmanager.h"
#include "timeline2/model/clipmodel.hpp"
#include "timeline2/model/groupsmodel.hpp"
#include "timeline2/model/timelineitemmodel.hpp"
#include "timeline2/model/timelinemodel.hpp"
#include "timeline2/model/trackmodel.hpp"
#include <iostream>
#include <mlt++/MltProducer.h>
#include <mlt++/MltProfile.h>
#include <unordered_set>

TEST_CASE("Functional test of the group hierarchy", "[GroupsModel]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);
    // Here we do some trickery to enable testing.
    // We mock the project class so that the undoStack function returns our undoStack

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);
    KdenliveTests::resetNextId();

    GroupsModel *groups = KdenliveTests::groupsModel(timeline);

    for (int i = 0; i < 10; i++) {
        groups->createGroupItem(i);
    }

    SECTION("Test Basic Creation")
    {
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups->getRootId(i) == i);
            REQUIRE(groups->isLeaf(i));
            REQUIRE(groups->getLeaves(i).size() == 1);
            REQUIRE(groups->getSubtree(i).size() == 1);
        }
    }

    groups->setGroup(0, 1);
    groups->setGroup(1, 2);
    groups->setGroup(3, 2);
    groups->setGroup(9, 3);
    groups->setGroup(6, 3);
    groups->setGroup(4, 3);
    groups->setGroup(7, 3);
    groups->setGroup(8, 5);

    SECTION("Test leaf nodes")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups->isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving")
    {
        REQUIRE(groups->getLeaves(2) == std::unordered_set<int>({0, 4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups->getLeaves(5) == std::unordered_set<int>({8}));
    }

    SECTION("Test subtree retrieving")
    {
        REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 6, 7, 9}));
        REQUIRE(groups->getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups->getSubtree(5) == std::unordered_set<int>({5, 8}));
    }

    SECTION("Test root retrieving")
    {
        std::set<int> first_tree = {0, 1, 2, 3, 4, 6, 7, 9};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups->getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5, 8};
        for (int n : second_tree) {
            REQUIRE(groups->getRootId(n) == 5);
        }
    }

    groups->setGroup(3, 8);
    SECTION("Test leaf nodes 2")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5, 8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups->isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 2")
    {
        REQUIRE(groups->getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups->getLeaves(2) == std::unordered_set<int>({0}));
        REQUIRE(groups->getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(5) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(8) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 2")
    {
        REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2}));
        REQUIRE(groups->getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups->getSubtree(5) == std::unordered_set<int>({5, 8, 3, 4, 6, 7, 9}));
    }

    SECTION("Test root retrieving 2")
    {
        std::set<int> first_tree = {0, 1, 2};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups->getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {5, 8, 3, 4, 6, 7, 9};
        for (int n : second_tree) {
            REQUIRE(groups->getRootId(n) == 5);
        }
    }

    groups->setGroup(5, 2);
    SECTION("Test leaf nodes 3")
    {
        std::unordered_set<int> nodes = {1, 2, 3, 5, 8};
        for (int i = 0; i < 10; i++) {
            REQUIRE(groups->isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 3")
    {
        REQUIRE(groups->getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups->getLeaves(2) == std::unordered_set<int>({0, 4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(5) == std::unordered_set<int>({4, 6, 7, 9}));
        REQUIRE(groups->getLeaves(8) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 3")
    {
        REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
        REQUIRE(groups->getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups->getSubtree(5) == std::unordered_set<int>({5, 8, 3, 4, 6, 7, 9}));
    }

    SECTION("Test root retrieving 3")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            REQUIRE(groups->getRootId(i) == 2);
        }
    }

    KdenliveTests::destructGroupItem(timeline, 8, false, undo, redo);
    SECTION("Test leaf nodes 4")
    {
        std::unordered_set<int> nodes = {1, 2, 3};
        for (int i = 0; i < 10; i++) {
            if (i == 8) continue;
            REQUIRE(groups->isLeaf(i) != (nodes.count(i) > 0));
            if (nodes.count(i) == 0) {
                REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
                REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
            }
        }
    }

    SECTION("Test leaves retrieving 4")
    {
        REQUIRE(groups->getLeaves(1) == std::unordered_set<int>({0}));
        REQUIRE(groups->getLeaves(2) == std::unordered_set<int>({0, 5}));
        REQUIRE(groups->getLeaves(3) == std::unordered_set<int>({4, 6, 7, 9}));
    }

    SECTION("Test subtree retrieving 4")
    {
        REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2, 5}));
        REQUIRE(groups->getSubtree(3) == std::unordered_set<int>({3, 4, 6, 7, 9}));
        REQUIRE(groups->getSubtree(5) == std::unordered_set<int>({5}));
    }

    SECTION("Test root retrieving 4")
    {
        std::set<int> first_tree = {0, 1, 2, 5};
        for (int n : first_tree) {
            CAPTURE(n);
            REQUIRE(groups->getRootId(n) == 2);
        }
        std::unordered_set<int> second_tree = {3, 4, 6, 7, 9};
        for (int n : second_tree) {
            CAPTURE(n);
            REQUIRE(groups->getRootId(n) == 3);
        }
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("Interface test of the group hierarchy", "[GroupsModel]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    GroupsModel *groups = KdenliveTests::groupsModel(timeline);

    for (int i = 0; i < 10; i++) {
        groups->createGroupItem(i);
        // the following call shouldn't do anything, but we test that behaviour too.
        groups->ungroupItem(i, undo, redo);
        REQUIRE(groups->getRootId(i) == i);
        REQUIRE(groups->isLeaf(i));
        REQUIRE(groups->getLeaves(i).size() == 1);
        REQUIRE(groups->getSubtree(i).size() == 1);
        REQUIRE(groups->checkConsistency(false));
    }
    KdenliveTests::resetNextId(10);

    auto g1 = std::unordered_set<int>({4, 6, 7, 9});
    int gid1 = groups->groupItems(g1, undo, redo);

    SECTION("One single group")
    {
        for (int i = 0; i < 10; i++) {
            if (g1.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid1);
            } else {
                REQUIRE(groups->getRootId(i) == i);
            }
            REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups->getLeaves(gid1) == g1);
        auto g1b = g1;
        g1b.insert(gid1);
        REQUIRE(groups->getSubtree(gid1) == g1b);
        REQUIRE(groups->checkConsistency(false));
    }
    SECTION("Twice the same group")
    {
        int old_gid1 = gid1;
        gid1 = groups->groupItems(g1, undo, redo); // recreate the same group (will create a parent with the old group as only element)
        for (int i = 0; i < 10; i++) {
            if (g1.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid1);
            } else {
                REQUIRE(groups->getRootId(i) == i);
            }
            REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups->getLeaves(gid1) == g1);
        REQUIRE(groups->getLeaves(old_gid1) == g1);
        auto g1b = g1;
        g1b.insert(old_gid1);
        REQUIRE(groups->getSubtree(old_gid1) == g1b);
        g1b.insert(gid1);
        REQUIRE(groups->getSubtree(gid1) == g1b);
        REQUIRE(groups->checkConsistency(false));
    }

    auto g2 = std::unordered_set<int>({3, 5, 7});
    int gid2 = groups->groupItems(g2, undo, redo);
    auto all_g2 = g2;
    all_g2.insert(4);
    all_g2.insert(6);
    all_g2.insert(9);

    SECTION("Heterogeneous group")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g2.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid2);
            } else {
                REQUIRE(groups->getRootId(i) == i);
            }
            REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups->getLeaves(gid1) == g1);
        REQUIRE(groups->getLeaves(gid2) == all_g2);
        REQUIRE(groups->checkConsistency(false));
    }

    auto g3 = std::unordered_set<int>({0, 1});
    int gid3 = groups->groupItems(g3, undo, redo);

    auto g4 = std::unordered_set<int>({0, 4});
    int gid4 = groups->groupItems(g4, undo, redo);
    auto all_g4 = all_g2;
    for (int i : g3)
        all_g4.insert(i);

    SECTION("Group of group")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g4.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid4);
            } else {
                REQUIRE(groups->getRootId(i) == i);
            }
            REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups->getLeaves(gid1) == g1);
        REQUIRE(groups->getLeaves(gid2) == all_g2);
        REQUIRE(groups->getLeaves(gid3) == g3);
        REQUIRE(groups->getLeaves(gid4) == all_g4);
        REQUIRE(groups->checkConsistency(false));
    }

    // the following should delete g4
    groups->ungroupItem(3, undo, redo);

    SECTION("Ungroup")
    {
        for (int i = 0; i < 10; i++) {
            CAPTURE(i);
            if (all_g2.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid2);
            } else if (g3.count(i) > 0) {
                REQUIRE(groups->getRootId(i) == gid3);
            } else {
                REQUIRE(groups->getRootId(i) == i);
            }
            REQUIRE(groups->getSubtree(i) == std::unordered_set<int>({i}));
            REQUIRE(groups->getLeaves(i) == std::unordered_set<int>({i}));
        }
        REQUIRE(groups->getLeaves(gid1) == g1);
        REQUIRE(groups->checkConsistency(false));
        REQUIRE(groups->getLeaves(gid2) == all_g2);
        REQUIRE(groups->getLeaves(gid3) == g3);
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("Orphan groups deletion", "[GroupsModel]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);

    KdenliveTests::resetNextId();
    GroupsModel *groups = KdenliveTests::groupsModel(timeline);

    for (int i = 0; i < 4; i++) {
        groups->createGroupItem(i);
    }
    KdenliveTests::resetNextId(5);
    auto g1 = std::unordered_set<int>({0, 1});
    int gid1 = groups->groupItems(g1, undo, redo);

    auto g2 = std::unordered_set<int>({2, 3});
    int gid2 = groups->groupItems(g2, undo, redo);
    Q_UNUSED(gid2);

    auto g3 = std::unordered_set<int>({0, 3});
    int gid3 = groups->groupItems(g3, undo, redo);

    REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({0, 1, 2, 3}));
    REQUIRE(groups->checkConsistency(false));

    KdenliveTests::destructGroupItem(timeline, 0, true, undo, redo);

    REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({1, 2, 3}));
    REQUIRE(groups->checkConsistency(false));

    SECTION("Normal deletion")
    {
        KdenliveTests::destructGroupItem(timeline, 1, false, undo, redo);

        REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({gid1, 2, 3}));
        REQUIRE(groups->checkConsistency(false));
        KdenliveTests::destructGroupItem(timeline, gid1, true, undo, redo);

        REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({2, 3}));
        REQUIRE(groups->checkConsistency(false));
    }

    SECTION("Cascade deletion")
    {
        KdenliveTests::destructGroupItem(timeline, 1, true, undo, redo);

        REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({2, 3}));
        REQUIRE(groups->checkConsistency(false));

        KdenliveTests::destructGroupItem(timeline, 2, true, undo, redo);

        REQUIRE(groups->getLeaves(gid3) == std::unordered_set<int>({3}));
        REQUIRE(groups->checkConsistency(false));

        REQUIRE(KdenliveTests::downLinks(timeline, gid3) > 0);
        KdenliveTests::destructGroupItem(timeline, 3, true, undo, redo);
        REQUIRE(KdenliveTests::downLinks(timeline, gid3) == 0);
        REQUIRE(KdenliveTests::downLinksSize(timeline) == 0);
        REQUIRE(groups->checkConsistency(false));
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("Integration with timeline", "[GroupsModel]")
{
    qDebug() << "STARTING PASS";
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    const QUuid uuid1 = document.uuid();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(uuid1);
    pCore->projectManager()->testSetActiveTimeline(timeline);

    // Create a new sequence clip
    std::pair<int, int> tracks = {2, 2};
    const QString seqId = ClipCreator::createPlaylistClip(QStringLiteral("Seq 2"), tracks, QStringLiteral("-1"), binModel);
    REQUIRE(seqId != QLatin1String("-1"));

    // Now use the new timeline sequence
    QUuid uuid2;
    QMap<QUuid, QString> allSequences = binModel->getAllSequenceClips();
    QMapIterator<QUuid, QString> i(allSequences);
    while (i.hasNext()) {
        // Find clips with the tag
        i.next();
        if (i.value() == seqId) {
            uuid2 = i.key();
        }
    }
    auto timeline2 = document.getTimeline(uuid2);

    /*TimelineItemModel tim(mockedDoc.uuid(), pCore->getProjectProfile(), undoStack);
    Mock<TimelineItemModel> timMock(tim);
    auto timeline = std::shared_ptr<TimelineItemModel>(&timMock.get(), [](...) {});
    KdenliveTests::finishTimelineConstruct(timeline);

    QUuid uuid2 = QUuid::createUuid();
    TimelineItemModel tim2(uuid2, pCore->getProjectProfile(), undoStack);
    Mock<TimelineItemModel> timMock2(tim2);
    auto timeline2 = std::shared_ptr<TimelineItemModel>(&timMock2.get(), [](...) {});
    KdenliveTests::finishTimelineConstruct(timeline2);
    mockedDoc.addTimeline(uuid2, timeline2);

    RESET(timMock2);*/

    QString binId = KdenliveTests::createProducer(pCore->getProjectProfile(), "red", binModel);
    QString binId2 = KdenliveTests::createProducerWithSound(pCore->getProjectProfile(), binModel);

    int length = binModel->getClipByBinID(binId)->frameDuration();

    std::vector<int> clips;
    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0) {
            clips.push_back(ClipModel::construct(timeline, binId, -1, PlaylistState::VideoOnly));
        } else {
            clips.push_back(ClipModel::construct(timeline, binId, -1, PlaylistState::AudioOnly));
            KdenliveTests::forceClipAudio(timeline, clips.back());
        }
    }
    std::vector<int> clips2;
    for (int i = 0; i < 4; i++) {
        if (i % 2 == 0) {
            clips2.push_back(ClipModel::construct(timeline2, binId, -1, PlaylistState::VideoOnly));
        } else {
            clips2.push_back(ClipModel::construct(timeline2, binId, -1, PlaylistState::AudioOnly));
            KdenliveTests::forceClipAudio(timeline2, clips2.back());
        }
    }
    int tid1 = TrackModel::construct(timeline);
    int tid2 = TrackModel::construct(timeline);
    Q_UNUSED(tid2);
    int tid3 = TrackModel::construct(timeline, -1, -1, QStringLiteral("audio"), true);

    int tid1_2 = TrackModel::construct(timeline2);
    int tid2_2 = TrackModel::construct(timeline2);
    Q_UNUSED(tid2_2);
    int tid3_2 = TrackModel::construct(timeline2, -1, -1, QStringLiteral("audio2"), true);

    int init_index = undoStack->index();
    pCore->projectManager()->setActiveTimeline(uuid2);
    REQUIRE(timeline2->checkConsistency());
    pCore->projectManager()->setActiveTimeline(uuid1);
    SECTION("Basic Creation and export/import from json")
    {
        auto check_roots = [&](int r1, int r2, int r3, int r4) {
            REQUIRE(KdenliveTests::groupsModel(timeline)->getRootId(clips[0]) == r1);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getRootId(clips[1]) == r2);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getRootId(clips[2]) == r3);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getRootId(clips[3]) == r4);
        };

        // the following function is a recursive function to check the correctness of a json import
        // Basically, it takes as input a groupId in the imported (target) group hierarchy, and outputs the corresponding groupId from the original one. If no
        // match is found, it returns -1
        std::function<int(int)> rec_check;
        rec_check = [&](int gid) {
            // we first check if the gid is a leaf
            if (KdenliveTests::groupsModel(timeline2)->isLeaf(gid)) {
                // then it must be a clip/composition
                int found = -1;
                for (int i = 0; i < 4; i++) {
                    if (clips2[i] == gid) {
                        found = i;
                        break;
                    }
                }
                if (found != -1) {
                    return clips[found];
                } else {
                    qDebug() << "ERROR: did not find correspondence for group" << gid;
                }
            } else {
                // we find correspondences of all the children
                auto children = KdenliveTests::groupsModel(timeline2)->getDirectChildren(gid);
                std::unordered_set<int> corresp;
                for (int c : children) {
                    corresp.insert(rec_check(c));
                }
                if (corresp.count(-1) > 0) {
                    return -1; // something went wrong
                }
                std::unordered_set<int> parents;
                for (int c : corresp) {
                    // we find the parents of the corresponding groups in the original hierarchy
                    parents.insert(KdenliveTests::groupUpLink(timeline)[c]);
                }
                // if the matching is correct, we should have found only one parent
                if (parents.size() != 1) {
                    return -1; // something went wrong
                }
                return *parents.begin();
            }
            return -1;
        };
        auto checkJsonParsing = [&]() {
            // we first destroy all groups in target timeline
            Fun undo = []() { return true; };
            Fun redo = []() { return true; };
            for (int i = 0; i < 4; i++) {
                while (KdenliveTests::groupsModel(timeline2)->getRootId(clips2[i]) != clips2[i]) {
                    KdenliveTests::groupsModel(timeline2)->ungroupItem(clips2[i], undo, redo);
                }
            }
            // we do the export then import
            REQUIRE(KdenliveTests::groupsModel(timeline2)->fromJson(KdenliveTests::groupsModel(timeline)->toJson()));
            std::unordered_map<int, int> roots;
            for (int i = 0; i < 4; i++) {
                int r = KdenliveTests::groupsModel(timeline2)->getRootId(clips2[i]);
                if (roots.count(r) == 0) {
                    roots[r] = rec_check(r);
                    REQUIRE(roots[r] != -1);
                }
            }
            for (int i = 0; i < 4; i++) {
                int r = KdenliveTests::groupsModel(timeline)->getRootId(clips[i]);
                int r2 = KdenliveTests::groupsModel(timeline2)->getRootId(clips2[i]);
                REQUIRE(roots[r2] == r);
            }
            pCore->projectManager()->setActiveTimeline(uuid1);
            REQUIRE(timeline->checkConsistency());
            pCore->projectManager()->setActiveTimeline(uuid2);
            REQUIRE(timeline2->checkConsistency());
        };
        pCore->projectManager()->setActiveTimeline(uuid1);
        REQUIRE(timeline->checkConsistency());
        pCore->projectManager()->setActiveTimeline(uuid2);
        REQUIRE(timeline2->checkConsistency());
        auto g1 = std::unordered_set<int>({clips[0], clips[1]});
        int gid1, gid2, gid3;
        // this fails because clips are not inserted
        pCore->projectManager()->setActiveTimeline(uuid1);
        REQUIRE(timeline->requestClipsGroup(g1) == -1);
        REQUIRE(timeline->checkConsistency());
        pCore->projectManager()->setActiveTimeline(uuid2);
        REQUIRE(timeline2->checkConsistency());

        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline->requestClipMove(clips[i], (i % 2 == 0) ? tid1 : tid3, i * length));
        }
        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline2->requestClipMove(clips2[i], (i % 2 == 0) ? tid1_2 : tid3_2, i * length));
        }
        pCore->projectManager()->setActiveTimeline(uuid1);
        REQUIRE(timeline->checkConsistency());
        pCore->projectManager()->setActiveTimeline(uuid2);
        REQUIRE(timeline2->checkConsistency());
        init_index = undoStack->index();
        REQUIRE(timeline->requestClipsGroup(g1, true, GroupType::Normal) > 0);
        auto state1 = [&]() {
            gid1 = KdenliveTests::groupsModel(timeline)->getRootId(clips[0]);
            check_roots(gid1, gid1, clips[2], clips[3]);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid1) == GroupType::Normal);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 1);
            pCore->projectManager()->setActiveTimeline(uuid1);
            REQUIRE(timeline->checkConsistency());
        };
        INFO("Test 1");
        state1();
        checkJsonParsing();
        state1();

        auto g2 = std::unordered_set<int>({clips[2], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g2, true, GroupType::AVSplit) > 0);
        auto state2 = [&]() {
            gid2 = KdenliveTests::groupsModel(timeline)->getRootId(clips[2]);
            check_roots(gid1, gid1, gid2, gid2);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid1) == GroupType::Normal);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid2) == GroupType::AVSplit);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            REQUIRE(undoStack->index() == init_index + 2);
            pCore->projectManager()->setActiveTimeline(uuid1);
            REQUIRE(timeline->checkConsistency());
        };
        INFO("Test 2");
        checkJsonParsing();
        state2();

        auto g3 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g3, true, GroupType::Normal) > 0);
        auto state3 = [&]() {
            REQUIRE(undoStack->index() == init_index + 3);
            gid3 = KdenliveTests::groupsModel(timeline)->getRootId(clips[0]);
            check_roots(gid3, gid3, gid3, gid3);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid1) == GroupType::Normal);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid2) == GroupType::AVSplit);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid3) == GroupType::Normal);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid3) ==
                    std::unordered_set<int>({gid1, clips[0], clips[1], gid3, gid2, clips[2], clips[3]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid3) == std::unordered_set<int>({clips[2], clips[3], clips[0], clips[1]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid2) == std::unordered_set<int>({gid2, clips[2], clips[3]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid2) == std::unordered_set<int>({clips[2], clips[3]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getSubtree(gid1) == std::unordered_set<int>({gid1, clips[0], clips[1]}));
            REQUIRE(KdenliveTests::groupsModel(timeline)->getLeaves(gid1) == std::unordered_set<int>({clips[0], clips[1]}));
            pCore->projectManager()->setActiveTimeline(uuid1);
            REQUIRE(timeline->checkConsistency());
        };

        INFO("Test 3");
        checkJsonParsing();
        state3();

        undoStack->undo();
        INFO("Test 4");
        checkJsonParsing();
        state2();
        undoStack->redo();
        INFO("Test 5");
        checkJsonParsing();
        state3();
        undoStack->undo();
        INFO("Test 6");
        checkJsonParsing();
        state2();
        undoStack->undo();
        INFO("Test 8");
        checkJsonParsing();
        state1();
        undoStack->undo();
        INFO("Test 9");
        checkJsonParsing();
        check_roots(clips[0], clips[1], clips[2], clips[3]);
        undoStack->redo();
        INFO("Test 10");
        checkJsonParsing();
        state1();
        undoStack->redo();
        INFO("Test 11");
        checkJsonParsing();
        state2();

        REQUIRE(timeline->requestClipsGroup(g3) > 0);
        checkJsonParsing();
        state3();

        undoStack->undo();
        checkJsonParsing();
        state2();

        undoStack->undo();
        checkJsonParsing();
        state1();
        undoStack->undo();
        checkJsonParsing();
        check_roots(clips[0], clips[1], clips[2], clips[3]);
    }

    SECTION("Group deletion undo")
    {
        CAPTURE(clips[0]);
        CAPTURE(clips[1]);
        CAPTURE(clips[2]);
        CAPTURE(clips[3]);
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid3, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid3, 50 + 3 * length));

        auto state0 = [&]() {
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid1)->checkConsistency());
            REQUIRE(timeline->getTrackClipsCount(tid1) == 2);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 2);
            REQUIRE(timeline->getClipsCount() == 4);
            REQUIRE(timeline->getClipTrackId(clips[0]) == tid1);
            REQUIRE(timeline->getClipTrackId(clips[2]) == tid1);
            REQUIRE(timeline->getClipTrackId(clips[1]) == tid3);
            REQUIRE(timeline->getClipTrackId(clips[3]) == tid3);
            REQUIRE(timeline->getClipPosition(clips[0]) == 10);
            REQUIRE(timeline->getClipPosition(clips[1]) == 10 + length);
            REQUIRE(timeline->getClipPosition(clips[2]) == 15 + 2 * length);
            REQUIRE(timeline->getClipPosition(clips[3]) == 50 + 3 * length);
            REQUIRE(timeline->checkConsistency());
        };

        auto state = [&](int gid1, int gid2, int gid3) {
            state0();
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid1) == GroupType::AVSplit);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid2) == GroupType::AVSplit);
            REQUIRE(KdenliveTests::groupsModel(timeline)->getType(gid3) == GroupType::Normal);
            REQUIRE(timeline->checkConsistency());
        };
        state0();
        auto g1 = std::unordered_set<int>({clips[0], clips[1]});
        int gid1, gid2, gid3;
        gid1 = timeline->requestClipsGroup(g1, true, GroupType::AVSplit);
        REQUIRE(gid1 > 0);
        auto g2 = std::unordered_set<int>({clips[2], clips[3]});
        gid2 = timeline->requestClipsGroup(g2, true, GroupType::AVSplit);
        REQUIRE(gid2 > 0);
        auto g3 = std::unordered_set<int>({clips[0], clips[3]});
        gid3 = timeline->requestClipsGroup(g3, true, GroupType::Normal);
        REQUIRE(gid3 > 0);
        state(gid1, gid2, gid3);

        std::vector<int> all_clips{clips[0], clips[2], clips[1], clips[3]};
        for (int i = 0; i < 4; i++) {
            REQUIRE(timeline->requestItemDeletion(all_clips[i]));
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid1)->checkConsistency());
            REQUIRE(KdenliveTests::getTrackById_const(timeline, tid3)->checkConsistency());
            REQUIRE(timeline->checkConsistency());

            undoStack->undo();
            state(gid1, gid2, gid3);
            undoStack->redo();
            REQUIRE(timeline->getTrackClipsCount(tid1) == 0);
            REQUIRE(timeline->getTrackClipsCount(tid3) == 0);
            REQUIRE(timeline->getClipsCount() == 0);
            REQUIRE(timeline->checkConsistency());
            undoStack->undo();
            state(gid1, gid2, gid3);
        }
        // we undo the three grouping operations
        undoStack->undo();
        state0();
        undoStack->undo();
        state0();
        undoStack->undo();
        state0();
    }

    SECTION("Group creation and query from timeline")
    {
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid1, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid1, 50 + 3 * length));
        auto state1 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->checkConsistency());
        };
        state1();

        auto g1 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g1) > 0);
        auto state2 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->checkConsistency());
        };
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        undoStack->undo();
        state1();

        undoStack->redo();
        state2();

        auto g2 = std::unordered_set<int>({clips[2], clips[1]});
        REQUIRE(timeline->requestClipsGroup(g2) > 0);
        auto state3 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state3();

        undoStack->undo();
        state2();

        undoStack->redo();
        state3();

        auto g3 = std::unordered_set<int>({clips[0], clips[1]});
        REQUIRE(timeline->requestClipsGroup(g3) > 0);
        auto all_g = std::unordered_set<int>({clips[0], clips[1], clips[2], clips[3]});
        auto state4 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[3]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[2]) == all_g);
            REQUIRE(timeline->getGroupElements(clips[1]) == all_g);
            REQUIRE(timeline->checkConsistency());
        };
        state4();

        undoStack->undo();
        state3();

        undoStack->redo();
        state4();

        REQUIRE(timeline->requestClipUngroup(clips[0]));
        state3();

        undoStack->undo();
        state4();

        REQUIRE(timeline->requestClipUngroup(clips[1]));
        state3();

        undoStack->undo();
        state4();

        undoStack->redo();
        state3();

        REQUIRE(timeline->requestClipUngroup(clips[0]));
        REQUIRE(timeline->getGroupElements(clips[2]) == g2);
        REQUIRE(timeline->getGroupElements(clips[1]) == g2);
        REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
        REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));

        REQUIRE(timeline->requestClipUngroup(clips[1]));
        state1();
    }
    SECTION("Ungroup multiple groups")
    {
        REQUIRE(timeline->requestClipMove(clips[0], tid1, 10));
        REQUIRE(timeline->requestClipMove(clips[1], tid1, 10 + length));
        REQUIRE(timeline->requestClipMove(clips[2], tid1, 15 + 2 * length));
        REQUIRE(timeline->requestClipMove(clips[3], tid1, 50 + 3 * length));
        auto state1 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[2]) == std::unordered_set<int>({clips[2]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == std::unordered_set<int>({clips[1]}));
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->checkConsistency());
        };
        state1();

        auto g1 = std::unordered_set<int>({clips[0], clips[3]});
        REQUIRE(timeline->requestClipsGroup(g1) > 0);
        auto g2 = std::unordered_set<int>({clips[1], clips[2]});
        REQUIRE(timeline->requestClipsGroup(g2) > 0);
        auto state2 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[0]) == g1);
            REQUIRE(timeline->getGroupElements(clips[3]) == g1);
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state2();

        // ungroup clips from same group
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[3]}));
        auto state3 = [&]() {
            REQUIRE(timeline->getGroupElements(clips[3]) == std::unordered_set<int>({clips[3]}));
            REQUIRE(timeline->getGroupElements(clips[0]) == std::unordered_set<int>({clips[0]}));
            REQUIRE(timeline->getGroupElements(clips[1]) == g2);
            REQUIRE(timeline->getGroupElements(clips[2]) == g2);
            REQUIRE(timeline->checkConsistency());
        };
        state3();

        undoStack->undo();
        state2();

        // ungroup clips from different groups
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[1]}));
        state1();

        undoStack->undo();
        state2();

        // ungroup all clips
        REQUIRE(timeline->requestClipsUngroup({clips[0], clips[1], clips[2], clips[3]}));
        state1();

        undoStack->undo();
        state2();
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}

TEST_CASE("Complex Functions", "[GroupsModel]")
{
    auto binModel = pCore->projectItemModel();
    binModel->clean();
    std::shared_ptr<DocUndoStack> undoStack = std::make_shared<DocUndoStack>(nullptr);

    // Create document
    KdenliveDoc document(undoStack);
    pCore->projectManager()->testSetDocument(&document);
    std::function<bool(void)> undo = []() { return true; };
    std::function<bool(void)> redo = []() { return true; };
    QDateTime documentDate = QDateTime::currentDateTime();
    KdenliveTests::updateTimeline(false, QString(), QString(), documentDate, 0);
    auto timeline = document.getTimeline(document.uuid());
    pCore->projectManager()->testSetActiveTimeline(timeline);
    KdenliveTests::resetNextId();

    GroupsModel *groups = KdenliveTests::groupsModel(timeline);

    SECTION("MergeSingleGroups")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        for (int i = 0; i < 6; i++) {
            groups->createGroupItem(i);
        }
        groups->setGroup(0, 3);
        groups->setGroup(2, 4);
        groups->setGroup(3, 1);
        groups->setGroup(4, 1);
        groups->setGroup(5, 0);

        auto test_tree = [&]() {
            REQUIRE(groups->getSubtree(1) == std::unordered_set<int>({0, 1, 2, 3, 4, 5}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({5}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({3, 4}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({0}));
            REQUIRE(groups->getDirectChildren(4) == std::unordered_set<int>({2}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups->mergeSingleGroups(1, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups->getSubtree(1) == std::unordered_set<int>({1, 2, 5}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({2, 5}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("MergeSingleGroups2")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        for (int i = 0; i < 3; i++) {
            groups->createGroupItem(i);
        }
        groups->setGroup(1, 0);
        groups->setGroup(2, 1);

        auto test_tree = [&]() {
            REQUIRE(groups->getSubtree(0) == std::unordered_set<int>({0, 1, 2}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({1}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({2}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups->mergeSingleGroups(0, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({2}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getRootId(2) == 2);
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("MergeSingleGroups3")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        for (int i = 0; i < 6; i++) {
            groups->createGroupItem(i);
        }
        groups->setGroup(0, 2);
        groups->setGroup(1, 0);
        groups->setGroup(3, 1);
        groups->setGroup(4, 1);
        groups->setGroup(5, 4);

        auto test_tree = [&]() {
            for (int i = 0; i < 6; i++) {
                REQUIRE(groups->getRootId(i) == 2);
            }
            REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({1}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({4, 3}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({0}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(4) == std::unordered_set<int>({5}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency(false));
        };
        test_tree();

        REQUIRE(groups->mergeSingleGroups(2, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups->getRootId(1) == 1);
            REQUIRE(groups->getRootId(3) == 1);
            REQUIRE(groups->getRootId(5) == 1);
            REQUIRE(groups->getSubtree(1) == std::unordered_set<int>({1, 3, 5}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({3, 5}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }
    SECTION("Split leaf")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };
        auto criterion2 = [](int a) { return a % 2 != 0; };

        // We create a leaf
        groups->createGroupItem(1);
        auto test_leaf = [&]() {
            REQUIRE(groups->getRootId(1) == 1);
            REQUIRE(groups->isLeaf(1));
            REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 1);
            REQUIRE(groups->checkConsistency());
        };
        test_leaf();

        REQUIRE(groups->split(1, criterion, undo, redo));
        test_leaf();
        undo();
        test_leaf();
        redo();
        REQUIRE(groups->split(1, criterion2, undo, redo));
        test_leaf();
        undo();
        test_leaf();
        redo();
    }
    SECTION("Simple split Tree")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };
        KdenliveTests::resetNextId();

        // We create a very simple tree
        for (int i = 0; i < 3; i++) {
            groups->createGroupItem(i);
        }
        groups->setGroup(1, 0);
        groups->setGroup(2, 0);
        KdenliveTests::resetNextId(3);
        auto test_tree = [&]() {
            REQUIRE(groups->getRootId(0) == 0);
            REQUIRE(groups->getRootId(1) == 0);
            REQUIRE(groups->getRootId(2) == 0);
            REQUIRE(groups->getSubtree(0) == std::unordered_set<int>({0, 1, 2}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({1, 2}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree();

        REQUIRE(groups->split(0, criterion, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups->getRootId(1) == 1);
            REQUIRE(groups->getRootId(2) == 2);
            REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({2}));
            REQUIRE(groups->getSubtree(1) == std::unordered_set<int>({1}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }

    SECTION("complex split Tree")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 != 0; };

        for (int i = 0; i < 9; i++) {
            groups->createGroupItem(i);
        }
        KdenliveTests::resetNextId(9);
        groups->setGroup(0, 3);
        groups->setGroup(1, 0);
        groups->setGroup(3, 2);
        groups->setGroup(4, 3);
        groups->setGroup(5, 8);
        groups->setGroup(6, 0);
        groups->setGroup(7, 8);
        groups->setGroup(8, 2);

        auto test_tree = [&]() {
            for (int i = 0; i < 9; i++) {
                REQUIRE(groups->getRootId(i) == 2);
            }
            REQUIRE(groups->getSubtree(2) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6, 7, 8}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({1, 6}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({3, 8}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({0, 4}));
            REQUIRE(groups->getDirectChildren(4) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(6) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(7) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(8) == std::unordered_set<int>({5, 7}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree();

        REQUIRE(groups->split(2, criterion, undo, redo));
        auto test_tree2 = [&]() {
            REQUIRE(groups->getRootId(6) == 3);
            REQUIRE(groups->getRootId(3) == 3);
            REQUIRE(groups->getRootId(4) == 3);
            REQUIRE(groups->getSubtree(3) == std::unordered_set<int>({3, 4, 6}));
            REQUIRE(groups->getDirectChildren(6) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(4) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({6, 4}));
            // new tree
            int newRoot = groups->getRootId(1);
            REQUIRE(groups->getRootId(1) == newRoot);
            REQUIRE(groups->getRootId(5) == newRoot);
            REQUIRE(groups->getRootId(7) == newRoot);
            int other = -1;
            REQUIRE(groups->getDirectChildren(newRoot).size() == 2);
            for (int c : groups->getDirectChildren(newRoot))
                if (c != 1) other = c;
            REQUIRE(other != -1);
            REQUIRE(groups->getSubtree(newRoot) == std::unordered_set<int>({1, 5, 7, newRoot, other}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(7) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(newRoot) == std::unordered_set<int>({1, other}));
            REQUIRE(groups->getDirectChildren(other) == std::unordered_set<int>({5, 7}));
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
    }
    SECTION("Splitting preserves group type")
    {
        Fun undo = []() { return true; };
        Fun redo = []() { return true; };
        REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 0);

        // This is a dummy split criterion
        auto criterion = [](int a) { return a % 2 == 0; };

        // We create a very simple tree
        for (int i = 0; i <= 6; i++) {
            groups->createGroupItem(i);
        }
        KdenliveTests::resetNextId(7);
        groups->setGroup(0, 4);
        groups->setGroup(2, 4);
        groups->setGroup(1, 5);
        groups->setGroup(3, 5);

        groups->setGroup(4, 6);
        groups->setGroup(5, 6);

        KdenliveTests::setGroupType(timeline, 4, GroupType::AVSplit);
        KdenliveTests::setGroupType(timeline, 5, GroupType::AVSplit);
        KdenliveTests::setGroupType(timeline, 6, GroupType::Normal);

        auto test_tree = [&]() {
            REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 7);
            for (int i = 0; i <= 6; i++) {
                REQUIRE(groups->getRootId(i) == 6);
            }
            REQUIRE(groups->getSubtree(6) == std::unordered_set<int>({0, 1, 2, 3, 4, 5, 6}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(4) == std::unordered_set<int>({0, 2}));
            REQUIRE(groups->getDirectChildren(5) == std::unordered_set<int>({1, 3}));
            REQUIRE(groups->getDirectChildren(6) == std::unordered_set<int>({4, 5}));
            REQUIRE(groups->getType(4) == GroupType::AVSplit);
            REQUIRE(groups->getType(5) == GroupType::AVSplit);
            REQUIRE(groups->getType(6) == GroupType::Normal);
            REQUIRE(groups->checkConsistency());
        };
        test_tree();
        qDebug() << " done testing";

        REQUIRE(groups->split(6, criterion, undo, redo));
        qDebug() << " done splitting";
        auto test_tree2 = [&]() {
            // REQUIRE(KdenliveTests::groupUpLink(timeline).size() == 6);
            int r1 = groups->getRootId(0);
            int r2 = groups->getRootId(1);
            bool ok = r1 == 4 || r2 == 5;
            REQUIRE(ok);
            REQUIRE(groups->getRootId(2) == r1);
            REQUIRE(groups->getRootId(3) == r2);
            REQUIRE(groups->getSubtree(r1) == std::unordered_set<int>({r1, 0, 2}));
            REQUIRE(groups->getSubtree(r2) == std::unordered_set<int>({r2, 1, 3}));
            REQUIRE(groups->getDirectChildren(0) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(1) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(2) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(3) == std::unordered_set<int>({}));
            REQUIRE(groups->getDirectChildren(r1) == std::unordered_set<int>({0, 2}));
            REQUIRE(groups->getDirectChildren(r2) == std::unordered_set<int>({1, 3}));
            REQUIRE(groups->getType(r1) == GroupType::AVSplit);
            REQUIRE(groups->getType(r2) == GroupType::AVSplit);
            REQUIRE(groups->checkConsistency());
        };
        test_tree2();

        undo();
        test_tree();

        redo();
        test_tree2();
        undo();
        test_tree();
        redo();
        test_tree2();
    }
    pCore->projectManager()->closeCurrentDocument(false, false);
}
