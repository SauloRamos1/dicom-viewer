#include "ErrorModel.h"

#include <QCoreApplication>
#include <QEvent>

#include "ErrorText.h"

ErrorModel::ErrorModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Titles and preview details are translated on demand: refresh them when
    // the language changes (installTranslator sends LanguageChange to qApp).
    if (QCoreApplication *app = QCoreApplication::instance())
        app->installEventFilter(this);
    setPreviewSamples();
}

int ErrorModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ErrorModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};
    const Entry &entry = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
    case SeverityRole: return ErrorText::severityKey(entry.severity);
    case TitleRole: return ErrorText::message(entry.code);
    case DetailRole: return entry.preview ? ErrorText::sampleDetail(entry.code) : entry.detail;
    case TagRole: return entry.tag;
    case TimestampRole: return entry.timestamp;
    case PreviewRole: return entry.preview;
    default: return {};
    }
}

QHash<int, QByteArray> ErrorModel::roleNames() const
{
    return {
        {SeverityRole, "severity"}, {TitleRole, "title"},     {DetailRole, "detail"},
        {TagRole, "tag"},           {TimestampRole, "timestamp"}, {PreviewRole, "preview"},
    };
}

void ErrorModel::setPreviewSamples()
{
    const auto sample = [](dv::ErrorCode code, dv::Severity severity, const char *tag) {
        return Entry{code, severity, {}, QString::fromLatin1(tag), {}, true};
    };
    setEntries({
        sample(dv::ErrorCode::NotDicom, dv::Severity::Error, ""),
        sample(dv::ErrorCode::UnsupportedTransferSyntax, dv::Severity::Error, "(0002,0010)"),
        sample(dv::ErrorCode::NoPixelData, dv::Severity::Warning, "(7FE0,0010)"),
    });
}

void ErrorModel::clear()
{
    setEntries({});
}

void ErrorModel::replaceWith(const std::vector<dv::DicomError> &errors)
{
    std::vector<Entry> entries;
    entries.reserve(errors.size());
    const QDateTime now = QDateTime::currentDateTime();
    for (const dv::DicomError &error : errors) {
        entries.push_back({error.code, error.severity, QString::fromStdString(error.detail),
                           error.tag ? QString::fromStdString(*error.tag) : QString(), now, false});
    }
    setEntries(std::move(entries));
}

bool ErrorModel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && watched == QCoreApplication::instance()
        && !m_entries.empty()) {
        emit dataChanged(index(0), index(rowCount() - 1), {TitleRole, DetailRole});
    }
    return QAbstractListModel::eventFilter(watched, event);
}

void ErrorModel::setEntries(std::vector<Entry> entries)
{
    const int before = rowCount();
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    if (rowCount() != before)
        emit countChanged();
}
