# Software bill of materials

Project licence: GPL-3.0-only. Qt Quick 3D is available under GPL-3.0-only or a commercial licence; Qt LGPL modules are used in a GPLv3 combined work.

The CycloneDX 1.6 and SPDX 2.3 JSON documents describe the same source-based component set, including runtime dependencies, test-only fixtures and build tools. SOUP identifiers are CycloneDX properties and SPDX package identifiers. The application is version 0.1.0; its license is GPL-3.0-only. Runtime scope is intended shipping, not a verified deployment. Test data files have individual SHA-256 checksums; the font has a SHA-256 checksum. Build tool entries indicate possible tools, not proof of use.

To update manually, inspect CMake configuration and resolved package sources, installed Qt module licenses, bundled third-party notices and deployment output. Update versions, licenses, dependency relationships and SOUP IDs in both JSON documents and SOUP.md; recalculate file hashes with `shasum -a 256`. Generate fresh UUIDs and a current UTC timestamp, then validate both formats against their official schemas and compare inventories. Tools such as cdxgen or syft may assist; neither was run for this inventory.

Validation performed: Python JSON syntax checks on both files. Schema validation, dependency completeness, actual package contents, and license compliance were not verified. See SOUP.md for assumptions and gaps.

## Packaging build tools

These tools do not ship as application runtime dependencies. Action versions were
confirmed on upstream GitHub release pages/API. AppImage assets have pinned SHA-256
hashes (linuxdeploy upstream API digest; Qt plugin downloaded and hashed locally).

| ID | Tool | Version | License | Use |
|---|---|---|---|---|
| BUILD-005 | [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) | 1-alpha-20251107-1 | NOASSERTION | CI/build only |
| BUILD-006 | [linuxdeploy-plugin-qt](https://github.com/linuxdeploy/linuxdeploy-plugin-qt) | 1-alpha-20250213-1 | NOASSERTION | CI/build only |
| BUILD-007 | [install-qt-action](https://github.com/jurplel/install-qt-action) | 4.5.0 | NOASSERTION | CI/build only |
| BUILD-008 | [aqtinstall](https://github.com/miurahr/aqtinstall) | 3.3.0 | NOASSERTION | CI/build only |
| BUILD-009 | [actions/checkout](https://github.com/actions/checkout) | 7.0.1 | NOASSERTION | CI/build only |
| BUILD-010 | [actions/cache](https://github.com/actions/cache) | 6.1.0 | NOASSERTION | CI/build only |
| BUILD-011 | [actions/upload-artifact](https://github.com/actions/upload-artifact) | 7.0.2 | NOASSERTION | CI/build only |
| BUILD-012 | [actions/download-artifact](https://github.com/actions/download-artifact) | 8.0.2 | NOASSERTION | CI/build only |
| BUILD-013 | [action-gh-release](https://github.com/softprops/action-gh-release) | 3.0.4 | NOASSERTION | CI/build only |
| BUILD-015 | [patchelf](https://github.com/NixOS/patchelf) | NOASSERTION | NOASSERTION | Linux RPATH deployment |
| BUILD-014 | [msvc-dev-cmd](https://github.com/ilammy/msvc-dev-cmd) | 1.13.0 | NOASSERTION | CI/build only |

Qt Quick 3D was incorrectly listed as LGPL in the original inventory. It is
GPL-3.0-only or commercial, as documented at https://doc.qt.io/qt-6/qtquick3d-index.html.
The application is licensed under GPL-3.0-only; the full text is included in LICENSE. See packaging/README.md for verification.

Qt Svg (SOUP-013, 6.12.0, LGPL-3.0-only) is explicitly linked so the deploy
tools retain the existing splash SVG decoder and its Qt image plugin.
