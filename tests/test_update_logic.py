from __future__ import annotations

from tools.check_updates import latest_release_for_major, update_supported_devkits


def release(tag: str, *assets: str, prerelease: bool = False) -> dict[str, object]:
    return {
        "tag_name": tag,
        "draft": False,
        "prerelease": prerelease,
        "assets": [{"name": asset} for asset in assets],
    }


def test_latest_release_stays_within_requested_major_and_requires_windows_asset() -> None:
    releases = [
        release("30.1000", "API.Development.Kit.WIN.30.1000.zip"),
        release("29.3200", "API.Development.Kit.MAC.29.3200.zip"),
        release("29.3100", "API.Development.Kit.WIN.29.3100.zip"),
        release("29.3000", "API.Development.Kit.WIN.29.3000.zip"),
    ]

    selected = latest_release_for_major(releases, 29)

    assert selected is not None
    assert selected["tag_name"] == "29.3100"


def test_prereleases_are_ignored() -> None:
    releases = [
        release("28.5000", "API.Development.Kit.WIN.28.5000.zip", prerelease=True),
        release("28.4001", "API.Development.Kit.WIN.28.4001.zip"),
    ]

    selected = latest_release_for_major(releases, 28)

    assert selected is not None
    assert selected["tag_name"] == "28.4001"


def test_updater_never_adds_an_unconfigured_major() -> None:
    config = {
        "project": {"supported_archicad": [27, 28, 29]},
        "devkits": {
            29: {
                "release": "29.3000",
                "asset": "API.Development.Kit.WIN.29.3000.zip",
                "url": "old",
            }
        },
    }
    releases = [
        release("30.1000", "API.Development.Kit.WIN.30.1000.zip"),
        release("29.3100", "API.Development.Kit.WIN.29.3100.zip"),
    ]

    changed = update_supported_devkits(config, releases)

    assert changed == [29]
    assert config["devkits"][29]["release"] == "29.3100"
    assert 30 not in config["devkits"]
