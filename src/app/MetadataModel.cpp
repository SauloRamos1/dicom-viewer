#include "MetadataModel.h"

#include <algorithm>

namespace {

QString tagText(const dv::TagEntry &entry)
{
    return QStringLiteral("(%1,%2)")
        .arg(entry.group, 4, 16, QLatin1Char('0'))
        .arg(entry.element, 4, 16, QLatin1Char('0'))
        .toUpper();
}

QString categoryKey(dv::TagGroup group)
{
    switch (group) {
    case dv::TagGroup::Patient: return QStringLiteral("patient");
    case dv::TagGroup::StudySeries: return QStringLiteral("study");
    case dv::TagGroup::Image: return QStringLiteral("image");
    case dv::TagGroup::Other: break;
    }
    return QStringLiteral("other");
}

} // namespace

MetadataModel::MetadataModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MetadataModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant MetadataModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Row &row = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
    case GroupRole: return row.entry.group;
    case TagRole: return tagText(row.entry);
    case NameRole: return QString::fromStdString(row.entry.name);
    case VrRole: return QString::fromStdString(row.entry.vr);
    case ValueRole: return QString::fromStdString(row.entry.value);
    case DepthRole: return row.entry.depth;
    case CategoryRole: return categoryKey(row.category);
    default: return {};
    }
}

QHash<int, QByteArray> MetadataModel::roleNames() const
{
    return {
        {GroupRole, "group"}, {TagRole, "tag"},     {NameRole, "name"},         {VrRole, "vr"},
        {ValueRole, "value"}, {DepthRole, "depth"}, {CategoryRole, "category"},
    };
}

void MetadataModel::setEntries(const std::vector<dv::TagEntry> &entries)
{
    std::vector<Row> rows;
    rows.reserve(entries.size());
    // Items of a sequence take the category of their top-level element.
    dv::TagGroup category = dv::TagGroup::Other;
    for (const dv::TagEntry &entry : entries) {
        if (entry.depth == 0)
            category = dv::groupOf(entry);
        rows.push_back({entry, category});
    }
    std::ranges::stable_sort(rows, {}, [](const Row &row) { return row.category; });

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void MetadataModel::clear()
{
    setEntries({});
}

MetadataFilterModel::MetadataFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

QString MetadataFilterModel::filterText() const
{
    return m_filterText;
}

void MetadataFilterModel::setFilterText(const QString &text)
{
    if (text == m_filterText)
        return;
    beginFilterChange();
    m_filterText = text;
    endFilterChange();
    emit filterTextChanged();
}

bool MetadataFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QString needle = m_filterText.trimmed();
    if (needle.isEmpty())
        return true;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    for (const int role : {MetadataModel::TagRole, MetadataModel::NameRole, MetadataModel::ValueRole}) {
        if (index.data(role).toString().contains(needle, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
