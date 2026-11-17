#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "Metadata.h"

// Flat list of DICOM elements, ordered by UI category (Patient, Study/Series,
// Image, Other) and by tag inside a category, so a ListView can use `category`
// as its section. Sequence items stay right after their parent element.
class MetadataModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DicomController")

public:
    enum Role {
        GroupRole = Qt::UserRole + 1, // tag group number, e.g. 0x0010
        TagRole,                      // "(0010,0010)"
        NameRole,
        VrRole,
        ValueRole,
        DepthRole,                    // nesting depth inside sequences
        CategoryRole,                 // "patient" | "study" | "image" | "other"
    };

    explicit MetadataModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const std::vector<dv::TagEntry> &entries);
    Q_INVOKABLE void clear();

private:
    struct Row {
        dv::TagEntry entry;
        dv::TagGroup category{};
    };

    std::vector<Row> m_rows;
};

// Case-insensitive "contains" filter over the tag text, name and value.
class MetadataFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DicomController")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    explicit MetadataFilterModel(QObject *parent = nullptr);

    QString filterText() const;
    void setFilterText(const QString &text);

signals:
    void filterTextChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
};
