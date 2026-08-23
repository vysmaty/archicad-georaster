# Feedback for archicad-dev template

GeoRaster was bootstrapped without Git history from `vysmaty/archicad-dev` commit `b2a757d85a0391e1ca4d53bcbc85a2f72cde6612`.

Generally reusable findings to review later in the template repository:

- let `tools/build.ps1` discover Visual Studio's bundled CMake when `cmake` is not on `PATH`;
- make language an explicit build/CI matrix dimension;
- allow a non-package `uv` mode for C++-only products that retain Python tooling;
- keep product-neutral core libraries outside the source directory globbed by Graphisoft's Add-On generator, otherwise they are compiled twice with different language settings;
- keep distribution mode and MDID development placeholders protected by structural tests;
- make optional Python/Tapir layers removable without rewriting DevKit and update tooling.

No change is applied back to `archicad-dev` from this repository. That review is a separate follow-up.
