# Agent guide for GeoRaster

Treat `georaster.yaml` as the source of truth. Support only Archicad 27, 28 and 29 on Windows x64. Do not enable distribution or replace development MDID `1/1` without an explicit reviewed decision.

Resolve Archicad API details from the exact pinned DevKit headers and official examples. Keep all `ACAPI_*` calls in `src/compat/` except registration entry points in `src/AddOnMain.cpp`. The `core/` library must remain C++17 and independent of Graphisoft headers.

Use `uv` only for development tools and structural tests; this repository is not a Python package. Do not commit DevKits, build output or `.apx` files. Keep `external/archicad-addon-cmake-tools` pinned and unmodified.

Before pushing, run Ruff, ty, pytest, CTest and all CZE builds for AC27–29 Debug/Release plus AC29 Debug INT.
