# /// script
# requires-python = ">=3.11"
# dependencies = ["pyyaml==6.0.2"]
# ///
"""Report or apply reviewed upstream pin updates for supported Archicad majors."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import urllib.request
from pathlib import Path
from typing import Any

import yaml

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "georaster.yaml"
DEVKIT_REPOSITORY = "GRAPHISOFT/archicad-api-devkit"
USER_AGENT = "archicad-georaster"


def load_manifest(path: Path = MANIFEST) -> dict[str, Any]:
    """Load the repository's version manifest."""
    with path.open(encoding="utf-8") as source:
        return yaml.safe_load(source)


def github_json(endpoint: str) -> Any:
    """Read JSON from GitHub's public API."""
    request = urllib.request.Request(
        f"https://api.github.com/{endpoint}",
        headers={"Accept": "application/vnd.github+json", "User-Agent": USER_AGENT},
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def _release_key(tag: str) -> tuple[int, ...]:
    return tuple(int(part) for part in tag.split("."))


def latest_release_for_major(releases: list[dict[str, Any]], major: int) -> dict[str, Any] | None:
    """Select the newest stable release for one major with its Windows DevKit asset."""
    candidates: list[dict[str, Any]] = []
    tag_pattern = re.compile(rf"^{major}\.\d+(?:\.\d+)*$")
    for release in releases:
        tag = str(release.get("tag_name", ""))
        if release.get("draft") or release.get("prerelease") or not tag_pattern.fullmatch(tag):
            continue
        expected_asset = f"API.Development.Kit.WIN.{tag}.zip"
        assets = {asset.get("name") for asset in release.get("assets", [])}
        if expected_asset in assets:
            candidates.append(release)
    if not candidates:
        return None
    return max(candidates, key=lambda item: _release_key(item["tag_name"]))


def update_supported_devkits(config: dict[str, Any], releases: list[dict[str, Any]]) -> list[int]:
    """Update only explicitly supported and already configured Archicad majors."""
    changed: list[int] = []
    supported = {int(version) for version in config["project"]["supported_archicad"]}
    for version, item in sorted(config["devkits"].items()):
        major = int(version)
        if major not in supported:
            continue
        latest = latest_release_for_major(releases, major)
        if latest is None or latest["tag_name"] == item["release"]:
            continue
        tag = latest["tag_name"]
        asset = f"API.Development.Kit.WIN.{tag}.zip"
        item.update(
            release=tag,
            asset=asset,
            url=f"https://github.com/{DEVKIT_REPOSITORY}/releases/download/{tag}/{asset}",
        )
        changed.append(major)
    return changed


def remote_head(url: str) -> str:
    """Resolve an upstream repository's default-branch HEAD."""
    result = subprocess.run(
        ["git", "ls-remote", url, "HEAD"], check=True, capture_output=True, text=True
    )
    return result.stdout.split()[0]


def update_cmake_tools(config: dict[str, Any], *, apply: bool) -> bool:
    """Report and optionally advance the CMake tools submodule and manifest pin."""
    tools = config["upstreams"]["graphisoft_cmake_tools"]
    head = remote_head(tools["url"])
    if head == tools["pinned_commit"]:
        return False
    if apply:
        subprocess.run(
            ["git", "submodule", "update", "--remote", tools["path"]], cwd=ROOT, check=True
        )
        tools["pinned_commit"] = subprocess.run(
            ["git", "-C", str(ROOT / tools["path"]), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
        ).stdout.strip()
    return True


def write_manifest(config: dict[str, Any], path: Path = MANIFEST) -> None:
    """Persist the manifest with stable key ordering."""
    path.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")


def check_updates(*, apply: bool = False) -> dict[str, Any]:
    """Collect upstream differences and optionally update controlled pins."""
    config = load_manifest()
    releases = github_json(f"repos/{DEVKIT_REPOSITORY}/releases?per_page=100")
    if apply:
        devkits = update_supported_devkits(config, releases)
    else:
        devkits = [
            int(version)
            for version, item in config["devkits"].items()
            if (latest := latest_release_for_major(releases, int(version))) is not None
            and latest["tag_name"] != item["release"]
        ]
    cmake_changed = update_cmake_tools(config, apply=apply)
    if apply and (devkits or cmake_changed):
        write_manifest(config)
    return {"devkits": devkits, "cmake_tools": cmake_changed}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true", help="update controlled pins")
    parser.add_argument("--json", action="store_true", help="emit machine-readable output")
    args = parser.parse_args()
    changes = check_updates(apply=args.apply)
    if args.json:
        print(json.dumps(changes, indent=2))
    else:
        action = "Updated" if args.apply else "Available"
        print(f"{action} DevKits: {changes['devkits'] or 'none'}")
        print(f"{action} CMake tools: {'yes' if changes['cmake_tools'] else 'no'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
