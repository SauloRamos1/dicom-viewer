#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <vector>

#include "DicomFile.h"
#include "DicomImageProvider.h"
#include "DicomError.h"
#include "ErrorModel.h"
#include "MetadataModel.h"
#include "Metadata.h"
#include "Orientation.h"
#include "Voi.h"

// Application state behind the UI: the open file, what is displayed (frame,
// window, invert, colormap) and the models of the side panels. Files are loaded
// and frames rendered on worker threads; results come back through the event
// loop and are dropped when a newer open() or reset() made them obsolete.
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
    Q_PROPERTY(double lastRenderMs READ lastRenderMs NOTIFY lastRenderMsChanged)

    Q_PROPERTY(int rows READ rows NOTIFY infoChanged)
    Q_PROPERTY(int columns READ columns NOTIFY infoChanged)
    Q_PROPERTY(int bitsStored READ bitsStored NOTIFY infoChanged)
    Q_PROPERTY(QString photometric READ photometric NOTIFY infoChanged)
    Q_PROPERTY(QString transferSyntax READ transferSyntax NOTIFY infoChanged)
    Q_PROPERTY(QString modality READ modality NOTIFY infoChanged)
    Q_PROPERTY(QString patientName READ patientName NOTIFY infoChanged)
    Q_PROPERTY(QString patientId READ patientId NOTIFY infoChanged)

    Q_PROPERTY(QString orientationLeft READ orientationLeft NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationRight READ orientationRight NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationTop READ orientationTop NOTIFY orientationChanged)
    Q_PROPERTY(QString orientationBottom READ orientationBottom NOTIFY orientationChanged)

    Q_PROPERTY(ErrorModel *errors READ errors CONSTANT)
    Q_PROPERTY(MetadataFilterModel *metadata READ metadata CONSTANT)

public:
    // Same values as dv::Colormap.
    enum Colormap { Gray, Bone, Hot, Jet };
    Q_ENUM(Colormap)

    explicit DicomController(QObject *parent = nullptr);

    bool hasImage() const { return m_file != nullptr; }
    bool busy() const { return m_busy; }
    QString fileName() const { return m_fileName; }
    QString filePath() const { return m_filePath; }
    int frameCount() const { return m_file ? m_info.frameCount : 0; }
    int frame() const { return m_frame; }
    bool windowable() const { return m_file && m_info.isMonochrome(); }
    double windowCenter() const { return m_voi.center; }
    double windowWidth() const { return m_voi.width; }
    bool invert() const { return m_invert; }
    Colormap colormap() const { return m_colormap; }
    int imageRevision() const { return m_revision; }
    double lastRenderMs() const { return m_lastRenderMs; }

    int rows() const { return m_info.rows; }
    int columns() const { return m_info.columns; }
    int bitsStored() const { return m_info.bitsStored; }
    QString photometric() const;
    QString transferSyntax() const;
    QString modality() const { return QString::fromStdString(m_info.modality); }
    QString patientName() const { return QString::fromStdString(m_info.patientName); }
    QString patientId() const { return QString::fromStdString(m_info.patientId); }

    QString orientationLeft() const { return QString::fromStdString(m_orientation.left); }
    QString orientationRight() const { return QString::fromStdString(m_orientation.right); }
    QString orientationTop() const { return QString::fromStdString(m_orientation.top); }
    QString orientationBottom() const { return QString::fromStdString(m_orientation.bottom); }

    ErrorModel *errors() const { return m_errors; }
    MetadataFilterModel *metadata() const { return m_metadata; }
    // Backs the image://dicom provider (see main.cpp).
    std::shared_ptr<FrameStore> frameStore() const { return m_store; }

    void setFrame(int frame);
    void setWindowCenter(double center);
    void setWindowWidth(double width);
    void setInvert(bool invert);
    void setColormap(Colormap colormap);

    // Starts loading `url` (a local file) in the background.
    Q_INVOKABLE void open(const QUrl &url);
    // The New action: back to the empty state with the sample errors.
    Q_INVOKABLE void reset();
    // Names: "default" (from the file), "minmax", "brain", "lung", "bone", "abdomen".
    Q_INVOKABLE void applyPreset(const QString &name);
    Q_INVOKABLE void resetWindow();

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
    void infoChanged();
    void orientationChanged();
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
        QImage image; // first frame
        double renderMs{};
    };
    struct RenderResult {
        quint64 fileSerial{};
        QImage image; // null when the render failed
        dv::DicomError error;
        double renderMs{};
    };

    void onLoadFinished(LoadResult result);
    void onRenderFinished(RenderResult result);
    void setBusy(bool busy);
    void clearView(QImage next = {});
    void setWindow(dv::Voi voi);
    void emitViewChanged();
    void requestRender();
    void startRender();
    void showError(const dv::DicomError &error);

    std::shared_ptr<FrameStore> m_store = std::make_shared<FrameStore>();
    ErrorModel *m_errors;
    MetadataModel *m_metadataModel;
    MetadataFilterModel *m_metadata;

    std::shared_ptr<const dv::DicomFile> m_file;
    dv::ImageInfo m_info;
    dv::OrientationLabels m_orientation;
    QString m_filePath;
    QString m_fileName;
    int m_frame = 0;
    dv::Voi m_voi;
    dv::Voi m_defaultVoi;
    bool m_invert = false;
    Colormap m_colormap = Gray;
    int m_revision = 0;
    double m_lastRenderMs = 0;
    bool m_busy = false;

    quint64 m_loadGeneration = 0; // bumped by open() and reset()
    quint64 m_fileSerial = 0;     // bumped whenever the displayed file changes
    bool m_renderInFlight = false;
    bool m_renderPending = false;
    QFutureWatcher<RenderResult> m_renderWatcher;
};
