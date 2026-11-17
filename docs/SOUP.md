# Software of unknown provenance (SOUP)

This inventory supports an educational and technical-test DICOM viewer. It is not for diagnostic use and makes no regulatory claim. Maintain it alongside dependency, packaging and fixture changes; review upstream license and release information and rerun verification on each update. Runtime entries describe intended use, not an audited distribution.

Requirements below are functional expectations, not measured performance limits. No quantitative performance requirements or benchmarks were established in the inspected sources. Known anomalies should be checked in the upstream issue tracker and release notes linked below; no specific anomaly is asserted.

| ID | Name | Version | Source and anomaly pointer | License | Use and requirement | Verification / risk | Shipped |
|---|---|---|---|---|---|---|---|
| SOUP-001 | Qt Quick | 6.12.0 | [https://github.com/qt/qtdeclarative](https://github.com/qt/qtdeclarative) | LGPL-3.0-only | QML rendering; must function correctly for supported inputs | tests/app (QML UI itself not covered) | Yes |
| SOUP-002 | Qt Quick Controls 2 | 6.12.0 | [https://github.com/qt/qtdeclarative](https://github.com/qt/qtdeclarative) | LGPL-3.0-only | controls; must function correctly for supported inputs | tests/app (QML UI itself not covered) | Yes |
| SOUP-003 | Qt Quick 3D | 6.12.0 | [https://github.com/qt/qtquick3d](https://github.com/qt/qtquick3d) | LGPL-3.0-only | 3D UI; must function correctly for supported inputs | tests/app (QML UI itself not covered) | Yes |
| SOUP-004 | Qt Concurrent | 6.12.0 | [https://github.com/qt/qtbase](https://github.com/qt/qtbase) | LGPL-3.0-only | background execution; must function correctly for supported inputs | tests/app (QML UI itself not covered) | Yes |
| SOUP-005 | Qt Quick Dialogs | 6.12.0 | [https://github.com/qt/qtdeclarative](https://github.com/qt/qtdeclarative) | LGPL-3.0-only | file dialog; must function correctly for supported inputs | tests/app (QML UI itself not covered) | Yes |
| SOUP-006 | DCMTK | 3.7.1 | [https://github.com/DCMTK/dcmtk](https://github.com/DCMTK/dcmtk) | BSD-3-Clause | DICOM parsing, images and codecs; must function correctly for supported inputs | tests/unit and tests/bdd | Yes |
| SOUP-007 | IJG libjpeg 8/12/16 | NOASSERTION | [https://github.com/DCMTK/dcmtk](https://github.com/DCMTK/dcmtk) | IJG | JPEG 8/12/16-bit decoding; must function correctly for supported inputs | tests/unit and tests/bdd | Yes |
| SOUP-008 | CharLS (DCMTK bundled) | NOASSERTION | [https://github.com/DCMTK/dcmtk](https://github.com/DCMTK/dcmtk) | BSD-3-Clause | JPEG-LS decoding; must function correctly for supported inputs | tests/unit and tests/bdd | Yes |
| SOUP-009 | oficonv (Citrus) | NOASSERTION | [https://github.com/DCMTK/dcmtk](https://github.com/DCMTK/dcmtk) | NOASSERTION | character conversion; must function correctly for supported inputs | tests/unit and tests/bdd | Yes |
| SOUP-010 | Font Awesome Free Solid | 7.3.1 | [https://fontawesome.com](https://fontawesome.com) | OFL-1.1 | UI glyphs; must function correctly for supported inputs | test suite | Yes |
| SOUP-011 | Catch2 | 3.16.1 | [https://github.com/catchorg/Catch2](https://github.com/catchorg/Catch2) | BSL-1.0 | test framework; must function correctly for supported inputs | test suite | No |
| SOUP-012 | pydicom test data | NOASSERTION | [https://github.com/pydicom/pydicom](https://github.com/pydicom/pydicom) | MIT | DICOM test fixtures; must function correctly for supported inputs | tests/unit and tests/bdd | No |

Qt issue tracker: https://bugreports.qt.io/ and releases: https://www.qt.io/download. DCMTK: https://github.com/DCMTK/dcmtk/issues and https://github.com/DCMTK/dcmtk/releases. Catch2: https://github.com/catchorg/Catch2/issues. Font Awesome: https://github.com/FortAwesome/Font-Awesome/issues. Test data: https://github.com/pydicom/pydicom/issues.

DCMTK is pinned to `DCMTK-3.7.1` (fetched HEAD `108acdae424d6999b2e398243938d8c563315ecf`); Catch2 to `v3.16.1` (HEAD `08092139210881f01a34e60c5f2fd6afa0ea4024`). CMake can select an installed package instead. To update, change the tag in `cmake/Dependencies.cmake`, inspect the resulting source, licenses and tests, then update this inventory and both SBOMs. Qt requires at least 6.12; update the installed SDK and review its modules and deployment. Font and fixtures are vendored: replace deliberately, retain notices, recalculate hashes and verify tests.

The configured DCMTK modules are ofstd, oflog, oficonv, dcmdata, dcmimgle, dcmimage, dcmjpeg and dcmjpls. External TIFF, PNG, XML, ZLIB, OPENSSL, ICONV, WRAP, OPENJPEG and SNDFILE are disabled. Built-in JPEG and JPEG-LS are retained. Qt Test is not linked; QtQuick.Dialogs is imported by QML.

## License obligations

- Qt: under the LGPLv3 option, provide license text and required notices, corresponding library source and modifications as applicable, and a practical way to replace or relink the library. Dynamic deployment with Qt deployment tools can support this; actual packaging and relinking must be verified. GPLv3 and commercial options also exist, with module-specific terms.
- DCMTK and bundled CharLS: retain applicable BSD-3-Clause copyright, conditions and disclaimer in binary distribution materials. Review bundled IJG and oficonv notices separately.
- Font Awesome: the embedded `src/app/resources/fonts/fa-solid-900.otf` is OFL-1.1; include its copyright and license and respect reserved names. The upstream CC-BY-4.0 applies to SVG/JS icons; this project uses font glyphs, not identified upstream SVG/JS icon files. `app.svg` is not established as a Font Awesome asset.
- Catch2: BSL-1.0, test-only; retain its notice when distributing it.
- pydicom fixtures: MIT, test-only; retain the copyright and license in `tests/data/LICENSE.pydicom` when distributing them.

## Build tools

CMake, Ninja, Xcode/Apple clang and Qt LinguistTools are build or development tools, not intended runtime components. Actual tool versions and use of Ninja or Xcode were not established.

## Verification gaps / assumptions

No build, deployment, binary inspection, license compliance audit, or test execution was performed. Shipped status is inferred from CMake and source; deploy tooling and Qt dynamic linking are not demonstrated. Qt contains additional third-party dependencies and platform plugins not inventoried here. The exact versions of bundled IJG, CharLS and Citrus and the complete oficonv license expression need source-level review; static archive inclusion is not verified. Installed-package overrides may differ. The SBOM is an initial source-based inventory, not a complete transitive binary inventory. Confirm module-specific Qt licensing and commercial terms for the actual distribution. JPEG-LS patent notice in DCMTK's bundled license requires separate assessment.
