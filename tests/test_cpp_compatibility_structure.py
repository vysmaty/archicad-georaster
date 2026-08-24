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


def test_picture_defaults_are_read_in_the_active_target_database() -> None:
    command = (ROOT / "src" / "GeoRasterCommand.cpp").read_text(encoding="utf-8")
    compatibility = (ROOT / "src" / "compat" / "ArchicadCompatibility.cpp").read_text(
        encoding="utf-8"
    )

    create_picture = compatibility[compatibility.index("GSErrCode CreatePicture(") :]
    assert "ACAPI_Element_GetDefaults(&element, nullptr)" in create_picture
    assert "GetPictureDefaults" not in command


def test_worksheet_failures_report_the_exact_api_stage() -> None:
    command = (ROOT / "src" / "GeoRasterCommand.cpp").read_text(encoding="utf-8")

    for stage in (
        "CreateWorksheet",
        "ActivateWorksheet",
        "CallUndoable",
        "CreatePicture",
    ):
        assert f'errorStage = "{stage}"' in command


def test_menu_is_a_user_defined_menu_and_picture_is_the_only_output() -> None:
    command = (ROOT / "src" / "GeoRasterCommand.cpp").read_text(encoding="utf-8")
    compatibility = (ROOT / "src" / "compat" / "ArchicadCompatibility.cpp").read_text(
        encoding="utf-8"
    )
    dialog = (ROOT / "src" / "ui" / "ImportDialog.cpp").read_text(encoding="utf-8")

    assert "MenuCode_UserDef" in compatibility
    assert "MenuCode_Tools" not in compatibility
    assert "ImportToExistingWorksheet" in command
    assert "APIWind_WorksheetID" in command
    assert "StaticDrawing" not in command
    assert "CreateStaticDrawing" not in compatibility
    assert "ACAPI_Drawing_StartDrawingData" not in compatibility
    assert "Drawing (" not in dialog


def test_worksheet_placement_uses_absolute_world_anchor() -> None:
    command = (ROOT / "src" / "GeoRasterCommand.cpp").read_text(encoding="utf-8")

    assert "placement.anchor, placement.width, placement.height" in command
    assert "localBounds" not in command


def test_bad_index_error_is_named_for_user_diagnostics() -> None:
    compatibility = (ROOT / "src" / "compat" / "ArchicadCompatibility.cpp").read_text(
        encoding="utf-8"
    )

    assert "APIERR_BADINDEX" in compatibility


def test_new_worksheet_is_activated_through_the_window_api() -> None:
    compatibility = (ROOT / "src" / "compat" / "ArchicadCompatibility.cpp").read_text(
        encoding="utf-8"
    )

    assert "ACAPI_Window_GetCurrentWindow" in compatibility
    assert "ACAPI_Window_ChangeWindow" in compatibility
    assert "ACAPI_Database_ChangeCurrentDatabase" not in compatibility


def test_project_working_units_stay_behind_the_compatibility_boundary() -> None:
    compatibility = (ROOT / "src" / "compat" / "ArchicadCompatibility.cpp").read_text(
        encoding="utf-8"
    )

    assert "ACAPI_ProjectSetting_GetPreferences" in compatibility
    assert "APIPrefs_WorkingUnitsID" in compatibility
    assert "ACAPI_Conversion_GetConvertedUnitValue" in compatibility
