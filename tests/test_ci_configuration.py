import json
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[1]


def test_cpp_build_uses_visual_studio_2022_runner() -> None:
    workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")

    assert "runs-on: windows-2022" in workflow
    assert "runs-on: windows-latest" not in workflow


def test_resource_compiler_receives_a_non_empty_lp_xml_path() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")

    assert '"${AC_API_DEVKIT_DIR}/LP_XMLConverter"' in cmake
    assert (ROOT / "RFIX.win" / "AddOnMain.rc2").is_file()
    assert (ROOT / "src" / "APIEnvir.h").is_file()


def test_ci_runs_python_quality_and_all_seven_cpp_builds() -> None:
    workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")

    assert "uv run ruff check ." in workflow
    assert "uv run ty check" in workflow
    assert "uv run pytest" in workflow
    assert workflow.count("configuration: Debug") == 4
    assert workflow.count("configuration: Release") == 3
    assert workflow.count("language: CZE") == 6
    assert workflow.count("language: INT") == 1
    assert "GeoRaster.apx" in workflow


def test_manifest_keeps_distribution_off_and_languages_explicit() -> None:
    manifest = (ROOT / "georaster.yaml").read_text(encoding="utf-8")
    config = (ROOT / "config.json").read_text(encoding="utf-8")

    assert "distribution: false" in manifest
    assert "languages: [CZE, INT]" in manifest
    assert '"defaultLanguage": "CZE"' in config
    assert '"languages": ["CZE", "INT"]' in config


def test_devkit_pins_and_cmake_presets_cover_exact_supported_majors() -> None:
    manifest = yaml.safe_load((ROOT / "georaster.yaml").read_text(encoding="utf-8"))
    presets = json.loads((ROOT / "CMakePresets.json").read_text(encoding="utf-8"))

    assert manifest["project"]["supported_archicad"] == [27, 28, 29]
    assert {version: item["release"] for version, item in manifest["devkits"].items()} == {
        27: "27.6003",
        28: "28.4001",
        29: "29.3100",
    }
    preset_names = {preset["name"] for preset in presets["buildPresets"]}
    assert preset_names == {
        "ac27-debug",
        "ac27-release",
        "ac28-debug",
        "ac28-release",
        "ac29-debug",
        "ac29-release",
    }


def test_local_build_identity_and_python_non_package_mode_are_locked() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    pyproject = (ROOT / "pyproject.toml").read_text(encoding="utf-8")
    resources = "\n".join(
        (ROOT / language / "AddOn.grc").read_text(encoding="utf-8") for language in ("RCZE", "RINT")
    )

    assert "AC_ADDON_FOR_DISTRIBUTION OFF" in cmake
    assert resources.count("'MDID' 32500") == 2
    assert resources.count("@GEORASTER_DEVELOPER_ID@") == 2
    assert resources.count("@GEORASTER_LOCAL_ID@") == 2
    assert "GRAPHISOFT_DEVELOPER_ID and GRAPHISOFT_LOCAL_ID must either both be set" in cmake
    assert "GEORASTER_GENERATED_RESOURCES_FOLDER" in cmake
    assert "package = false" in pyproject
    assert "[build-system]" not in pyproject


def test_ci_passes_mdid_secrets_only_to_the_build_step() -> None:
    workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")

    assert "GRAPHISOFT_DEVELOPER_ID: ${{ secrets.GRAPHISOFT_DEVELOPER_ID }}" in workflow
    assert "GRAPHISOFT_LOCAL_ID: ${{ secrets.GRAPHISOFT_LOCAL_ID }}" in workflow
    assert "pull_request_target:" not in workflow


def test_scheduled_updates_only_open_a_review_pr() -> None:
    workflow = (ROOT / ".github" / "workflows" / "upstream-updates.yml").read_text(encoding="utf-8")

    assert "schedule:" in workflow
    assert "tools/check_updates.py --apply" in workflow
    assert "create-pull-request" in workflow
    assert "gh pr merge" not in workflow.lower()
