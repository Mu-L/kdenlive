/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle <jb@kdenlive.org>

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "abstracttask.h"
#include <memory>
#include <unordered_map>
#include <mlt++/MltConsumer.h>

class QProcess;

class CutTask : public AbstractTask
{
public:
    CutTask(const ObjectId &owner, const QString &destination, const QStringList &encodingParams, int in ,int out, bool addToProject, QObject* object);
    static void start(const ObjectId &owner, int in , int out, QObject* object, bool force = false);

private Q_SLOTS:
    void processLogInfo();

protected:
    void run() override;

private:
    GenTime m_inPoint;
    GenTime m_outPoint;
    const QString m_destination;
    QStringList m_encodingParams;
    QString m_errorMessage;
    QString m_logDetails;
    QProcess *m_jobProcess;
    int m_jobDuration;
    bool m_addToProject;
};
