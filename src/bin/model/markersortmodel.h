/*
SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

/**
 * @class MarkerSortModel
 * @brief Acts as an filtering proxy for the Bin Views, used when triggering the lineedit filter.
 */
class MarkerSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit MarkerSortModel(QObject *parent = nullptr);
    std::vector<int> getIgnoredSnapPoints() const;

public Q_SLOTS:
    /** @brief Set search tag that will filter the view */
    void slotSetFilters(const QList<int> filter);
    /** @brief Reset search filters */
    void slotClearSearchFilters();
    void slotSetFilterString(const QString &filter);
    void slotSetSortColumn(int column);
    void slotSetSortOrder(bool descending);

protected:
    /** @brief Decide which items should be displayed depending on the search string  */
    // cppcheck-suppress unusedFunction
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsRowItself(int source_row, const QModelIndex &source_parent) const;
    bool filterString(int sourceRow, const QModelIndex &sourceParent) const;
    /** @brief Reimplemented to allow sorting by category, time or comment  */
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    QList<int> m_filterList;
    mutable QList<int> m_ignoredPositions;
    QString m_searchString;
    int m_sortColumn;
    int m_sortOrder;
};
