#include <catch2/catch_test_macros.hpp>

#include <set>

#include "MetadataModel.h"
#include "TestSupport.h"

namespace {

dv::TagEntry entry(std::uint16_t group, std::uint16_t element, const char *name, const char *value, int depth = 0)
{
    return {.group = group, .element = element, .name = name, .vr = "LO", .value = value, .depth = depth};
}

QString text(const QAbstractItemModel &model, int row, int role)
{
    return model.index(row, 0).data(role).toString();
}

} // namespace

TEST_CASE("MetadataModel lists every element of a file by category", "[app][metadata]")
{
    const auto tags = dv::readMetadata(dv::test::dataFile("CT_small.dcm"));
    REQUIRE(tags.has_value());

    MetadataModel model;
    model.setEntries(*tags);
    REQUIRE(model.rowCount() == static_cast<int>(tags->size()));

    // Categories form contiguous runs (one ListView section each) in a fixed order.
    std::vector<QString> runs;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QString category = text(model, row, MetadataModel::CategoryRole);
        if (runs.empty() || runs.back() != category)
            runs.push_back(category);
    }
    CHECK(std::set<QString>(runs.begin(), runs.end()).size() == runs.size());
    const QStringList order{"patient", "study", "image", "other"};
    int last = -1;
    for (const QString &category : runs) {
        CHECK(order.indexOf(category) > last);
        last = static_cast<int>(order.indexOf(category));
    }

    // Patient Name is a Patient element with its tag text, name and VR.
    bool found = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (text(model, row, MetadataModel::TagRole) != QStringLiteral("(0010,0010)"))
            continue;
        found = true;
        CHECK(text(model, row, MetadataModel::CategoryRole) == QStringLiteral("patient"));
        CHECK(text(model, row, MetadataModel::NameRole) == QStringLiteral("Patient Name"));
        CHECK(text(model, row, MetadataModel::VrRole) == QStringLiteral("PN"));
        CHECK(model.index(row, 0).data(MetadataModel::GroupRole).toInt() == 0x0010);
    }
    CHECK(found);
}

TEST_CASE("MetadataModel keeps sequence items after their parent", "[app][metadata]")
{
    MetadataModel model;
    model.setEntries({
        entry(0x0008, 0x1140, "Referenced Image Sequence", "", 0),
        entry(0x0018, 0x0050, "Slice Thickness", "5", 1),
        entry(0x0010, 0x0020, "Patient ID", "P1", 0),
        entry(0x0018, 0x0060, "KVP", "120", 0),
    });

    REQUIRE(model.rowCount() == 4);
    CHECK(text(model, 0, MetadataModel::NameRole) == QStringLiteral("Patient ID"));
    CHECK(text(model, 0, MetadataModel::CategoryRole) == QStringLiteral("patient"));
    CHECK(text(model, 1, MetadataModel::NameRole) == QStringLiteral("Referenced Image Sequence"));
    CHECK(text(model, 2, MetadataModel::NameRole) == QStringLiteral("Slice Thickness"));
    CHECK(text(model, 2, MetadataModel::CategoryRole) == QStringLiteral("study")); // inherited from the sequence
    CHECK(model.index(2, 0).data(MetadataModel::DepthRole).toInt() == 1);
    CHECK(text(model, 3, MetadataModel::CategoryRole) == QStringLiteral("other"));
}

TEST_CASE("MetadataFilterModel matches tag, name and value case-insensitively", "[app][metadata]")
{
    MetadataModel model;
    model.setEntries({
        entry(0x0010, 0x0010, "Patient's Name", "DOE^JANE"),
        entry(0x0010, 0x0020, "Patient ID", "ABC-123"),
        entry(0x0008, 0x0060, "Modality", "CT"),
        entry(0x0028, 0x0010, "Rows", "128"),
    });
    MetadataFilterModel filter;
    filter.setSourceModel(&model);
    int changes = 0;
    QObject::connect(&filter, &MetadataFilterModel::filterTextChanged, [&] { ++changes; });
    REQUIRE(filter.rowCount() == 4);

    SECTION("by tag text")
    {
        filter.setFilterText(QStringLiteral("(0010,0020)"));
        REQUIRE(filter.rowCount() == 1);
        CHECK(text(filter, 0, MetadataModel::NameRole) == QStringLiteral("Patient ID"));
        filter.setFilterText(QStringLiteral("0010"));
        CHECK(filter.rowCount() == 3); // two (0010,xxxx) tags and Rows (0028,0010)
    }
    SECTION("by name")
    {
        filter.setFilterText(QStringLiteral("MODALITY"));
        REQUIRE(filter.rowCount() == 1);
        CHECK(text(filter, 0, MetadataModel::ValueRole) == QStringLiteral("CT"));
    }
    SECTION("by value")
    {
        filter.setFilterText(QStringLiteral("doe^"));
        REQUIRE(filter.rowCount() == 1);
        CHECK(text(filter, 0, MetadataModel::TagRole) == QStringLiteral("(0010,0010)"));
    }
    SECTION("no match and clearing the filter")
    {
        filter.setFilterText(QStringLiteral("zzz"));
        CHECK(filter.rowCount() == 0);
        filter.setFilterText({});
        CHECK(filter.rowCount() == 4);
        CHECK(changes == 2);
        filter.setFilterText({});
        CHECK(changes == 2);
    }
}

TEST_CASE("MetadataModel clear empties the list", "[app][metadata]")
{
    MetadataModel model;
    MetadataFilterModel filter;
    filter.setSourceModel(&model);
    model.setEntries({entry(0x0008, 0x0060, "Modality", "CT")});
    REQUIRE(filter.rowCount() == 1);

    model.clear();
    CHECK(model.rowCount() == 0);
    CHECK(filter.rowCount() == 0);
}
