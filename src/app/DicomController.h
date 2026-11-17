// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector3D>
#include <QtQml/qqmlregistration.h>

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

#include "DicomFile.h"
#include "DicomImageProvider.h"
#include "DicomError.h"
#include "ErrorModel.h"
#include "GpuTexture.h"
#include "Heightmap.h"
#include "MetadataModel.h"
#include "Metadata.h"
#include "Orientation.h"
#include "Voi.h"
#include "Volume.h"
#include "VolumeGpu.h"
#include "Windowing.h"

// Application state behind the UI: the open file, what is displayed (frame,
// window, invert, colormap) and the models of the side panels. Files are loaded
// and frames rendered on worker threads; results come back through the event
// loop and are dropped when a newer open() or reset() made them obsolete.
//
// Two render paths show the same image. CPU: DCMTK windows the frame to 8 bits
// (re-rendered on every W/L, invert or colormap change) and QML shows it through
// image://dicom. GPU: the raw frame is uploaded once, packed in an RGBA8 texture
// (see GpuTexture.h) through image://dicomraw, and a shader applies the window,
// invert and colormap, so those changes only move uniforms. Colour images and
// anything the shader cannot do stay on the CPU path (`gpuCapable`).
//
// A folder opens as a series: its slices become the frames of the 2D view (the
// frame slider steps through them) and a volume for the 3D view. A multi-frame
// monochrome file gets a volume too. Without a volume the 3D view shows the
// current frame as a heightmap; both are built here, off the GUI thread, and
// handed to the 3D scene by shared pointer (heightmap(), volumeData()).
//
// A failed load leaves the displayed image, metadata and window untouched and
// only fills the error list. The one exception is a valid DICOM file without
// pixel data: it replaces the view with a metadata-only one plus the error.
class DicomController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasImage READ hasImage NOTIFY hasImageChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY fileChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY fileChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    // True for monochrome images, the only ones the window applies to.
    Q_PROPERTY(bool windowable READ windowable NOTIFY infoChanged)
    Q_PROPERTY(double windowCenter READ windowCenter WRITE setWindowCenter NOTIFY windowChanged)
    Q_PROPERTY(double windowWidth READ windowWidth WRITE setWindowWidth NOTIFY windowChanged)
    Q_PROPERTY(bool invert READ invert WRITE setInvert NOTIFY invertChanged)
    Q_PROPERTY(Colormap colormap READ colormap WRITE setColormap NOTIFY colormapChanged)
    // Bumped on every new rendered image; part of the image://dicom/<revision> URL.
    Q_PROPERTY(int imageRevision READ imageRevision NOTIFY imageRevisionChanged)
    // CPU: DicomImage render + 8-bit conversion. GPU: texture preparation on a
    // frame/file change, plus the time from the change to the next frame swap.
    // lastRenderBackend is the path that produced lastRenderMs; averageRenderMs
    // is the mean of the last 10 such samples since the backend was chosen.
    Q_PROPERTY(double lastRenderMs READ lastRenderMs NOTIFY lastRenderMsChanged)
    Q_PROPERTY(double averageRenderMs READ averageRenderMs NOTIFY lastRenderMsChanged)
    Q_PROPERTY(RenderBackend lastRenderBackend READ lastRenderBackend NOTIFY lastRenderMsChanged)

    // The chosen render path; the GPU one needs gpuAvailable. gpuCapable: the
    // open image can use the shader. gpuActive: the shader content is the one on
    // screen (it flips only once the new path's image is ready, so switching
    // never shows a stale frame). gpuImageRevision is part of the
    // image://dicomraw/<revision> URL of the packed frame; the rest are the
    // shader constants of the open file (GpuTexture.h).
    Q_PROPERTY(RenderBackend renderBackend READ renderBackend WRITE setRenderBackend NOTIFY renderBackendChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY gpuAvailableChanged)
    Q_PROPERTY(bool gpuCapable READ gpuCapable NOTIFY infoChanged)
    Q_PROPERTY(bool gpuActive READ gpuActive NOTIFY gpuActiveChanged)
    Q_PROPERTY(int gpuImageRevision READ gpuImageRevision NOTIFY gpuImageRevisionChanged)
    Q_PROPERTY(double rescaleSlope READ rescaleSlope NOTIFY infoChanged)
    Q_PROPERTY(double rescaleIntercept READ rescaleIntercept NOTIFY infoChanged)
    Q_PROPERTY(double storedMin READ storedMin NOTIFY infoChanged)
    Q_PROPERTY(bool monochrome1 READ monochrome1 NOTIFY infoChanged)

    Q_PROPERTY(int rows READ rows NOTIFY infoChanged)
    Q_PROPERTY(int columns READ columns NOTIFY infoChanged)
    Q_PROPERTY(int bitsStored READ bitsStored NOTIFY infoChanged)
    Q_PROPERTY(QString photometric READ photometric NOTIFY infoChanged)
    Q_PROPERTY(QString transferSyntax READ transferSyntax NOTIFY infoChanged)
    Q_PROPERTY(QString modality READ modality NOTIFY infoChanged)
    Q_PROPERTY(QString patientName READ patientName NOTIFY infoChanged)
    Q_PROPERTY(QString patientId READ patientId NOTIFY infoChanged)

    // Value range of the stored bits (modality units): bounds for the window controls.
    Q_PROPERTY(double valueMin READ valueMin NOTIFY infoChanged)
    Q_PROPERTY(double valueMax READ valueMax NOTIFY infoChanged)

    // How the view shows the image: quarter turns clockwise (0..3), then mirrored
    // on screen. The orientation labels below already include it; View2D draws it.
    Q_PROPERTY(int viewRotation READ viewRotation NOTIFY viewTransformChanged)
    Q_PROPERTY(bool flipHorizontal READ flipHorizontal WRITE setFlipHorizontal NOTIFY viewTransformChanged)
    Q_PROPERTY(bool flipVertical READ flipVertical WRITE setFlipVertical NOTIFY viewTransformChanged)

    Q_PROPERTY(QString orientationLeft READ orientationLeft NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationRight READ orientationRight NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationTop READ orientationTop NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationBottom READ orientationBottom NOTIFY orientationChanged)

    // The 3D view is the one shown. Only then is the heightmap of a single
    // image built (hasVolume decides which kind of 3D the file gets).
    Q_PROPERTY(bool view3D READ view3D WRITE setView3D NOTIFY view3DChanged)
    // A volume exists: voxels (width, height, depth), millimetres per voxel, the
    // factor it was downsampled by to fit the GPU budget (1 = not) and the
    // modality range the 16-bit samples span (VolumeGpu.h).
    Q_PROPERTY(bool hasVolume READ hasVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVector3D volumeSize READ volumeSize NOTIFY volumeChanged)
    Q_PROPERTY(QVector3D volumeSpacing READ volumeSpacing NOTIFY volumeChanged)
    Q_PROPERTY(int volumeDownsample READ volumeDownsample NOTIFY volumeChanged)
    Q_PROPERTY(double volumeValueMin READ volumeValueMin NOTIFY volumeChanged)
    Q_PROPERTY(double volumeValueMax READ volumeValueMax NOTIFY volumeChanged)

    Q_PROPERTY(ErrorModel *errors READ errors CONSTANT)
    Q_PROPERTY(MetadataFilterModel *metadata READ metadata CONSTANT)

public:
    // Same values as dv::Colormap.
    enum Colormap { Gray, Bone, Hot, Jet };
    Q_ENUM(Colormap)

    enum RenderBackend { Cpu, Gpu };
    Q_ENUM(RenderBackend)

    // The volume as uploaded to the GPU: R16 texels (VolumeGpu.h), in an owned
    // buffer the 3D scene may keep while the controller moves on.
    struct VolumeData {
        int width{};
        int height{};
        int depth{};
        QByteArray bytes;
    };

    explicit DicomController(QObject *parent = nullptr);

    bool hasImage() const { return m_file != nullptr; }
    bool busy() const { return m_busy; }
    QString fileName() const { return m_fileName; }
    QString filePath() const { return m_filePath; }
    int frameCount() const { return !m_file ? 0 : m_slices.empty() ? m_info.frameCount : static_cast<int>(m_slices.size()); }
    int frame() const { return m_frame; }
    bool windowable() const { return m_file && m_info.isMonochrome(); }
    double windowCenter() const { return m_voi.center; }
    double windowWidth() const { return m_voi.width; }
    bool invert() const { return m_invert; }
    Colormap colormap() const { return m_colormap; }
    int imageRevision() const { return m_revision; }
    double lastRenderMs() const { return m_lastRenderMs; }
    double averageRenderMs() const { return m_averageRenderMs; }
    RenderBackend lastRenderBackend() const { return m_lastRenderBackend; }

    RenderBackend renderBackend() const { return m_backend; }
    bool gpuAvailable() const { return m_gpuAvailable; }
    bool gpuCapable() const { return m_gpuCapable; }
    bool gpuActive() const { return m_shownGpu; }
    int gpuImageRevision() const { return m_gpuRevision; }
    double rescaleSlope() const { return m_gpuParams.slope; }
    double rescaleIntercept() const { return m_gpuParams.intercept; }
    double storedMin() const { return m_gpuParams.storedMin; }
    bool monochrome1() const { return m_gpuParams.monochrome1; }

    int rows() const { return m_info.rows; }
    int columns() const { return m_info.columns; }
    int bitsStored() const { return m_info.bitsStored; }
    QString photometric() const;
    QString transferSyntax() const;
    QString modality() const { return QString::fromStdString(m_info.modality); }
    QString patientName() const { return QString::fromStdString(m_info.patientName); }
    QString patientId() const { return QString::fromStdString(m_info.patientId); }

    double valueMin() const { return m_valueRange.min; }
    double valueMax() const { return m_valueRange.max; }

    int viewRotation() const { return m_view.quarterTurns; }
    bool flipHorizontal() const { return m_view.flipHorizontal; }
    bool flipVertical() const { return m_view.flipVertical; }

    QString orientationLeft() const { return shownLabels().left; }
    QString orientationRight() const { return shownLabels().right; }
    QString orientationTop() const { return shownLabels().top; }
    QString orientationBottom() const { return shownLabels().bottom; }

    bool view3D() const { return m_view3D; }
    bool hasVolume() const { return m_volume != nullptr; }
    QVector3D volumeSize() const { return m_volumeSize; }
    QVector3D volumeSpacing() const { return m_volumeSpacing; }
    int volumeDownsample() const { return m_volumeDownsample; }
    double volumeValueMin() const { return m_volumeValueMin; }
    double volumeValueMax() const { return m_volumeValueMax; }
    // Latest heightmap of the displayed frame; null while the 3D view is not
    // shown, the file has a volume, or the mesh is still being built.
    std::shared_ptr<const dv::HeightmapMesh> heightmap() const { return m_mesh; }
    std::shared_ptr<const VolumeData> volumeData() const { return m_volume; }

    ErrorModel *errors() const { return m_errors; }
    MetadataFilterModel *metadata() const { return m_metadata; }
    // Backs the image://dicom provider (see main.cpp).
    std::shared_ptr<FrameStore> frameStore() const { return m_store; }
    // Backs the image://dicomraw provider: the packed frame of the GPU path.
    std::shared_ptr<FrameStore> gpuFrameStore() const { return m_gpuStore; }

    void setFrame(int frame);
    void setWindowCenter(double center);
    void setWindowWidth(double width);
    void setInvert(bool invert);
    void setColormap(Colormap colormap);
    void setFlipHorizontal(bool flip);
    void setFlipVertical(bool flip);
    // The GPU choice is refused while !gpuAvailable.
    void setRenderBackend(RenderBackend backend);
    void setView3D(bool shown);

    // Starts loading `url` (a local file, or a folder, which opens as a series)
    // in the background.
    Q_INVOKABLE void open(const QUrl &url);
    // Starts loading the DICOM files of a local folder as one series.
    Q_INVOKABLE void openSeries(const QUrl &folder);
    // The New action: back to the empty state with the sample errors.
    Q_INVOKABLE void reset();
    // Names: "default" (from the file), "minmax", "brain", "lung", "bone", "abdomen".
    Q_INVOKABLE void applyPreset(const QString &name);
    Q_INVOKABLE void resetWindow();
    Q_INVOKABLE void rotateRight();
    Q_INVOKABLE void rotateLeft();
    // Back to the image as stored: no rotation, no flip.
    Q_INVOKABLE void resetTransform();
    // From the scene: the GPU path cannot run (software scene graph, shader
    // failed to load). Forces the CPU backend and adds a one-time info entry.
    Q_INVOKABLE void reportGpuUnavailable(const QString &reason);
    // From the scene, on every frame swap: ends the GPU timing started by a change.
    Q_INVOKABLE void reportFrameSwapped();

signals:
    void hasImageChanged();
    void busyChanged();
    void fileChanged();
    void frameCountChanged();
    void frameChanged();
    void windowChanged();
    void invertChanged();
    void colormapChanged();
    void imageRevisionChanged();
    void lastRenderMsChanged();
    void renderBackendChanged();
    void gpuAvailableChanged();
    void gpuActiveChanged();
    void gpuImageRevisionChanged();
    void infoChanged();
    void view3DChanged();
    void volumeChanged();
    void heightmapChanged();
    void orientationChanged();
    void viewTransformChanged();
    // A new file replaced the view.
    void loaded();
    // A load or render failed; the error list holds the details.
    void errorOccurred();

private:
    struct LoadResult {
        quint64 generation{};
        QString path;
        std::shared_ptr<const dv::DicomFile> file; // null when the load failed
        dv::DicomError error;                      // set when `file` is null
        std::vector<dv::TagEntry> tags;
        bool hasTags{false};
        dv::Voi voi;
        QImage image; // first frame: the CPU image, or the packed frame when `gpu`
        bool gpu{false};
        bool gpuCapable{false};
        double renderMs{};
        // Series: every slice, in order (empty for a single file).
        std::vector<std::shared_ptr<const dv::DicomFile>> slices;
        std::shared_ptr<const VolumeData> volume; // null when the file has none
        QVector3D volumeSize;
        QVector3D volumeSpacing;
        int volumeDownsample{1};
        double volumeValueMin{};
        double volumeValueMax{1};
        std::vector<dv::DicomError> notices; // shown with the new file (volume failed, single-slice series)
    };
    struct MeshResult {
        quint64 fileSerial{};
        std::shared_ptr<const dv::HeightmapMesh> mesh; // null on failure
        dv::DicomError error;
    };
    // One displayed frame: its file and the frame index inside that file (a
    // series slice is frame 0 of its own file).
    struct FrameSource {
        std::shared_ptr<const dv::DicomFile> file;
        int frame{};
    };
    struct RenderResult {
        quint64 fileSerial{};
        int frame{};
        bool gpu{false};
        QImage image; // null when the render failed
        dv::DicomError error;
        double renderMs{};
    };

    void onLoadFinished(LoadResult result);
    void onRenderFinished(RenderResult result);
    void onMeshFinished(MeshResult result);
    FrameSource frameSource(int frame) const;
    static LoadResult loadFile(quint64 generation, const QString &path, bool useGpu);
    static LoadResult loadFolder(quint64 generation, const QString &folder, bool useGpu,
                                 const std::shared_ptr<std::atomic<bool>> &cancel);
    static void prepareFirstFrame(LoadResult &result, std::shared_ptr<const dv::DicomFile> file, bool useGpu);
    static void attachVolume(LoadResult &result, const dv::Volume &volume);
    static MeshResult buildMesh(const FrameSource &source, const dv::RenderParams &params, quint64 serial);
    void runLoad(std::function<LoadResult()> job);
    bool wantsMesh() const { return m_view3D && m_file && !m_volume; }
    void requestMesh();
    void startMesh();
    quint64 beginLoad(std::shared_ptr<std::atomic<bool>> &cancel);
    void setBusy(bool busy);
    void clearView(QImage next = {});
    void setWindow(dv::Voi voi);
    void emitViewChanged();
    void requestRender();
    void startRender();
    bool wantsGpu() const { return m_backend == Gpu && m_gpuAvailable && m_file && m_gpuCapable; }
    // A window, invert or colormap change: the CPU path re-renders, the GPU one
    // only has its uniforms moved.
    void displayChanged();
    void setShownGpu(bool shown);
    void publishGpuFrame(QImage packed, int frame, double prepareMs);
    void recordSample(double ms, RenderBackend backend);
    void startGpuSample(double baseMs);
    void showError(const dv::DicomError &error);
    void setViewTransform(dv::ViewTransform view);
    // Orientation labels of the four screen edges (stored ones moved by m_view).
    struct ShownLabels {
        QString left, right, top, bottom;
    };
    ShownLabels shownLabels() const;

    std::shared_ptr<FrameStore> m_store = std::make_shared<FrameStore>();
    std::shared_ptr<FrameStore> m_gpuStore = std::make_shared<FrameStore>();
    ErrorModel *m_errors;
    MetadataModel *m_metadataModel;
    MetadataFilterModel *m_metadata;

    std::shared_ptr<const dv::DicomFile> m_file;
    dv::ImageInfo m_info;
    dv::OrientationLabels m_orientation;
    dv::ValueRange m_valueRange;
    dv::ViewTransform m_view;
    QString m_filePath;
    QString m_fileName;
    int m_frame = 0;
    dv::Voi m_voi;
    dv::Voi m_defaultVoi;
    bool m_invert = false;
    Colormap m_colormap = Gray;
    int m_revision = 0;
    double m_lastRenderMs = 0;
    double m_averageRenderMs = 0;
    RenderBackend m_lastRenderBackend = Cpu;
    std::deque<double> m_samples; // last 10 render times of the chosen backend
    bool m_busy = false;

    RenderBackend m_backend = Cpu;
    bool m_gpuAvailable = true;
    bool m_gpuCapable = false;
    bool m_shownGpu = false;
    int m_gpuRevision = 0;
    int m_gpuFrame = -1; // frame held by m_gpuStore, -1 when none
    dv::GpuParams m_gpuParams;
    QElapsedTimer m_gpuTimer; // valid while a GPU change waits for its frame swap
    double m_gpuBaseMs = 0;   // texture preparation counted in that sample

    std::vector<std::shared_ptr<const dv::DicomFile>> m_slices; // series slices; m_file is the first
    std::shared_ptr<const VolumeData> m_volume;
    QVector3D m_volumeSize;
    QVector3D m_volumeSpacing;
    int m_volumeDownsample = 1;
    double m_volumeValueMin = 0;
    double m_volumeValueMax = 1;
    bool m_view3D = false;
    std::shared_ptr<const dv::HeightmapMesh> m_mesh;
    bool m_meshInFlight = false;
    bool m_meshPending = false;
    QFutureWatcher<MeshResult> m_meshWatcher;
    std::shared_ptr<std::atomic<bool>> m_loadCancel; // set to abandon the load in flight

    quint64 m_loadGeneration = 0; // bumped by open() and reset()
    quint64 m_fileSerial = 0;     // bumped whenever the displayed file changes
    bool m_renderInFlight = false;
    bool m_renderPending = false;
    QFutureWatcher<RenderResult> m_renderWatcher;
};
