# Test data

Small DICOM files used by the unit and BDD tests. They come unmodified from the
[pydicom](https://github.com/pydicom/pydicom) test data
(`src/pydicom/data/test_files/`), which is released under the MIT license
(full text of the project license in `LICENSE.pydicom`).

| File | Size | Role |
|---|---|---|
| `CT_small.dcm` | 39 KB | CT, 128x128, 16-bit signed, MONOCHROME2, slope 1 / intercept -1024 (HU), no window tags: HU readout, MinMax window, windowing |
| `MR_small.dcm` | 10 KB | MR, 64x64, 16-bit signed, MONOCHROME2, Explicit VR Little Endian, window 600/1600: reference image, metadata, MONOCHROME1 base (rewritten at runtime) |
| `MR_small_bigendian.dcm` | 10 KB | Same image, Explicit VR Big Endian |
| `MR_small_RLE.dcm` | 8 KB | Same image, RLE Lossless |
| `MR_small_jpeg_ls_lossless.dcm` | 6 KB | Same image, JPEG-LS Lossless |
| `JPEG-lossy.dcm` | 10 KB | NM, 1024x256, 12 bits stored, JPEG Extended (Process 2+4) |
| `SC_rgb_small_odd.dcm` | 1 KB | RGB 3x3 uncompressed: known pixel values |
| `SC_rgb_jpeg_dcmtk.dcm` | 3 KB | RGB 100x100, JPEG Baseline |
| `SC_rgb_rle_2frame.dcm` | 3 KB | RGB 100x100, two frames, RLE Lossless |
| `SC_ybr_full_422_uncompressed.dcm` | 22 KB | YBR_FULL_422 uncompressed, 100x100 (converted to RGB) |
| `JPEG2000.dcm` | 3 KB | JPEG 2000: must fail with `UnsupportedTransferSyntax` (not supported by this build) |
| `image_dfl.dcm` | 5 KB | Deflated Explicit VR Little Endian: must fail with `UnsupportedTransferSyntax` (DCMTK is built without zlib) |

Fixtures that are invalid or variants of the files above (empty file, random
bytes, truncated file, missing Pixel Data, MONOCHROME1, multi-frame CT,
multi-slice series) are generated at runtime in a temporary directory by the
tests, so no extra binary files are committed.

## License

The files above are copied from pydicom, which is distributed under the
following license (the same text is in `LICENSE.pydicom`; pydicom's license
lists third-party exceptions that apply to its source code, not to these files).

```
Copyright (c) 2008-2020 Darcy Mason and pydicom contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```
