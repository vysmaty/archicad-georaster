from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_addon_entrypoint_delegates_version_sensitive_api_calls() -> None:
    entrypoint = (ROOT / "src" / "AddOnMain.cpp").read_text(encoding="utf-8")

    assert '#include "compat/ArchicadCompatibility.hpp"' in entrypoint
    assert "ACCompat::RegisterMenu" in entrypoint
    assert "ACCompat::InstallMenuHandler" in entrypoint
    assert "ServerMainVers_" not in entrypoint


def test_archicad_calls_stay_behind_compatibility_boundary() -> None:
    allowed = {
        ROOT / "src" / "AddOnMain.cpp",
        ROOT / "src" / "compat" / "ArchicadCompatibility.cpp",
        ROOT / "src" / "compat" / "ArchicadCompatibility.hpp",
    }
    offenders = []
    for source in (ROOT / "src").rglob("*.cpp"):
        if source not in allowed and "ACAPI_" in source.read_text(encoding="utf-8"):
            offenders.append(source.relative_to(ROOT).as_posix())

    assert offenders == []


def test_core_has_no_archicad_dependency() -> None:
    core_text = "\n".join(path.read_text(encoding="utf-8") for path in (ROOT / "core").glob("*"))

    assert "ACAPI_" not in core_text
    assert "API_Element" not in core_text
