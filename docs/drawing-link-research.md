# External Drawing link research

Archicad can create a linked raster Drawing through its user interface. The public C++ DevKit for
Archicad 27–29 exposes `API_DrawingLinkInfo` only for querying an existing link through
`ACAPI_Drawing_GetDrawingLink`; `API_DrawingType` has no writable source-path member and no public
API creates or changes that link. GeoRaster therefore supports `Picture` only. An earlier static
Drawing experiment was removed because a Picture stored in temporary Drawing data produced an empty
Drawing frame.

The DevKit also offers `Import2DDrawingSupported` for a registered Add-On file type. It is an I/O
callback route, not a setter for a PNG/JPEG drawing link, and registering those extensions would
conflict with Archicad's built-in image importer. Any future experiment must use an isolated private
test file type and must not change the production GeoRaster registration.
