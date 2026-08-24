# External Drawing link research

Archicad can create a linked raster Drawing through its user interface. The public C++ DevKit
exposes `API_DrawingLinkInfo` only for querying an existing link; `API_DrawingType` has no writable
source-path member. GeoRaster therefore creates a static Drawing from temporary drawing data in this
release.

The DevKit also offers `Import2DDrawingSupported` for a registered Add-On file type. It is an I/O
callback route, not a setter for a PNG/JPEG drawing link, and registering those extensions would
conflict with Archicad's built-in image importer. Any future experiment must use an isolated private
test file type and must not change the production GeoRaster registration.
