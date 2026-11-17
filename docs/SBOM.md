# Software bill of materials

The CycloneDX 1.6 and SPDX 2.3 JSON documents describe the same source-based component set, including runtime dependencies, test-only fixtures and build tools. SOUP identifiers are CycloneDX properties and SPDX package identifiers. The application is version 0.1.0; its license is unasserted. Runtime scope is intended shipping, not a verified deployment. Test data files have individual SHA-256 checksums; the font has a SHA-256 checksum. Build tool entries indicate possible tools, not proof of use.

To update manually, inspect CMake configuration and resolved package sources, installed Qt module licenses, bundled third-party notices and deployment output. Update versions, licenses, dependency relationships and SOUP IDs in both JSON documents and SOUP.md; recalculate file hashes with `shasum -a 256`. Generate fresh UUIDs and a current UTC timestamp, then validate both formats against their official schemas and compare inventories. Tools such as cdxgen or syft may assist; neither was run for this inventory.

Validation performed: Python JSON syntax checks on both files. Schema validation, dependency completeness, actual package contents, and license compliance were not verified. See SOUP.md for assumptions and gaps.
