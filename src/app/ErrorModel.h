#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <vector>

#include "DicomError.h"

// Entries of the Errors panel. At startup (and after New) it holds a few sample
// errors flagged `preview`; a failed load replaces them with the real errors;
// a successful load clears the list.
class ErrorModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DicomController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        SeverityRole = Qt::UserRole + 1, // "error" | "warning" | "info"
        TitleRole,                       // translated message for the error code
        DetailRole,
        TagRole,
        TimestampRole,
        PreviewRole,
    };

    explicit ErrorModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return rowCount(); }

    // Replaces the content with sample errors flagged as preview.
    Q_INVOKABLE void setPreviewSamples();
    Q_INVOKABLE void clear();
    void replaceWith(const std::vector<dv::DicomError> &errors);

signals:
    void countChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Entry {
        dv::ErrorCode code{};
        dv::Severity severity{dv::Severity::Error};
        QString detail;
        QString tag;
        QDateTime timestamp;
        bool preview{false};
    };

    void setEntries(std::vector<Entry> entries);

    std::vector<Entry> m_entries;
};
