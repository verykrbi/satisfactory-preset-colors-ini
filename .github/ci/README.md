# Release automation

Pushing a version tag builds the mod on a **self-hosted Windows runner** (your own PC) and publishes a GitHub Release with the zip attached. Uploading that zip to [ficsit.app](https://ficsit.app) is still done by hand ("New Version" on the mod page).

- [How it works](#how-it-works)
- [One-time setup](#one-time-setup)
- [Releasing](#releasing)
- [Security](#security)
- [Maintenance](#maintenance)
- [Troubleshooting](#troubleshooting)

## How it works

### The runner

A GitHub Actions *runner* is a small program that executes workflow jobs. A self-hosted runner is one you install and run yourself, so the build happens on your machine instead of on GitHub's.

- **The runner calls GitHub, not the other way round.** It keeps an outbound HTTPS connection to GitHub and asks for work. GitHub never connects to your PC, so you open no ports and change no router or firewall settings.
- **Jobs wait while the runner is off.** If you push a tag with the runner stopped, the job stays queued ("Waiting for a runner to pick up this job...") and starts as soon as the runner starts. GitHub cancels jobs that wait too long (about 24 hours).
- **It runs as you.** The workflow's steps run with your user account's permissions, in a work folder inside the runner's folder.
- **Labels.** The workflow asks for a runner with the labels `self-hosted`, `Windows` and `X64`, which a Windows runner gets by default.

### The workflow

`.github/workflows/release.yml` runs only when a tag starting with `v` is pushed. It never runs on pull requests. Its steps:

1. **Check out** the repository tag into the runner's work folder.
2. **Check the tag** matches `SemVersion` in `PresetColorsIni.uplugin` (tag `v0.1.0` for `0.1.0`).
3. **Check the build setup** exists: the engine and the build project.
4. **Copy the mod** from the checkout into the build project's `Mods` folder.
5. **Build the editor target** (`FactoryEditor`). The cook step in packaging loads the editor build of the mod, so it has to exist first.
6. **Package** the mod with `RunUAT PackagePlugin` for Windows (Shipping), which produces the multi-target zip.
7. **Check the zip**: it must contain `Windows/PresetColorsIni.uplugin` and a Shipping DLL.
8. **Upload** the zip as a workflow artifact and **create the GitHub Release** with the zip attached. The release is created with GitHub's own API; no third-party actions run on your machine.

### Why a separate build project

The workflow builds in a dedicated copy of your modding project, not in the one you develop in. That keeps builds from touching your working copy or fighting with an open editor, and it gives the CI its own compiled output. The workflow copies the mod into the build project on every run, so the build project never needs the mod itself.

## One-time setup

You need on the runner machine:
- the Satisfactory modding project (the one this mod is developed in), with Wwise integrated;
- the Satisfactory fork of Unreal Engine;
- Git for Windows.

### 1. The build project

```powershell
.\.github\ci\setup-build-project.ps1 -Source <your modding project> -Destination <build project folder>
```

The script copies the project and leaves out what a build doesn't need: git history, Visual Studio files, local build output, and every mod except the ones the build itself needs (SML, Alpakit, SMLEditor, AccessTransformers, GameFeatures and WwisePatches). New mods are left out automatically; if a game or SML update adds a required folder, add it to `$keepMods` in the script.

- Put the build project on a drive with plenty of free space. It is roughly 12 GB when copied, and grows when you build in it.
- The **first build in it compiles everything from scratch** (about 1,900 compile steps), which takes hours. Later builds should only recompile what changed, so keep the folder.
- The paths the workflow uses are set at the top of `release.yml` under `env`: `ENGINE_DIR` and `BUILD_PROJECT_DIR`. Change them there if your engine or build project live elsewhere.

### 2. The runner

In the repository on GitHub: **Settings → Actions → Runners → New self-hosted runner → Windows**. The page shows the download steps and a `config.cmd` command with a registration token.

- Install it in a **short path** (for example `C:\CI\runner`). The Unreal build runs into Windows path-length limits otherwise.
- Answer the `config.cmd` questions with the defaults: default runner group, default name, default labels, default work folder.
- Answer **no** to "run as a service" if you want to start it by hand.
- The registration token is a secret. It expires within about an hour, but don't share it.

### 3. Repository settings

- **Pull requests:** make sure no workflow runs for pull requests from outside contributors (Settings → Actions → General → Fork pull request workflows → require approval). The release workflow doesn't run on pull requests, but this keeps any workflow you add later safe.
- No other setting is needed. The workflow asks for the `contents: write` permission it needs by itself.

## Releasing

1. Bump `Version`, `VersionName` and `SemVersion` in `PresetColorsIni.uplugin`, and commit and push.
2. Start the runner by hand: run `run.cmd` in the runner folder. It prints "Listening for Jobs", and GitHub shows it as Idle.
3. Tag and push:

   ```
   git tag v0.1.0
   git push origin v0.1.0
   ```

   The tag must equal `v` plus `SemVersion`.
4. Watch the run in the repository's **Actions** tab. Keep the runner window open and the PC awake until it finishes.
5. Download `PresetColorsIni.zip` from the GitHub Release and upload it to ficsit.app as a new version.

The zip includes the `.pdb` debug files (about 24 MB zipped). Players don't need them; they only make crash stack traces readable.

## Security

The workflow runs code on your PC with your user's permissions. Keep that in mind:

- **Never trigger it from pull requests.** On a public repository, anyone can open a pull request, and a workflow that runs on pull requests would run their code on your machine. This workflow runs only on tags you push.
- **Don't run the runner as administrator.**
- **Keep the runner folder private.** It contains the runner's credentials (`.credentials` and `.runner`). Don't commit it, zip it, or share it.
- **Don't share registration tokens.** Generate a new one on the Runners page when you need it.
- **Stop the runner when you're not releasing.** Starting it by hand, and closing it afterwards, means the machine accepts no jobs while it's off.

## Maintenance

- **After a game or SML update**, bring the build project up to date by rerunning `setup-build-project.ps1` against your updated modding project. It overwrites changed files and keeps the compiled output in the build project, so the next build isn't a full rebuild. Files that were removed from the source project are not deleted from the copy.
- **Updating the runner:** GitHub updates it automatically when it starts a job. If it asks for a manual update, follow the instructions on the Runners page.
- **Removing the runner:** run `config.cmd remove` in the runner folder (it needs a removal token from the Runners page), or delete it from the Runners page on GitHub.

## Troubleshooting

**The job stays on "Waiting for a runner to pick up this job..."**
The runner isn't running. Start it with `run.cmd`. Queued jobs start by themselves, no need to push the tag again.

**The first step fails in under a second, with "running scripts is disabled on this system".**
Windows PowerShell's default execution policy refuses to run script files, and the runner starts every step as a script file. The workflow sets `PSExecutionPolicyPreference: Bypass` in its `env` for this reason. If you see the error, check that line is still there. It only affects the workflow's own steps and doesn't change your machine's settings.

**"Version mismatch": the tag doesn't match `SemVersion`.**
The tag has to be `v` followed by `SemVersion` from `PresetColorsIni.uplugin` (for `0.1.0`, the tag `v0.1.0`). Fix the version or the tag.

**"Missing build setup": the engine or build project isn't found.**
Check `ENGINE_DIR` and `BUILD_PROJECT_DIR` in `release.yml`, and that the build project exists (see the setup above).

**The build is very slow.**
The first build in a new build project compiles about 1,900 files. It's slow once, then incremental. Keep the build project folder.

**Fixing a failed release and running it again.**
A re-run of a tag's job uses the workflow file from the commit the tag points to, so fixing the workflow isn't enough: the tag has to move to the fixed commit. If nothing was published under the tag yet, move it:

```
git push origin :refs/tags/v0.1.0
git tag -f v0.1.0
git push origin v0.1.0
```

If a release was already published for that tag, bump the version and use a new tag instead.
