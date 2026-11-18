# Release automation

Pushing a version tag builds the mod on a self-hosted Windows runner and publishes a GitHub Release with the zip. Uploading that zip to ficsit.app is still done by hand ("New Version" on the mod page).

## One-time setup

You need the Satisfactory modding project (with the Satisfactory fork of Unreal Engine, Wwise integrated) on the runner machine.

### 1. The build project

The workflow builds in a separate copy of your modding project, so builds never touch your development copy:

```powershell
.\.github\ci\setup-build-project.ps1 -Source D:\Modding\Satisfactory -Destination E:\CI\project
```

The first build in the copy compiles the whole game module and takes a long time. Later builds are incremental, so keep the copy. Update it (rerun the script) after upgrading the project to a new game version.

The paths the workflow uses are set in `release.yml` under `env`: `ENGINE_DIR` and `BUILD_PROJECT_DIR`.

### 2. The runner

In the repository on GitHub: **Settings → Actions → Runners → New self-hosted runner → Windows**. Follow the download and `config.cmd` steps it shows. The token on that page is a secret; don't share it.

- Install it in a short path (for example `E:\CI\runner`).
- Start it by hand with `run.cmd` when you want to release. Jobs wait in the queue while it's off.
- Don't run it as a service unless you want it always on.

### 3. Repository settings

- **Settings → Actions → General → Fork pull request workflows**: require approval for outside contributors. The release workflow doesn't run on pull requests, but this keeps any future workflow safe.
- The workflow requests the `contents: write` permission it needs by itself. No other setting is required.

## Releasing

1. Bump `Version`, `VersionName` and `SemVersion` in `PresetColorsIni.uplugin`, and commit.
2. Start the runner (`run.cmd`).
3. Tag and push:

   ```
   git tag v0.1.0
   git push origin v0.1.0
   ```

   The tag must equal `v` plus `SemVersion`.
4. The workflow checks the version, builds, checks the zip, and creates a GitHub Release with `PresetColorsIni.zip` attached.
5. Upload that zip to ficsit.app as a new version.
