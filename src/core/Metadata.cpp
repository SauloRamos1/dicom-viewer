#include "Metadata.h"

#include <cctype>
#include <mutex>
#include <string>

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcvr.h>

#include "DicomFileState.h"
#include "Guarded.h"

namespace dv {

namespace {

constexpr std::size_t kMaxValueLength = 128;

bool isBinary(const DcmElement& element)
{
    switch (element.getVR()) {
    case EVR_OB:
    case EVR_OW:
    case EVR_OF:
    case EVR_OD:
    case EVR_OL:
    case EVR_OV:
    case EVR_UN:
    case EVR_PixelData:
    case EVR_ox:
    case EVR_px:
        return true;
    default:
        return false;
    }
}

// Shortens `text` to kMaxValueLength bytes without splitting a UTF-8 sequence.
bool truncate(std::string& text)
{
    if (text.size() <= kMaxValueLength)
        return false;
    std::size_t cut = kMaxValueLength;
    while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80)
        --cut;
    text.resize(cut);
    text += "...";
    return true;
}

// "PatientName" -> "Patient Name", "SOPInstanceUID" -> "SOP Instance UID".
std::string humanize(const std::string& keyword)
{
    std::string text;
    for (std::size_t i = 0; i < keyword.size(); ++i) {
        const bool upper = std::isupper(static_cast<unsigned char>(keyword[i])) != 0;
        const bool prevLower = i > 0 && std::islower(static_cast<unsigned char>(keyword[i - 1])) != 0;
        const bool prevUpper = i > 0 && std::isupper(static_cast<unsigned char>(keyword[i - 1])) != 0;
        const bool nextLower = i + 1 < keyword.size() && std::islower(static_cast<unsigned char>(keyword[i + 1])) != 0;
        if (upper && (prevLower || (prevUpper && nextLower)))
            text += ' ';
        text += keyword[i];
    }
    return text;
}

std::string tagName(DcmTag& tag)
{
    if (tag.getGroup() % 2 != 0)
        return "Private Tag";
    const std::string name = tag.getTagName();
    return name == DcmTag_ERROR_TagName ? "Unknown Tag" : humanize(name);
}

TagEntry makeEntry(DcmElement& element, int depth)
{
    DcmTag tag(element.getTag());
    TagEntry entry{.group = tag.getGroup(),
                   .element = tag.getElement(),
                   .name = tagName(tag),
                   .vr = DcmVR(element.getVR()).getVRName(),
                   .depth = depth};
    if (isBinary(element)) {
        entry.value = "[binary " + std::to_string(element.getLength()) + " bytes]";
    } else {
        OFString value;
        element.getOFStringArray(value);
        entry.value = value.c_str();
        entry.truncated = truncate(entry.value);
    }
    return entry;
}

void walk(DcmItem& item, int depth, std::vector<TagEntry>& out)
{
    for (unsigned long i = 0; i < item.card(); ++i) {
        DcmElement* element = item.getElement(i);
        if (element == nullptr)
            continue;
        TagEntry entry = makeEntry(*element, depth);
        auto* sequence = dynamic_cast<DcmSequenceOfItems*>(element);
        if (sequence == nullptr) {
            out.push_back(std::move(entry));
            continue;
        }
        const unsigned long items = sequence->card();
        entry.value = "[sequence, " + std::to_string(items) + (items == 1 ? " item]" : " items]");
        out.push_back(std::move(entry));
        for (unsigned long n = 0; n < items; ++n) {
            if (DcmItem* nested = sequence->getItem(n))
                walk(*nested, depth + 1, out);
        }
    }
}

std::vector<TagEntry> collect(DcmFileFormat& file)
{
    std::vector<TagEntry> entries;
    walk(*file.getMetaInfo(), 0, entries);
    walk(*file.getDataset(), 0, entries);
    return entries;
}

} // namespace

std::expected<std::vector<TagEntry>, DicomError> readMetadata(const std::filesystem::path& path)
{
    return guarded<std::vector<TagEntry>>([&]() -> std::expected<std::vector<TagEntry>, DicomError> {
        DcmFileFormat file;
        if (auto parsed = parseFile(path, file); !parsed)
            return std::unexpected(std::move(parsed.error()));
        return collect(file);
    });
}

std::expected<std::vector<TagEntry>, DicomError> readMetadata(const DicomFile& file)
{
    return guarded<std::vector<TagEntry>>([&]() -> std::expected<std::vector<TagEntry>, DicomError> {
        const std::scoped_lock lock(file.state().mutex);
        return collect(file.state().file);
    });
}

} // namespace dv
