# DICOM Viewer - Plan

Tech test: C++/Qt app to select a DICOM file, load it and display it correctly
(dimensions, grayscale, orientation), with error messages for invalid files.
Optional extras: Window Level/Width, metadata, RGB. Deadline: 3 days.

## 1. Decisions

| Topic | Decision |
|---|---|
| Qt | 6.12.0, Qt Quick + Controls **Material** style, Quick3D |
| C++ | **C++23**, errors via `std::expected<T, DicomError>`; no exceptions cross into QML |
| DICOM lib | **DCMTK 3.7.1** (`DicomImage` handles Modality LUT, VOI, MONOCHROME1/2, RGB/YBR). Codecs: JPEG, JPEG-LS, RLE. JPEG2000 and Deflated (DCMTK built without zlib) = documented limitation (clear `UnsupportedTransferSyntax` error) |
| Dependencies | **CMake + Qt only**, no package manager. DCMTK 3.7.1 and Catch2 v3 via `FetchContent` pinned by tag, with `FIND_PACKAGE_ARGS` (uses an installed copy if found, else downloads and builds it). DCMTK built static, apps/docs off. Requires CMake >= 3.24, Qt from official installer |
| Tests | **Catch2 v3**: unit tests + BDD (`tests/features/*.feature` as spec, steps as `SCENARIO/GIVEN/WHEN/THEN`), run by `ctest` |
| Test data | ~5 small pydicom files (MIT) committed in `tests/data/` with source license |
| Icons | **Font Awesome 7 Free** (Solid, OFL-1.1) bundled as font resource |
| Theme | Light/Dark toggle; first run follows system; persisted (`QSettings`) |
| i18n | UI strings in EN with `qsTr()`; **PT-BR** translation (`.ts` -> `.qm` via `qt_add_translations`); language menu in toolbar, runtime switch, persisted |
| README | `README.md` (EN) + `README.pt-BR.md` |
| Dev loop | `QmlPreviewMcpServer` (Qt 6.12 bin) for QML hot reload; Debug builds define `QT_QML_DEBUG` |
| macOS toolchain | Use Xcode toolchain/SDK (`/usr/bin/clang++` + CommandLineTools SDK 27 fails to link on this machine). Machine-specific values (Qt path, `CMAKE_OSX_SYSROOT`) live in a gitignored `CMakeUserPresets.json`; committed `CMakePresets.json` is generic (`debug`, `release`). Local build: `cmake --preset debug-local && cmake --build --preset debug-local && ctest --preset debug-local` |

## 2. UI

Mockup approved (Excalidraw). Main window, top to bottom:

**Toolbar** (icon-only buttons, each with `ToolTip` + shortcut):

| Button | FA icon | Action | Shortcut |
|---|---|---|---|
| New | `file` | Reset viewer: close file, clear image/metadata/options, errors back to sample preview | Ctrl+N |
| Open | `folder-open` | `FileDialog` (`*.dcm`, all files) -> load | Ctrl+O |
| Options | `sliders` | Show/hide floating Options pane | Ctrl+, |
| Metadata | `circle-info` | Toggle left panel (checkable) | Ctrl+I |
| 3D View | `cube` | Toggle viewport 2D <-> 3D (checkable) | Ctrl+3 |
| Errors | `triangle-exclamation` + count badge | Toggle right panel (checkable) | Ctrl+E |
| *(right)* Language | `language` | Menu: English / Português (BR) | - |
| *(right)* Theme | `moon` / `sun` | Light <-> Dark | Ctrl+T |

**Body** = horizontal `SplitView`:
- **Left - Metadata** (collapsible): filter field + tag table grouped (Patient, Study/Series, Image). Columns: tag, name, VR, value.
- **Center - Viewport**: 2D image or 3D scene. Overlays: patient/modality (top-left), orientation letters (from Image Orientation Patient), W/L + zoom + frame (bottom-left), pixel value / HU under cursor (bottom-right). Mouse: wheel zoom, drag pan, right-drag W/L, double-click fit.
- **Right - Errors** (toggle): list of entries (severity icon, message, tag, time). At startup shows sample errors flagged "preview". On failed load: cleared and filled with the real errors and the panel opens automatically. On successful load: cleared.

**Options** = floating `Pane`, top-right of viewport, draggable by its header, closable:
- W/L presets (Default from file, Min-Max, CT Brain/Lung/Bone/Abdomen), Window Width + Level sliders
- Invert, Colormap (Gray, Bone, Hot, Jet), Smooth interpolation
- Zoom + Fit, Rotate 90, Flip H/V
- Frame/slice slider (multi-frame)
- **Render backend: CPU / GPU** toggle + last render time (ms) for comparison
- 3D: Depth scale (heightmap), Threshold, Opacity, Sample steps (volume)
- Reset

**Status bar**: file name, size, bits, photometric, frames, transfer syntax.

**Splash screen**: frameless QML window, logo, app + Qt + DCMTK versions, progress bar with real init steps (register codecs, load settings, load translations, create Main), "Not for diagnostic use", minimum 1.5 s, fades into Main.

## 3. Architecture

```
src/
  core/            pure C++23 + DCMTK, no Qt GUI (unit-testable)
    DicomError.h       enum ErrorCode, Severity, message, optional tag
    DicomFile.*        load(path) -> expected<DicomFile, DicomError>; codec registration (once)
    PixelData.cpp      DicomFile::rawModalityValues / valueAt (HU)
    Windowing.*        VOI presets, applyWindow, renderFrame (DCMTK DicomImage) + invert
    Colormap.cpp       Gray/Bone/Hot/Jet LUTs
    Metadata.*         flat list of tags (group, element, name, VR, value) via DcmObject walk
    Orientation.*      orientation labels from (0020,0037)
    Volume.*           multi-frame or series -> 3D scalar volume (sorted by Image Position)
  app/             Qt glue
    DicomController    QML_SINGLETON (default-constructed; main.cpp adds its provider): open/reset,
                       state, W/L props, frame, invert, colormap, render time; owns the models
    DicomImageProvider QQuickImageProvider for CPU path, reads the controller's FrameStore
    MetadataModel      QAbstractListModel (rows ordered by category) + MetadataFilterModel (QSortFilterProxyModel)
    ErrorModel         QAbstractListModel (preview vs real); ErrorText maps ErrorCode -> translated text
    AppSettings        theme, last dir (QSettings); LanguageManager: language
    LanguageManager    QTranslator swap + engine.retranslate()
    HeightmapGeometry  QQuick3DGeometry from current frame
    VolumeTexture      QQuick3DTextureData (3D texture) for ray-march material
  app/qml/         (inside src/app so Qt Creator lists the files under the target)
  Main.qml, Splash.qml, TopBar.qml, View2D.qml (ViewerPane.qml arrives with View3D.qml),
  View3D.qml, OptionsPane.qml, MetadataPanel.qml, ErrorsPanel.qml, StatusBar.qml, Icon.qml
  shaders/ windowing.frag (GPU path), volume.frag (ray-march)
  app/resources/fonts/fa-solid-900.otf, app/resources/icons/app.svg
i18n/dicomviewer_pt_BR.ts
tests/
  unit/*.cpp, bdd/*.cpp, features/*.feature, data/*.dcm
```

Render paths (same input, comparable output):
- **CPU**: `Windowing` -> 8-bit `QImage` (Grayscale8 / RGB888) -> image provider.
- **GPU**: raw 16-bit values uploaded once as texture; `ShaderEffect` applies slope/intercept, W/L, invert, colormap.
- Render time measured with `QElapsedTimer` (CPU) and frame-swap timing (GPU), shown in Options.

3D:
- **Single frame**: heightmap mesh, height = windowed intensity * depth scale, colormap as vertex color.
- **Multi-frame / series**: 3D texture + ray-march custom material with threshold/opacity.

## 4. Error handling

`ErrorCode`: FileNotFound, FileUnreadable, NotDicom, NoPixelData, UnsupportedTransferSyntax, UnsupportedPhotometric, CorruptPixelData, DecodeFailed, VolumeInconsistent, FrameOutOfRange. Each maps to a translated user message plus a technical detail (DCMTK `OFCondition` text). Load runs off the GUI thread (`QtConcurrent`), and the UI shows a busy indicator. A failed load leaves the previous image, metadata and window untouched; the one exception is a valid file without Pixel Data, which shows its metadata (no image) plus the error. Starting a new load, or New, makes any load in flight obsolete (generation counter). Rendering also runs on a worker; changes made while a render runs are coalesced into one more render with the latest values.

## 5. Tests

Unit (Catch2):
- load valid CT/MR -> dims, bits, photometric, frames correct
- not-a-DICOM / empty / truncated / missing file -> correct `ErrorCode`
- no pixel data -> NoPixelData
- J2K -> UnsupportedTransferSyntax
- MONOCHROME1 rendered inverted vs MONOCHROME2
- RGB file -> 3 channels, known pixel
- windowing math: W/L bounds, clamp, invert, preset values
- slope/intercept -> HU at known pixel
- metadata contains expected tags/values
- orientation labels for axial/sagittal/coronal vectors

BDD (`.feature` + Catch2 SCENARIO):
- Open valid monochrome file -> image shown with file dimensions
- Open invalid file -> errors list replaced with real error, previous image kept
- New -> viewer reset, sample errors back
- Change W/L -> output changes as expected
- RGB file -> displayed in color
- Switch language -> strings translated

QML: `qmltestrunner` smoke tests (TopBar tooltips present, toggles flip panels).

## 6. Delivery steps (each committed after review/verification)

1. Scaffold: CMake + FetchContent (DCMTK, Catch2) + Material + FA font + i18n wiring + Splash; build green.
2. Core: DicomFile/PixelData/Windowing/Metadata/Orientation + unit tests (TDD).
3. App glue: controller, models, image provider (CPU), error flow + BDD tests, plus the first UI flow (Open dialog, drop, command line file, 2D view with zoom/pan/W-L mouse, metadata and errors panels, status bar). Adds Qt6::Concurrent; the Open dialog is `QtQuick.Dialogs` (no extra CMake component needed). `app/` tests compile the Qt glue sources again (same files as the app, no QML module) into `dicomapp_tests`.
4. UI: TopBar, SplitView panels, Options pane, status bar, themes, language menu.
5. GPU render path + timing toggle.
6. 3D: heightmap, then volume for multi-frame.
7. Packaging + CI (DMG / ZIP / AppImage, GitHub Release).
8. README EN + PT-BR, screenshots, final review (cpp-reviewer, qml-review), `/simplify`.

Agents: Sonnet for implementation/tests per step, Haiku for mechanical work (font download, sample data, translations), Opus only for review of shaders/threading.

## 7. Open items

- Git remote (GitHub repo name, public?) - needed for delivery.

## 8. Packaging and CI

- `qt_generate_deploy_qml_app_script` + `install(SCRIPT ...)`: `cmake --install` runs macdeployqt (macOS), windeployqt (Windows) and Qt's own Linux deploy.
- CPack: DMG (macOS), ZIP (Windows). Linux: AppImage via linuxdeploy + linuxdeploy-plugin-qt.
- GitHub Actions matrix (macos-latest, windows-latest, ubuntu-22.04): install Qt 6.12 (aqtinstall), configure, build, `ctest`, package, upload artifacts; on tag `v*` attach packages to a GitHub Release.
- README links the Release downloads and the build-from-source steps.

## 9. Series

In scope: Open button has a menu with "Open file..." and "Open folder (series)...". A folder is loaded as a series: slices filtered by Series Instance UID, sorted by Image Position (Patient) along the slice normal (fallback: Instance Number). The 2D view navigates slices with the frame slider; 3D uses the volume.
