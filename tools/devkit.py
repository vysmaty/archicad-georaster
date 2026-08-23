# /// script
# requires-python = ">=3.11"
# dependencies = ["pyyaml==6.0.2"]
# ///
"""Install, locate, and validate pinned Archicad API Development Kits."""

from __future__ import annotations

import argparse
import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path
from typing import Any

import yaml

ROOT = Path(__file__).resolve().parents[1]
CONFIG_PATH = ROOT / "georaster.yaml"
DEVKIT_ROOT = ROOT / "third_party" / "devkits"


def load_config() -> dict[str, Any]:
    """Read and minimally validate the repository development manifest."""
    with CONFIG_PATH.open(encoding="utf-8") as config_file:
        config = yaml.safe_load(config_file)
    if not isinstance(config, dict) or not isinstance(config.get("devkits"), dict):
        raise TypeError(f"{CONFIG_PATH} must contain a devkits mapping")
    return config


def version_config(version: str) -> dict[str, str]:
    config = load_config()
    try:
        item = config["devkits"][int(version)]
    except (KeyError, ValueError) as error:
        raise ValueError(f"Unsupported Archicad version: {version}") from error
    required = ("release", "asset", "url", "cmake_toolset")
    if not all(isinstance(item.get(key), str) for key in required):
        raise ValueError(f"DevKit metadata for Archicad {version} is incomplete")
    return item


def install_dir(version: str) -> Path:
    return DEVKIT_ROOT / f"ac{version}"


def find_support_directory(version: str) -> Path | None:
    root = install_dir(version)
    candidates = [path for path in root.rglob("Support") if (path / "Inc").is_dir()]
    if len(candidates) == 1:
        return candidates[0]
    if len(candidates) > 1:
        raise RuntimeError(f"Multiple DevKit Support folders found under {root}")
    return None


def download(url: str, destination: Path) -> None:
    print(f"Downloading {url}")
    with urllib.request.urlopen(url) as response, destination.open("wb") as archive:
        shutil.copyfileobj(response, archive)


def install(version: str) -> int:
    item = version_config(version)
    existing = find_support_directory(version)
    if existing:
        print(f"Archicad {version} DevKit is already installed: {existing}")
        return 0

    target = install_dir(version)
    archive = DEVKIT_ROOT / item["asset"]
    target.mkdir(parents=True, exist_ok=True)
    DEVKIT_ROOT.mkdir(parents=True, exist_ok=True)
    try:
        if not archive.exists():
            download(item["url"], archive)
        print(f"Extracting {archive.name} to {target}")
        with zipfile.ZipFile(archive) as zip_file:
            zip_file.extractall(target)
    except Exception:
        if target.exists():
            shutil.rmtree(target)
        raise

    support = find_support_directory(version)
    if not support:
        raise RuntimeError(f"No valid Support directory was found after extracting {archive}")
    print(f"Archicad {version} DevKit installed: {support}")
    return 0


def validate() -> int:
    config = load_config()
    result = 0
    for version, _item in sorted(config["devkits"].items()):
        try:
            checked = version_config(str(version))
            support = find_support_directory(str(version))
            installation = str(support) if support else "not installed"
            print(f"AC{version}: {checked['release']} ({checked['cmake_toolset']}); {installation}")
        except (RuntimeError, ValueError) as error:
            print(f"AC{version}: invalid metadata: {error}", file=sys.stderr)
            result = 1
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    install_parser = commands.add_parser("install", help="download and install a pinned DevKit")
    install_parser.add_argument("version", choices=("27", "28", "29"))
    path_parser = commands.add_parser("path", help="print an installed DevKit Support directory")
    path_parser.add_argument("version", choices=("27", "28", "29"))
    commands.add_parser("validate", help="validate manifest and report installed DevKits")
    args = parser.parse_args()

    if args.command == "install":
        return install(args.version)
    if args.command == "path":
        support = find_support_directory(args.version)
        if not support:
            print(
                f"Archicad {args.version} DevKit is not installed. Run bootstrap.ps1 first.",
                file=sys.stderr,
            )
            return 1
        print(support)
        return 0
    return validate()


if __name__ == "__main__":
    raise SystemExit(main())
