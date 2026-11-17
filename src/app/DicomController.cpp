// SPDX-License-Identifier: GPL-3.0-only
#include "DicomController.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QtConcurrent>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <numeric>
#include <optional>
#include <ranges>

#include "ErrorText.h"
#include "Windowing.h"

static_assert(static_cast<int>(dv::Colormap::Gray) == DicomController::Gray);
static_assert(static_cast<int>(dv::Colormap::Bone) == DicomController::Bone);
static_assert(static_cast<int>(dv::Colormap::Hot) == DicomController::Hot);
static_assert(static_cast<int>(dv::Colormap::Jet) == DicomController::Jet);

namespace {

std::filesystem::path toPath(const QString &path)
{
    const QByteArray utf8 = path.toUtf8();
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

// Warning entry for the files of a series folder that were left out: how many, and
// the first few by name with the reason.
dv::DicomError skippedFilesNotice(const std::vector<dv::SkippedFile> &skipped)
{
    constexpr std::size_t kListed = 5;
    QStringList lines;
    for (const dv::SkippedFile &file : skipped | std::views::take(kListed)) {
        const std::u8string name = file.path.filename().u8string();
        lines << QStringLiteral("%1 - %2").arg(
            QString::fromUtf8(reinterpret_cast<const char *>(name.data()), static_cast<qsizetype>(name.size())),
            ErrorText::message(file.error.code));
    }
    if (skipped.size() > kListed)
        lines << DicomController::tr("and %1 more").arg(skipped.size() - kListed);
    const int count = static_cast<int>(skipped.size());
    auto notice = dv::makeError(dv::ErrorCode::SkippedFiles,
                                (DicomController::tr("%n file(s) skipped: ", nullptr, count) + lines.join(QStringLiteral("; ")))
                                    .toStdString());
    notice.severity = dv::Severity::Warning;
    return notice;
}

// Result of preparing one frame, timed; `image` is null on failure.
struct Rendered {
    QImage image;
    dv::DicomError error;
    double ms{};
};

// Wraps the packed pixels in a QImage without copying; the image owns them.
QImage toPackedImage(dv::PackedFrame packed)
{
    auto *pixels = new std::vector<std::uint8_t>(std::move(packed.rgba));
    return QImage(
        pixels->data(), packed.width, packed.height, packed.width * 4, QImage::Format_RGBA8888,
        [](void *owned) { delete static_cast<std::vector<std::uint8_t> *>(owned); }, pixels);
}

// `gpu`: prepare the packed texture of the shader path instead of the windowed
// 8-bit image (then `params` is not used).
Rendered render(const dv::DicomFile &file, int frame, const dv::RenderParams &params, bool gpu)
{
    QElapsedTimer timer;
    timer.start();
    Rendered out;
    if (gpu) {
        auto packed = dv::packFrame(file, frame);
        if (!packed)
            out.error = std::move(packed.error());
        else
            out.image = toPackedImage(*std::move(packed));
    } else if (auto rendered = dv::renderFrame(file, frame, params)) {
        out.image = toQImage(*rendered);
    } else {
        out.error = std::move(rendered.error());
    }
    if (out.image.isNull() && out.error.message.empty())
        out.error = dv::makeError(dv::ErrorCode::DecodeFailed, "unusable image buffer");
    out.ms = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    return out;
}

constexpr size_t kSampleWindow = 10;

// A number as short as it reads: whole values without decimals, at most `decimals` otherwise.
QString shortNumber(double value, int decimals = 2)
{
    QString text = QString::number(value, 'f', decimals);
    while (text.endsWith(QLatin1Char('0')))
        text.chop(1);
    if (text.endsWith(QLatin1Char('.')))
        text.chop(1);
    return text == QLatin1String("-0") ? QStringLiteral("0") : text;
}

// Rec. 601 luma of interleaved RGB8.
std::vector<std::uint8_t> lumaOf(const std::vector<std::uint8_t> &rgb)
{
    std::vector<std::uint8_t> luma(rgb.size() / 3);
    for (size_t i = 0; i < luma.size(); ++i)
        luma[i] = static_cast<std::uint8_t>((299 * rgb[3 * i] + 587 * rgb[3 * i + 1] + 114 * rgb[3 * i + 2] + 500) / 1000);
    return luma;
}

std::vector<std::uint8_t> grayToRgb(const std::vector<std::uint8_t> &gray)
{
    std::vector<std::uint8_t> rgb(gray.size() * 3);
    for (size_t i = 0; i < gray.size(); ++i)
        rgb[3 * i] = rgb[3 * i + 1] = rgb[3 * i + 2] = gray[i];
    return rgb;
}

// Regular files of `folder`, by name; the series loader sorts the slices itself.
std::vector<std::filesystem::path> filesOf(const QString &folder)
{
    std::vector<std::filesystem::path> paths;
    for (const QFileInfo &entry : QDir(folder).entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name))
        paths.push_back(toPath(entry.absoluteFilePath()));
    return paths;
}

} // namespace

DicomController::DicomController(QObject *parent)
    : QObject(parent)
    , m_errors(new ErrorModel(this))
    , m_metadataModel(new MetadataModel(this))
    , m_metadata(new MetadataFilterModel(this))
{
    m_metadata->setSourceModel(m_metadataModel);
    connect(&m_renderWatcher, &QFutureWatcher<RenderResult>::finished, this,
            [this] { onRenderFinished(m_renderWatcher.result()); });
    connect(&m_meshWatcher, &QFutureWatcher<MeshResult>::finished, this,
            [this] { onMeshFinished(m_meshWatcher.result()); });
}

QString DicomController::photometric() const
{
    if (!m_file)
        return {};
    switch (m_info.photometric) {
    case dv::Photometric::Monochrome1: return QStringLiteral("MONOCHROME1");
    case dv::Photometric::Monochrome2: return QStringLiteral("MONOCHROME2");
    case dv::Photometric::Rgb: return QStringLiteral("RGB");
    case dv::Photometric::YbrFull: return QStringLiteral("YBR_FULL");
    case dv::Photometric::YbrFull422: return QStringLiteral("YBR_FULL_422");
    case dv::Photometric::PaletteColor: return QStringLiteral("PALETTE COLOR");
    case dv::Photometric::Other: break;
    }
    return {};
}

QString DicomController::transferSyntax() const
{
    const std::string &name = m_info.transferSyntaxName.empty() ? m_info.transferSyntaxUid : m_info.transferSyntaxName;
    return QString::fromStdString(name);
}

QString DicomController::pixelSpacingText() const
{
    const auto spacing = dv::physicalPixelSpacing(m_info);
    if (!spacing)
        return {};
    // Keep one decimal for whole values ("1.0"), like the numbers in the file.
    const auto text = [](double value) {
        const QString shortest = shortNumber(value, 3);
        return shortest.contains(QLatin1Char('.')) ? shortest : shortest + QStringLiteral(".0");
    };
    return QStringLiteral("%1 x %2 mm").arg(text((*spacing)[0]), text((*spacing)[1]));
}

QVariantMap DicomController::pixelAt(const QPointF &screen, const QSizeF &viewport, double zoom, const QPointF &pan) const
{
    if (!m_file)
        return {};
    const auto pixel = dv::screenToPixel({.viewportWidth = viewport.width(),
                                          .viewportHeight = viewport.height(),
                                          .columns = m_info.columns,
                                          .rows = m_info.rows,
                                          .pixelAspect = pixelAspect(),
                                          .transform = m_view,
                                          .zoom = zoom,
                                          .panX = pan.x(),
                                          .panY = pan.y()},
                                         screen.x(), screen.y());
    if (!pixel)
        return {};

    QString text;
    if (m_info.isMonochrome()) {
        const FrameSource source = frameSource(m_frame);
        const auto value = source.file->valueAt(pixel->x, pixel->y, source.frame);
        if (!value)
            return {};
        text = shortNumber(static_cast<double>(*value));
        if (m_info.modality == "CT")
            text += QStringLiteral(" HU");
    } else {
        // The shown 8-bit RGB frame; invert is undone so the values are the image's own.
        const QImage image = m_store->get();
        if (image.format() != QImage::Format_RGB888 || pixel->x >= image.width() || pixel->y >= image.height())
            return {};
        const uchar *rgb = image.constScanLine(pixel->y) + 3 * pixel->x;
        const auto channel = [this, rgb](int i) { return m_invert ? 255 - rgb[i] : static_cast<int>(rgb[i]); };
        text = QStringLiteral("R %1  G %2  B %3").arg(channel(0)).arg(channel(1)).arg(channel(2));
    }
    return {{QStringLiteral("x"), pixel->x}, {QStringLiteral("y"), pixel->y}, {QStringLiteral("text"), text}};
}

void DicomController::setFrame(int frame)
{
    if (!m_file)
        return;
    frame = std::clamp(frame, 0, frameCount() - 1);
    if (frame == m_frame)
        return;
    m_frame = frame;
    emit frameChanged();
    requestRender();
    requestMesh();
}

void DicomController::setWindowCenter(double center)
{
    setWindow({center, m_voi.width});
}

void DicomController::setWindowWidth(double width)
{
    setWindow({m_voi.center, width});
}

void DicomController::setInvert(bool invert)
{
    if (!m_file || invert == m_invert)
        return;
    m_invert = invert;
    emit invertChanged();
    displayChanged();
}

void DicomController::setColormap(Colormap colormap)
{
    if (!m_file || colormap == m_colormap)
        return;
    m_colormap = colormap;
    emit colormapChanged();
    displayChanged();
}

void DicomController::setFlipHorizontal(bool flip)
{
    auto view = m_view;
    view.flipHorizontal = flip;
    setViewTransform(view);
}

void DicomController::setFlipVertical(bool flip)
{
    auto view = m_view;
    view.flipVertical = flip;
    setViewTransform(view);
}

void DicomController::rotateRight()
{
    auto view = m_view;
    view.quarterTurns = (view.quarterTurns + 1) % 4;
    setViewTransform(view);
}

void DicomController::rotateLeft()
{
    auto view = m_view;
    view.quarterTurns = (view.quarterTurns + 3) % 4;
    setViewTransform(view);
}

void DicomController::resetTransform()
{
    setViewTransform({});
}

DicomController::FrameSource DicomController::frameSource(int frame) const
{
    if (m_slices.empty())
        return {m_file, frame};
    return {m_slices[static_cast<size_t>(frame)], 0};
}

quint64 DicomController::beginLoad(std::shared_ptr<std::atomic<bool>> &cancel)
{
    if (m_loadCancel)
        m_loadCancel->store(true); // the load in flight is obsolete, let it stop early
    m_loadCancel = cancel = std::make_shared<std::atomic<bool>>(false);
    setBusy(true);
    return ++m_loadGeneration;
}

void DicomController::runLoad(std::function<LoadResult()> job)
{
    auto *watcher = new QFutureWatcher<LoadResult>(this);
    connect(watcher, &QFutureWatcher<LoadResult>::finished, this, [this, watcher] {
        LoadResult result = watcher->result();
        watcher->deleteLater();
        onLoadFinished(std::move(result));
    });
    watcher->setFuture(QtConcurrent::run(std::move(job)));
}

void DicomController::open(const QUrl &url)
{
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (QFileInfo(path).isDir()) {
        openSeries(url);
        return;
    }
    std::shared_ptr<std::atomic<bool>> cancel;
    const quint64 generation = beginLoad(cancel);
    const bool useGpu = m_backend == Gpu && m_gpuAvailable;
    runLoad([generation, path, useGpu] { return loadFile(generation, path, useGpu); });
}

void DicomController::openSeries(const QUrl &folder)
{
    std::shared_ptr<std::atomic<bool>> cancel;
    const quint64 generation = beginLoad(cancel);
    const bool useGpu = m_backend == Gpu && m_gpuAvailable;
    const QString path = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    runLoad([generation, path, useGpu, cancel] { return loadFolder(generation, path, useGpu, cancel); });
}

// Window, first frame and metadata of a loaded file; fills result.file on success.
// `gpuCapable`: whether the shader path can render every frame of the load (a series
// needs its slices to agree, see dv::gpuSupported(span)).
// `volumeWindow`: the window of the whole volume, used when the file names none (the
// Min-Max of one slice of a series can be all air).
void DicomController::prepareFirstFrame(LoadResult &result, std::shared_ptr<const dv::DicomFile> file, bool useGpu,
                                        bool gpuCapable, std::optional<dv::Voi> volumeWindow)
{
    if (file->info().isMonochrome()) {
        if (volumeWindow && !file->info().defaultVoi) {
            result.voi = *volumeWindow;
        } else {
            auto voi = dv::presetVoi(dv::WindowPreset::FromFile, *file);
            if (!voi) {
                result.error = std::move(voi.error());
                return;
            }
            result.voi = *voi;
        }
    }
    result.gpuCapable = gpuCapable;
    result.gpu = useGpu && result.gpuCapable;
    Rendered first = render(*file, 0, {.voi = result.voi}, result.gpu);
    if (first.image.isNull()) {
        result.error = std::move(first.error);
        return;
    }
    result.image = std::move(first.image);
    result.renderMs = first.ms;
    if (auto tags = dv::readMetadata(*file)) {
        result.tags = *std::move(tags);
        result.hasTags = true;
    }
    result.file = std::move(file);
}

// Min-Max window over every voxel (the same formula as dv::WindowPreset::MinMax).
static dv::Voi minMaxWindow(const dv::Volume &volume)
{
    const auto [lo, hi] = std::ranges::minmax(volume.voxels);
    return {(static_cast<double>(hi) + lo + 1.0) / 2.0, static_cast<double>(hi) - lo + 1.0};
}

// Packs `volume` for the GPU (downsampled to the budget, 16-bit) into the result.
void DicomController::attachVolume(LoadResult &result, const dv::Volume &volume)
{
    const dv::PackedVolume packed = dv::packVolume(volume);
    auto data = std::make_shared<VolumeData>();
    data->width = packed.width;
    data->height = packed.height;
    data->depth = packed.depth;
    data->bytes = QByteArray(reinterpret_cast<const char *>(packed.voxels.data()),
                             static_cast<qsizetype>(packed.voxels.size() * sizeof(std::uint16_t)));
    result.volume = std::move(data);
    result.volumeSize = QVector3D(static_cast<float>(volume.width), static_cast<float>(volume.height),
                                  static_cast<float>(volume.depth));
    result.volumeSpacing = QVector3D(static_cast<float>(volume.spacing[0]), static_cast<float>(volume.spacing[1]),
                                     static_cast<float>(volume.spacing[2]));
    result.volumeDownsample = packed.downsample;
    result.volumeValueMin = packed.valueMin;
    result.volumeValueMax = packed.valueMax;
}

DicomController::LoadResult DicomController::loadFile(quint64 generation, const QString &path, bool useGpu)
{
    LoadResult result;
    result.generation = generation;
    result.path = path;
    try {
        auto loaded = dv::DicomFile::load(toPath(path));
        if (!loaded) {
            result.error = std::move(loaded.error());
            // A valid file without pixel data can still show its metadata.
            if (result.error.code == dv::ErrorCode::NoPixelData) {
                if (auto tags = dv::readMetadata(toPath(path))) {
                    result.tags = *std::move(tags);
                    result.hasTags = true;
                }
            }
            return result;
        }
        auto file = std::make_shared<const dv::DicomFile>(*std::move(loaded));
        const dv::ImageInfo &info = file->info();
        std::optional<dv::Volume> volume;
        if (info.frameCount > 1 && info.isMonochrome()) {
            if (auto stacked = dv::Volume::fromMultiFrame(*file))
                volume = *std::move(stacked);
            else
                result.notices.push_back(stacked.error());
        }
        prepareFirstFrame(result, file, useGpu, dv::gpuSupported(*file),
                          volume ? std::optional(minMaxWindow(*volume)) : std::nullopt);
        if (result.file && volume)
            attachVolume(result, *volume);
    } catch (const std::exception &e) { // out of memory while building the volume
        result = {};
        result.generation = generation;
        result.path = path;
        result.error = dv::makeError(dv::ErrorCode::DecodeFailed, e.what());
    }
    return result;
}

DicomController::LoadResult DicomController::loadFolder(quint64 generation, const QString &folder, bool useGpu,
                                                        const std::shared_ptr<std::atomic<bool>> &cancel)
{
    LoadResult result;
    result.generation = generation;
    result.path = folder;
    try {
        if (!QFileInfo(folder).isDir()) {
            result.error = dv::makeError(dv::ErrorCode::FileNotFound, folder.toStdString());
            return result;
        }
        const auto paths = filesOf(folder);
        auto series = dv::loadSeries(paths, cancel.get());
        if (!series) {
            result.error = std::move(series.error());
            return result;
        }
        if (!series->skipped.empty())
            result.notices.push_back(skippedFilesNotice(series->skipped));
        const bool gpuCapable = dv::gpuSupported(series->slices);
        std::vector<std::shared_ptr<const dv::DicomFile>> slices;
        for (dv::DicomFile &slice : series->slices)
            slices.push_back(std::make_shared<const dv::DicomFile>(std::move(slice)));
        const bool isVolume = slices.size() > 1;
        prepareFirstFrame(result, slices.front(), useGpu, gpuCapable,
                          isVolume ? std::optional(minMaxWindow(series->volume)) : std::nullopt);
        if (!result.file)
            return result;
        if (isVolume) {
            result.slices = std::move(slices);
            attachVolume(result, series->volume);
        } else {
            auto notice = dv::makeError(dv::ErrorCode::SingleSliceSeries, "1 slice");
            notice.severity = dv::Severity::Info;
            result.notices.push_back(std::move(notice));
        }
    } catch (const std::exception &e) {
        result = {};
        result.generation = generation;
        result.path = folder;
        result.error = dv::makeError(dv::ErrorCode::DecodeFailed, e.what());
    }
    return result;
}

void DicomController::onLoadFinished(LoadResult result)
{
    if (result.generation != m_loadGeneration)
        return; // superseded by a newer open() or by reset()
    setBusy(false);

    if (!result.file) {
        showError(result.error);
        if (result.error.code != dv::ErrorCode::NoPixelData || !result.hasTags)
            return;
        clearView();
        m_filePath = result.path;
        m_fileName = QFileInfo(result.path).fileName();
        m_metadataModel->setEntries(result.tags);
        emitViewChanged();
        return;
    }

    clearView(result.gpu ? QImage() : std::move(result.image));
    m_file = std::move(result.file);
    m_slices = std::move(result.slices);
    m_volume = std::move(result.volume);
    m_volumeSize = result.volumeSize;
    m_volumeSpacing = result.volumeSpacing;
    m_volumeDownsample = result.volumeDownsample;
    m_volumeValueMin = result.volumeValueMin;
    m_volumeValueMax = result.volumeValueMax;
    m_info = m_file->info();
    m_valueRange = dv::modalityRange(m_info);
    m_gpuCapable = result.gpuCapable;
    m_gpuParams = dv::gpuParams(m_info);
    m_filePath = result.path;
    m_fileName = QFileInfo(result.path).fileName();
    m_voi = m_defaultVoi = result.voi;
    if (result.gpu) {
        publishGpuFrame(std::move(result.image), 0, result.renderMs);
        m_shownGpu = true;
    } else {
        recordSample(result.renderMs, Cpu);
    }
    if (const auto labels = dv::orientationLabels(m_info))
        m_orientation = *labels;
    m_metadataModel->setEntries(result.tags);
    m_errors->clear();
    bool failed = false;
    for (const dv::DicomError &notice : result.notices) {
        m_errors->append(notice);
        failed |= notice.severity == dv::Severity::Error;
    }
    emitViewChanged();
    emit loaded();
    if (failed)
        emit errorOccurred();
    requestMesh();
    if (wantsGpu() != m_shownGpu && m_gpuCapable) // the backend was switched while loading
        requestRender();
}

void DicomController::reset()
{
    if (m_loadCancel)
        m_loadCancel->store(true);
    ++m_loadGeneration;
    setBusy(false);
    clearView();
    m_errors->setPreviewSamples();
    emitViewChanged();
}

void DicomController::applyPreset(const QString &name)
{
    static const QHash<QString, dv::WindowPreset> presets{
        {QStringLiteral("default"), dv::WindowPreset::FromFile},
        {QStringLiteral("minmax"), dv::WindowPreset::MinMax},
        {QStringLiteral("brain"), dv::WindowPreset::CtBrain},
        {QStringLiteral("lung"), dv::WindowPreset::CtLung},
        {QStringLiteral("bone"), dv::WindowPreset::CtBone},
        {QStringLiteral("abdomen"), dv::WindowPreset::CtAbdomen},
    };
    const auto preset = presets.find(name);
    if (!windowable() || preset == presets.end())
        return;
    const FrameSource source = frameSource(m_frame);
    if (const auto voi = dv::presetVoi(*preset, *source.file, source.frame)) {
        setWindow(*voi);
    } else {
        showError(voi.error());
    }
}

void DicomController::resetWindow()
{
    if (!windowable())
        return;
    setWindow(m_defaultVoi);
}

void DicomController::setViewTransform(dv::ViewTransform view)
{
    if (!m_file || view == m_view)
        return;
    m_view = view;
    emit viewTransformChanged();
    emit orientationChanged();
}

DicomController::ShownLabels DicomController::shownLabels() const
{
    const auto labels = dv::transformLabels(m_orientation, m_view);
    return {QString::fromStdString(labels.left), QString::fromStdString(labels.right),
            QString::fromStdString(labels.top), QString::fromStdString(labels.bottom)};
}

// The one place the window changes: the width stays at 1 or more.
void DicomController::setWindow(dv::Voi voi)
{
    voi.width = std::max(voi.width, 1.0);
    if (!windowable() || voi == m_voi)
        return;
    m_voi = voi;
    emit windowChanged();
    displayChanged();
}

void DicomController::setBusy(bool busy)
{
    if (busy == m_busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

// Drops everything tied to the displayed file (not the errors); `next` is the
// frame to serve meanwhile.
void DicomController::clearView(QImage next)
{
    ++m_fileSerial;
    m_renderPending = false;
    m_meshPending = false;
    m_file.reset();
    m_slices.clear();
    m_volume.reset();
    m_volumeSize = m_volumeSpacing = {};
    m_volumeDownsample = 1;
    m_volumeValueMin = 0;
    m_volumeValueMax = 1;
    m_mesh.reset();
    m_info = {};
    m_orientation = {};
    m_valueRange = {};
    m_view = {};
    m_filePath.clear();
    m_fileName.clear();
    m_frame = 0;
    m_voi = m_defaultVoi = {};
    m_invert = false;
    m_colormap = Gray;
    m_lastRenderMs = m_averageRenderMs = 0;
    m_lastRenderBackend = m_backend;
    m_samples.clear();
    m_gpuTimer.invalidate();
    m_gpuCapable = false;
    m_shownGpu = false;
    m_gpuParams = {};
    m_gpuFrame = -1;
    m_metadataModel->clear();
    m_store->set(std::move(next));
    m_gpuStore->set({});
    ++m_revision;
    ++m_gpuRevision;
}

void DicomController::emitViewChanged()
{
    emit hasImageChanged();
    emit fileChanged();
    emit frameCountChanged();
    emit frameChanged();
    emit windowChanged();
    emit invertChanged();
    emit colormapChanged();
    emit infoChanged();
    emit orientationChanged();
    emit viewTransformChanged();
    emit lastRenderMsChanged();
    emit imageRevisionChanged();
    emit gpuImageRevisionChanged();
    emit gpuActiveChanged();
    emit volumeChanged();
    emit heightmapChanged();
}

void DicomController::showError(const dv::DicomError &error)
{
    m_errors->replaceWith({error});
    emit errorOccurred();
}

// Renders run one at a time; changes that arrive meanwhile are coalesced into
// one more render with the latest parameters.
void DicomController::requestRender()
{
    if (!m_file)
        return;
    if (m_renderInFlight)
        m_renderPending = true;
    else
        startRender();
}

void DicomController::startRender()
{
    m_renderInFlight = true;
    m_renderPending = false;
    const dv::RenderParams params{.voi = m_voi, .invert = m_invert, .colormap = static_cast<dv::Colormap>(m_colormap)};
    m_renderWatcher.setFuture(QtConcurrent::run(
        [source = frameSource(m_frame), frame = m_frame, params, serial = m_fileSerial, gpu = wantsGpu()] {
            Rendered rendered = render(*source.file, source.frame, params, gpu);
            return RenderResult{serial, frame, gpu, std::move(rendered.image), std::move(rendered.error), rendered.ms};
        }));
}

void DicomController::onRenderFinished(RenderResult result)
{
    m_renderInFlight = false;
    if (result.fileSerial == m_fileSerial) {
        if (result.image.isNull()) {
            showError(result.error);
        } else {
            if (result.gpu) {
                publishGpuFrame(std::move(result.image), result.frame, result.renderMs);
            } else {
                m_store->set(std::move(result.image));
                ++m_revision;
                recordSample(result.renderMs, Cpu);
                emit imageRevisionChanged();
            }
            // Show the new path only once its image is in, and not while a newer
            // render is still due (it would flash stale parameters).
            if (!m_renderPending && result.gpu == wantsGpu())
                setShownGpu(result.gpu);
        }
    }
    if (m_renderPending && m_file)
        startRender();
}

void DicomController::displayChanged()
{
    requestMesh();
    if (!wantsGpu())
        requestRender();
    else if (m_shownGpu)
        startGpuSample(0);
}

void DicomController::setShownGpu(bool shown)
{
    if (shown == m_shownGpu)
        return;
    m_shownGpu = shown;
    emit gpuActiveChanged();
}

void DicomController::publishGpuFrame(QImage packed, int frame, double prepareMs)
{
    m_gpuStore->set(std::move(packed));
    m_gpuFrame = frame;
    ++m_gpuRevision;
    emit gpuImageRevisionChanged();
    startGpuSample(prepareMs);
}

void DicomController::recordSample(double ms, RenderBackend backend)
{
    m_lastRenderMs = ms;
    m_lastRenderBackend = backend;
    m_samples.push_back(ms);
    if (m_samples.size() > kSampleWindow)
        m_samples.pop_front();
    m_averageRenderMs = std::accumulate(m_samples.begin(), m_samples.end(), 0.0) / static_cast<double>(m_samples.size());
    emit lastRenderMsChanged();
}

// A GPU sample opens at the change and closes at the next frame swap.
void DicomController::startGpuSample(double baseMs)
{
    if (!m_gpuTimer.isValid()) {
        m_gpuBaseMs = 0;
        m_gpuTimer.start();
    }
    m_gpuBaseMs += baseMs;
}

void DicomController::reportFrameSwapped()
{
    if (!m_gpuTimer.isValid())
        return;
    const double ms = m_gpuBaseMs + static_cast<double>(m_gpuTimer.nsecsElapsed()) / 1e6;
    m_gpuTimer.invalidate();
    recordSample(ms, Gpu);
}

void DicomController::setView3D(bool shown)
{
    if (shown == m_view3D)
        return;
    m_view3D = shown;
    emit view3DChanged();
    if (shown) {
        requestMesh();
    } else if (m_mesh) { // leaving 3D frees the mesh; it is rebuilt on return
        m_mesh.reset();
        emit heightmapChanged();
    }
}

// Like renders, heightmap builds run one at a time and changes that arrive
// meanwhile become one more build with the latest parameters.
void DicomController::requestMesh()
{
    if (!wantsMesh())
        return;
    if (m_meshInFlight)
        m_meshPending = true;
    else
        startMesh();
}

void DicomController::startMesh()
{
    m_meshInFlight = true;
    m_meshPending = false;
    const dv::RenderParams params{.voi = m_voi, .invert = m_invert, .colormap = static_cast<dv::Colormap>(m_colormap)};
    m_meshWatcher.setFuture(QtConcurrent::run([source = frameSource(m_frame), params, serial = m_fileSerial] {
        return buildMesh(source, params, serial);
    }));
}

// Height = the windowed gray level without invert or colormap; colour = what the
// 2D view would show. Colour images take the luma of their RGB as height.
DicomController::MeshResult DicomController::buildMesh(const FrameSource &source, const dv::RenderParams &params,
                                                       quint64 serial)
{
    MeshResult out{.fileSerial = serial};
    const auto fail = [&out](dv::DicomError error) {
        out.error = std::move(error);
        return std::move(out);
    };
    try {
        const auto plain = dv::renderFrame(*source.file, source.frame, {.voi = params.voi});
        if (!plain)
            return fail(plain.error());
        const bool recolour = params.invert || params.colormap != dv::Colormap::Gray;
        std::optional<dv::RenderedImage> shown;
        if (recolour) { // style the decoded frame instead of decoding it again
            shown = *plain;
            dv::stylize(*shown, params);
        }
        const dv::RenderedImage &colour = shown ? *shown : *plain;
        const std::vector<std::uint8_t> levels = plain->channels == 1 ? plain->data : lumaOf(plain->data);
        std::vector<std::uint8_t> rgb;
        if (colour.channels == 3)
            rgb = colour.data;
        else if (recolour)
            rgb = grayToRgb(colour.data);
        auto mesh = std::make_shared<dv::HeightmapMesh>(dv::buildHeightmap(plain->width, plain->height, levels, rgb));
        if (mesh->vertices.empty())
            return fail(dv::makeError(dv::ErrorCode::DecodeFailed, "unusable image buffer"));
        out.mesh = std::move(mesh);
    } catch (const std::exception &e) { // out of memory on a very large image
        return fail(dv::makeError(dv::ErrorCode::DecodeFailed, e.what()));
    }
    return out;
}

void DicomController::onMeshFinished(MeshResult result)
{
    m_meshInFlight = false;
    if (result.fileSerial == m_fileSerial && wantsMesh()) {
        if (result.mesh) {
            m_mesh = std::move(result.mesh);
            emit heightmapChanged();
        } else {
            showError(result.error);
        }
    }
    if (m_meshPending && wantsMesh())
        startMesh();
}

void DicomController::setRenderBackend(RenderBackend backend)
{
    if ((backend == Gpu && !m_gpuAvailable) || backend == m_backend)
        return;
    m_backend = backend;
    m_samples.clear();
    m_gpuTimer.invalidate();
    emit renderBackendChanged();
    if (!m_file || !m_gpuCapable) // nothing on screen changes for colour images
        return;
    if (wantsGpu() && m_gpuFrame == m_frame) {
        setShownGpu(true); // the packed frame is still in the store
        startGpuSample(0);
    } else {
        requestRender();
    }
}

void DicomController::reportGpuUnavailable(const QString &reason)
{
    if (!m_gpuAvailable)
        return;
    m_gpuAvailable = false;
    emit gpuAvailableChanged();
    auto notice = dv::makeError(dv::ErrorCode::GpuUnavailable, reason.toStdString());
    notice.severity = dv::Severity::Info;
    m_errors->append(notice);
    setRenderBackend(Cpu);
}
