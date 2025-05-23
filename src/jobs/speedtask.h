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

class SpeedTask : public AbstractTask
{
public:
    SpeedTask(const ObjectId &owner, const QString &destination, int in, int out, std::unordered_map<QString, QVariant> filterParams, QObject *object);
    static void start(QObject* object, bool force = false);

private Q_SLOTS:
    void processLogInfo();

protected:
    void run() override;

private:
    double m_speed;
    int m_inPoint;
    int m_outPoint;
    QString m_assetId;
    QString m_filterName;
    std::unordered_map<QString, QVariant> m_filterParams;
    const QString m_destination;
    QStringList m_consumerArgs;
    QString m_errorMessage;
    QString m_logDetails;
    QProcess *m_jobProcess;
};
