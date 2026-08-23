# Compatibility

| Archicad | DevKit | Toolset | Builds |
| --- | --- | --- | --- |
| 27 | 27.6003 | v142 | CZE Debug + Release |
| 28 | 28.4001 | v142 | CZE Debug + Release |
| 29 | 29.3100 | v143 | CZE Debug + Release, INT Debug |

`georaster.yaml` is authoritative. Shared core code is C++17. All version-sensitive Archicad calls belong in `src/compat/` and must be checked against all three pinned DevKits.

The updater cannot add a new major automatically. AC30 or later requires a reviewed manifest, preset, CI, compatibility and smoke-test change.
