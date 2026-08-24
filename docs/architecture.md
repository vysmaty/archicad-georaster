# Architecture

GeoRaster is split at a hard API boundary:

```text
AddOnMain → command/dialog → src/compat → pinned Archicad DevKit
                         ↘ georaster_core (no Archicad API)
```

`georaster_core` parses World Files with the classic locale, reads PNG IHDR and JPEG SOF dimensions, applies half-pixel outer corners, computes Worksheet placement and inverts rigid 2D project-to-survey matrices. It has no GDAL, PROJ or image-library dependency.

The compatibility layer owns the user-defined GeoRaster menu, database operations, Survey Point access,
Undo scopes and `API_PictureType` creation. Worksheet creation is intentionally outside Undo; picture
creation is one Undo operation. On failure, the original database is restored before the empty
Worksheet is deleted.

The raw PNG/JPEG bytes are copied into `API_ElementMemo::pictHdl`. Picture defaults are obtained from Archicad, so the current layer and ordinary default settings are preserved.
