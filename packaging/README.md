# Packaging and CI

Configure with Qt 6.12.0 on CMAKE_PREFIX_PATH and Ninja on PATH:

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --install build/release --prefix /absolute/staging --component Runtime
cpack --config build/release/CPackConfig.cmake -B dist
```

On this Mac use `release-local` for all three preset commands: the ignored
CMakeUserPresets.json selects the working Xcode SDK and Qt path. It writes to
build/release. CPack chooses DragNDrop on macOS (Applications link), ZIP on Windows,
and TGZ on Linux. The runtime component excludes tests, headers and static archives.
Windows commands run in an MSVC x64 developer shell with Ninja available.
On Linux run `packaging/linux/build-appimage.sh build/release dist` as well.
The script installs to a temporary AppDir, verifies both downloaded tool hashes,
and removes the staging directory on exit. curl, sha256sum, patchelf, Qt qmake and standard
Ubuntu utilities are required. The tar.gz includes bin/lib/qml/plugins and qt.conf;
extract the complete tree and run bin/appDicomViewer.

## Qt and runner assumptions

CI uses macos-latest arm64 with Apple clang, windows-latest MSVC 2022 x64,
and ubuntu-24.04 x86_64 with GCC 13 and glibc 2.39. C++23 mode is required for
std::expected; GCC 13's libstdc++ supports it. Linux artifacts target Ubuntu 24.04
or compatible newer systems, not older glibc systems. AppImage does not erase this
baseline. Ubuntu's libfuse2 package is named libfuse2t64 on 24.04. AppImage tools
use extract-and-run to avoid requiring a FUSE mount during packaging.

install-qt-action installs Qt 6.12.0 plus aqt module names `qtquick3d`,
`qtshadertools`, `qtquicktimeline`. QuickTimeline is recommended by Quick3D and
supports its animation dependencies. Concurrent, Quick/QuickControls2 (Material),
QuickDialogs2, Svg (needed for the splash icon), LinguistTools and Qt Test are in the default desktop archives;
no separate modules or QuickTest package are requested. The registered suite uses
Catch2 and QGuiApplication, not qmltestrunner. It tests geometry/texture data and
controller GPU fallback without creating a GPU scene graph, so no GPU skip or
QSG_RHI_BACKEND override is needed. QT_QPA_PLATFORM=offscreen is set in CI.

Module/architecture names were checked in the official repository Updates.xml:
- https://download.qt.io/online/qtsdkrepository/linux_x64/desktop/qt6_6120/qt6_6120/Updates.xml
- https://download.qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_6120/qt6_6120/Updates.xml
- https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_msvc2022_64/Updates.xml

Qt deployment scans the QML module, including Material implementation, Dialogs
backends and Quick3D.Helpers. Font, .qm, .qsb and volume snippets remain embedded
in the executable. The macOS minimum is read from Qt's supported minimum (14.4).
Licenses and source-based SBOMs ship in Contents/Resources/licenses on macOS,
and bin/licenses next to the executable elsewhere.

## Releases and supply chain

Push/PR builds for main and feat/** upload platform artifacts; tags v* run the
release job after all builds pass. After resolving the licensing TODO below,
update project() version, verify all three artifacts, then create and push a tag
such as `git tag v0.1.0 && git push origin v0.1.0`. Only the release job has
contents: write. Build jobs have contents: read. No custom secrets are needed.
DCMTK FetchContent is cached by OS/architecture and dependency/root CMake hashes.

Action tags were checked against upstream GitHub releases (and the releases/latest
API): checkout v7.0.1, cache v6.1.0, upload-artifact v7.0.2,
download-artifact v8.0.2, install-qt-action v4.5.0, msvc-dev-cmd v1.13.0,
action-gh-release v3.0.4. aqtinstall is pinned to 3.3.0. See docs/SOUP.md and
both docs/sbom JSON inventories. linuxdeploy 1-alpha-20251107-1 has an upstream
API SHA-256 digest; plugin-qt 1-alpha-20250213-1 has no published API digest,
so its downloaded asset was hashed locally and the hash is checked in the script.

## Signing and licensing TODOs

Packages are unsigned for distribution. macdeployqt signs ad-hoc; this is not a
Developer ID signature or notarization. A downloaded app may require Finder
right-click > Open, subject to macOS policy. Windows Authenticode and Linux package
signatures are also absent. Add managed signing/notarization before public delivery.

The project own code is licensed under GPL-3.0-only; the full text ships as LICENSE. Qt Quick 3D and Quick Timeline are
GPLv3/commercial, not LGPLv3. Other Qt LGPLv3 notices, source/relink guidance,
DCMTK bundled notices and Font Awesome OFL text are in THIRD_PARTY_NOTICES.md.
The SBOM remains source-based, not a complete transitive binary license audit.

Windows/Linux builds and the Actions run require CI verification; macOS local
package smoke-test evidence is recorded below after validation.

## Local verification (2026-11-16)

Clean Release build on Apple Silicon with the Xcode SDK: all 190 CTest tests
passed. Runtime installation and CPack DragNDrop succeeded. The DMG is
63,087,106 bytes (60.16 MiB), with the app and Applications link at its root.
Mounted read-only: codesign reported ad-hoc signing and deep/strict verification
passed. All 93 Mach-O files were inspected with otool -L (both architectures of
universal Qt libraries): no non-system absolute dependency paths, including no
/Users or SDK Qt paths. The bundle contains 46 Qt frameworks, Cocoa/offscreen,
imageformats including SVG, Material/impl, Dialogs/quickimpl, Quick3D/Helpers,
notices and both SBOMs. The existing font, translations, qsb and material snippets
are embedded by the app resource targets. Icons were inspected at small sizes.

From the mounted bundle, CT_small.dcm stayed open for 6 seconds with offscreen
and 4 seconds with Cocoa, then each process was terminated. Both used an otherwise
empty environment with only HOME and PATH=/usr/bin:/bin (plus QT_QPA_PLATFORM for
offscreen). No missing QML/plugin/shader/SVG/bundled-font errors occurred.
Offscreen emitted expected no-RHI/3D and opacity limitations and a generic
Sans Serif alias warning; Cocoa emitted a macOS sandbox-extension diagnostic.
Neither launch exited early. The image was detached and temporary trees removed.
YAML (PyYAML), JSON, inventory parity, shell syntax and git diff checks passed.
actionlint was not installed and no global tools were installed for validation.
The installed SDK also reports an optional Qt TaskTree discovery warning;
third-party DCMTK emits readdir_r deprecation warnings. Neither fails the build.
Windows, Linux, AppImage execution and GitHub Actions remain unexecuted locally.
