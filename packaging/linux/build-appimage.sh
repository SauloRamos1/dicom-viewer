# SPDX-License-Identifier: GPL-3.0-only
#!/usr/bin/env bash
# Run from the repository root after the Release build. x86_64 Linux only.
set -euo pipefail
build_dir="${1:-build/release}"
output_dir="${2:-dist}"
mkdir -p "$output_dir"
output_dir="$(cd "$output_dir" && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

download() {
    curl --fail --location --retry 3 "$1" -o "$2"
    echo "$3  $2" | sha256sum --check -
    chmod +x "$2"
}
# Release assets, not the mutable continuous tag. Update hashes with versions.
download https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage \
    "$work/linuxdeploy" c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
download https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage \
    "$work/linuxdeploy-plugin-qt" 15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

cmake --install "$build_dir" --prefix "$work/AppDir/usr" --component Runtime
export PATH="$work:$PATH"
QMAKE="$(command -v qmake6 || command -v qmake)"
export QMAKE
export QML_SOURCES_PATHS="$PWD/src/app/qml"
# Qt's install step already scans imports; let the plugin complete AppDir layout.
export APPIMAGE_EXTRACT_AND_RUN=1
version="$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\([^"]*\)")/\1/p' "$build_dir/CPackConfig.cmake")"
export OUTPUT="dicom-viewer-${version}-linux-x86_64.AppImage"
cd "$output_dir"
"$work/linuxdeploy" --appdir "$work/AppDir" \
    --executable "$work/AppDir/usr/bin/appDicomViewer" \
    --desktop-file "$work/AppDir/usr/share/applications/io.github.SauloRamos1.DicomViewer.desktop" \
    --icon-file "$work/AppDir/usr/share/icons/hicolor/256x256/apps/io.github.SauloRamos1.DicomViewer.png" \
    --plugin qt --output appimage
