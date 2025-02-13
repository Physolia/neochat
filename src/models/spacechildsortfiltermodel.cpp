// SPDX-FileCopyrightText: 2023 James Graham <james.h.graham@protonmail.com>
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL

#include "spacechildsortfiltermodel.h"

#include "spacechildrenmodel.h"

SpaceChildSortFilterModel::SpaceChildSortFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setRecursiveFilteringEnabled(true);
    sort(0);
}

void SpaceChildSortFilterModel::setFilterText(const QString &filterText)
{
    m_filterText = filterText;
    Q_EMIT filterTextChanged();
    invalidateFilter();
}

QString SpaceChildSortFilterModel::filterText() const
{
    return m_filterText;
}

bool SpaceChildSortFilterModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const
{
    if (!source_left.data(SpaceChildrenModel::IsSpaceRole).toBool() && source_right.data(SpaceChildrenModel::IsSpaceRole).toBool()) {
        return false;
    }
    return true;
}

bool SpaceChildSortFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (auto sourceModel = static_cast<SpaceChildrenModel *>(this->sourceModel())) {
        bool isReplaced = sourceModel->isRoomReplaced(index.data(SpaceChildrenModel::RoomIDRole).toString());
        bool acceptRoom = index.data(SpaceChildrenModel::DisplayNameRole).toString().contains(m_filterText, Qt::CaseInsensitive);
        return !isReplaced && acceptRoom;
    }
    return true;
}

#include "moc_spacechildsortfiltermodel.cpp"
