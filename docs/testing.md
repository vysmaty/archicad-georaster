# Testing

Repository checks:

```powershell
uv sync --all-groups --locked
uv run ruff format --check .
uv run ruff check .
uv run ty check tools tests
uv run pytest
```

Each `tools/build.ps1` invocation builds `GeoRaster.apx` and runs the framework-free CTest suite. CI covers AC27–29 Debug/Release in CZE and AC29 Debug in INT.

Manual smoke testing in each Archicad major must verify: CZE dialog loading; the same PNG and JPEG in a new Worksheet; Floor Plan placement under a known Survey Point translation and rotation; 10 km blocking; and Undo removal of the inserted Picture.
