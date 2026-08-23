# Windows development setup

Install Visual Studio 2022 Desktop development with C++, MSVC v142/v143, a Windows SDK, Git and uv. Then:

```powershell
uv sync --all-groups --locked
.\tools\bootstrap.ps1 -Version 27
.\tools\bootstrap.ps1 -Version 28
.\tools\bootstrap.ps1 -Version 29
.\tools\build.ps1 -Version 29 -Configuration Debug -Language CZE
```

The build wrapper finds CMake on `PATH` or in Visual Studio Build Tools, builds the Add-On and runs CTest. DevKits remain in ignored `third_party/devkits/` storage.

Load the resulting `.apx` through Archicad's Add-On Manager only into the matching major. Local artifacts use MDID `1/1` and are not distributable.
