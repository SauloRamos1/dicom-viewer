#include "DicomController.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QtConcurrent>

#include <algorithm>
#include <filesystem>

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

// Rendering of one frame, timed; `image` is null on failure.
struct Rendered {
    QImage image;
    dv::DicomError error;
    double ms{};
};

Rendered render(const dv::DicomFile &file, int frame, const dv::RenderParams &params)
{
    QElapsedTimer timer;
    timer.start();
    Rendered out;
    auto rendered = dv::renderFrame(file, frame, params);
    if (!rendered) {
        out.error = std::move(rendered.error());
    } else {
        out.image = toQImage(*rendered);
        if (out.image.isNull())
            out.error = dv::makeError(dv::ErrorCode::DecodeFailed, "unusable image buffer");
    }
    out.ms = static_cast<double>(timer.nsecsElapsed()) / 1e6;
    return out;
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

void DicomController::setFrame(int frame)
{
    if (!m_file)
        return;
    frame = std::clamp(frame, 0, m_info.frameCount - 1);
    if (frame == m_frame)
        return;
    m_frame = frame;
    emit frameChanged();
    requestRender();
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
    requestRender();
}

void DicomController::setColormap(Colormap colormap)
{
    if (!m_file || colormap == m_colormap)
        return;
    m_colormap = colormap;
    emit colormapChanged();
    requestRender();
}

void DicomController::open(const QUrl &url)
{
    const quint64 generation = ++m_loadGeneration;
    setBusy(true);

    auto *watcher = new QFutureWatcher<LoadResult>(this);
    connect(watcher, &QFutureWatcher<LoadResult>::finished, this, [this, watcher] {
        LoadResult result = watcher->result();
        watcher->deleteLater();
        onLoadFinished(std::move(result));
    });
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    watcher->setFuture(QtConcurrent::run([generation, path] {
        LoadResult result;
        result.generation = generation;
        result.path = path;
        auto loaded = dv::DicomFile::load(toPath(path));
        if (loaded) {
            auto file = std::make_shared<const dv::DicomFile>(*std::move(loaded));
            if (file->info().isMonochrome()) {
                if (auto voi = dv::presetVoi(dv::WindowPreset::FromFile, *file))
                    result.voi = *voi;
            }
            Rendered first = render(*file, 0, {.voi = result.voi});
            if (first.image.isNull()) {
                result.error = std::move(first.error);
            } else {
                result.image = std::move(first.image);
                result.renderMs = first.ms;
                if (auto tags = dv::readMetadata(*file)) {
                    result.tags = *std::move(tags);
                    result.hasTags = true;
                }
                result.file = std::move(file);
            }
        } else {
            result.error = std::move(loaded.error());
            // A valid file without pixel data can still show its metadata.
            if (result.error.code == dv::ErrorCode::NoPixelData) {
                if (auto tags = dv::readMetadata(toPath(path))) {
                    result.tags = *std::move(tags);
                    result.hasTags = true;
                }
            }
        }
        return result;
    }));
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

    clearView(std::move(result.image));
    m_file = std::move(result.file);
    m_info = m_file->info();
    m_filePath = result.path;
    m_fileName = QFileInfo(result.path).fileName();
    m_voi = m_defaultVoi = result.voi;
    m_lastRenderMs = result.renderMs;
    if (const auto labels = dv::orientationLabels(m_info))
        m_orientation = *labels;
    m_metadataModel->setEntries(result.tags);
    m_errors->clear();
    emitViewChanged();
    emit loaded();
}

void DicomController::reset()
{
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
    if (const auto voi = dv::presetVoi(*preset, *m_file, m_frame)) {
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

// The one place the window changes: the width stays at 1 or more.
void DicomController::setWindow(dv::Voi voi)
{
    voi.width = std::max(voi.width, 1.0);
    if (!windowable() || voi == m_voi)
        return;
    m_voi = voi;
    emit windowChanged();
    requestRender();
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
    m_file.reset();
    m_info = {};
    m_orientation = {};
    m_filePath.clear();
    m_fileName.clear();
    m_frame = 0;
    m_voi = m_defaultVoi = {};
    m_invert = false;
    m_colormap = Gray;
    m_lastRenderMs = 0;
    m_metadataModel->clear();
    m_store->set(std::move(next));
    ++m_revision;
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
    emit lastRenderMsChanged();
    emit imageRevisionChanged();
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
        [file = m_file, frame = m_frame, params, serial = m_fileSerial] {
            Rendered rendered = render(*file, frame, params);
            return RenderResult{serial, std::move(rendered.image), std::move(rendered.error), rendered.ms};
        }));
}

void DicomController::onRenderFinished(RenderResult result)
{
    m_renderInFlight = false;
    if (result.fileSerial == m_fileSerial) {
        if (result.image.isNull()) {
            showError(result.error);
        } else {
            m_store->set(std::move(result.image));
            m_lastRenderMs = result.renderMs;
            ++m_revision;
            emit lastRenderMsChanged();
            emit imageRevisionChanged();
        }
    }
    if (m_renderPending && m_file)
        startRender();
}
