/*
    SPDX-FileCopyrightText: 2022 Julius Künzel <julius.kuenzel@kde.org>

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_editrenderpreset_ui.h"

class RenderPresetModel;
class Monitor;

class RenderPresetDialog : public QDialog, Ui::EditRenderPreset_UI
{
    Q_OBJECT
public:
    enum Mode {
        New = 0,
        Edit,
        SaveAs
    };
    explicit RenderPresetDialog(QWidget *parent, RenderPresetModel *preset = nullptr, Mode mode = Mode::New);
    /** @returns the name that was finally under which the preset has been saved */
    ~RenderPresetDialog() override;
    QString saveName();

protected:
    bool eventFilter(QObject *o, QEvent *e) override;

private:
    QString m_saveName;
    QStringList m_uiParams;
    Monitor *m_monitor;
    double m_fixedResRatio;
    bool m_manualPreset;

    void setPixelAspectRatio(int num, int den);
    void updateDisplayAspectRatio();

private Q_SLOTS:
    void slotUpdateParams();
};
