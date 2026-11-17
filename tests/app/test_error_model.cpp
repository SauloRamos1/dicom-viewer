#include <catch2/catch_test_macros.hpp>

#include <QCoreApplication>
#include <QEvent>

#include "ErrorModel.h"
#include "ErrorText.h"

namespace {
QVariant value(const ErrorModel &model, int row, int role)
{
    return model.data(model.index(row), role);
}
} // namespace

TEST_CASE("ErrorModel starts with preview samples", "[app][errors]")
{
    const ErrorModel model;
    REQUIRE(model.count() == 3);
    for (int row = 0; row < model.rowCount(); ++row) {
        CHECK(value(model, row, ErrorModel::PreviewRole).toBool());
        CHECK_FALSE(value(model, row, ErrorModel::TitleRole).toString().isEmpty());
        CHECK_FALSE(value(model, row, ErrorModel::DetailRole).toString().isEmpty());
    }
    CHECK(value(model, 0, ErrorModel::TitleRole).toString() == ErrorText::message(dv::ErrorCode::NotDicom));
}

TEST_CASE("ErrorModel clear empties the list and reports the count", "[app][errors]")
{
    ErrorModel model;
    int countChanges = 0;
    QObject::connect(&model, &ErrorModel::countChanged, [&] { ++countChanges; });

    model.clear();
    CHECK(model.count() == 0);
    CHECK(countChanges == 1);
    model.clear();
    CHECK(countChanges == 1); // no change, no signal
}

TEST_CASE("ErrorModel replaceWith shows the real errors", "[app][errors]")
{
    ErrorModel model;
    model.replaceWith({dv::makeError(dv::ErrorCode::NoPixelData, "no Pixel Data in dataset", "(7FE0,0010)")});

    REQUIRE(model.count() == 1);
    CHECK_FALSE(value(model, 0, ErrorModel::PreviewRole).toBool());
    CHECK(value(model, 0, ErrorModel::SeverityRole).toString() == QStringLiteral("error"));
    CHECK(value(model, 0, ErrorModel::TitleRole).toString() == ErrorText::message(dv::ErrorCode::NoPixelData));
    CHECK(value(model, 0, ErrorModel::DetailRole).toString() == QStringLiteral("no Pixel Data in dataset"));
    CHECK(value(model, 0, ErrorModel::TagRole).toString() == QStringLiteral("(7FE0,0010)"));
    CHECK(value(model, 0, ErrorModel::TimestampRole).toDateTime().isValid());
}

TEST_CASE("ErrorModel returns to the preview samples", "[app][errors]")
{
    ErrorModel model;
    model.replaceWith({dv::makeError(dv::ErrorCode::NotDicom)});
    model.setPreviewSamples();

    CHECK(model.count() == 3);
    CHECK(value(model, 2, ErrorModel::PreviewRole).toBool());
}

TEST_CASE("ErrorModel ignores invalid indexes", "[app][errors]")
{
    const ErrorModel model;
    CHECK_FALSE(model.data(model.index(99), ErrorModel::TitleRole).isValid());
    CHECK(model.roleNames().size() == 6);
}

TEST_CASE("ErrorModel refreshes the translated texts when the language changes", "[app][errors]")
{
    ErrorModel model;
    int refreshed = 0;
    QObject::connect(&model, &QAbstractItemModel::dataChanged, [&](const QModelIndex &first, const QModelIndex &last, const QList<int> &roles) {
        ++refreshed;
        CHECK(first.row() == 0);
        CHECK(last.row() == 2);
        CHECK(roles.contains(ErrorModel::TitleRole));
    });

    QEvent change(QEvent::LanguageChange);
    QCoreApplication::sendEvent(QCoreApplication::instance(), &change);
    CHECK(refreshed == 1);

    model.clear(); // nothing to refresh
    QCoreApplication::sendEvent(QCoreApplication::instance(), &change);
    CHECK(refreshed == 1);
}
