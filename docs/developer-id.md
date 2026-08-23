# Developer ID, Local ID, and distribution

Every Archicad Add-On needs an `MDID` resource with resource ID `32500`. Its two numbers are the Graphisoft-issued **Developer ID** and the per-Add-On **Local ID**. The repository's `RCZE/AddOn.grc` and `RINT/AddOn.grc` contain `1, 1` placeholders for private local development only.

Before distribution:

1. Register as an Archicad API developer and create the Add-On in your Graphisoft developer profile.
2. Obtain the Developer ID and Local ID through Graphisoft's process.
3. Replace both placeholders in the MDID resource through a reviewed release-only change.
4. Keep product/company metadata in `config.json` current.
5. Build through a separate release path with `AC_ADDON_FOR_DISTRIBUTION=ON`.
6. Test the signed/release artifact in every target Archicad major.

Graphisoft documents the identifiers in [How to get MDID for my Add-On?](https://archicadapi.graphisoft.com/how-to-get-mdid-for-my-add-on). Do not store account credentials, signing secrets, or private keys in this repository or GitHub Actions variables committed as files.

The ordinary presets and CI intentionally keep `AC_ADDON_FOR_DISTRIBUTION=OFF`. Changing MDIDs alone does not create a release-ready artifact.
