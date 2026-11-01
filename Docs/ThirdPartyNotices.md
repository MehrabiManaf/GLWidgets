# Third party notices

GLWidgets is MIT licensed (see [LICENSE](../LICENSE)). It is built with the libraries below, all compiled into it. An
application that ships GLWidgets ships them too, and must include these notices and the licenses they refer to.

| Library | Version | License | Source |
|---|---|---|---|
| SDL3 | 3.4.18 | zlib | https://github.com/libsdl-org/SDL |
| FreeType | 2.15.0 | FreeType License (FTL) | https://freetype.org |
| HarfBuzz | 14.6.0 | MIT ("Old MIT") | https://github.com/harfbuzz/harfbuzz |
| SheenBidi | 3.0.0 | Apache License 2.0 | https://github.com/Tehreer/SheenBidi |
| libunibreak | 8.0 | zlib | https://github.com/adah1972/libunibreak |
| Vazirmatn (font) | 33.003 | SIL Open Font License 1.1 | https://github.com/rastikerdar/vazirmatn |

The license texts are in each library's sources (fetched into the build directory, `_deps/<name>-src`), and the font's in
[Library/Fonts/Vazirmatn-OFL.txt](../Library/Fonts/Vazirmatn-OFL.txt).

## FreeType

The FreeType License asks for this credit in the documentation of software that uses it:

> Portions of this software are copyright © 2026 The FreeType Project (https://freetype.org). All rights reserved.

## Vazirmatn

The font is embedded unchanged in the library, as the default font. Under the SIL Open Font License it may be bundled
with software, but not sold by itself, and a modified version may not be called Vazirmatn.
