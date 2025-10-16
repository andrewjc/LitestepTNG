# LiteStep NG - Next Generation Windows Shell Replacement

LiteStep NG is the modern Windows 10 and Windows 11 shell replacement that evolves the classic LiteStep vision with a performant core, a declarative theme engine, and tooling tuned for creators who want total control over the desktop experience. This project is built for power users, themers, and automation enthusiasts who need a lightweight, extensible alternative to Explorer.

## Table of Contents
- [Highlights](#highlights)
- [System Requirements](#system-requirements)
- [Architecture](#architecture)
- [Differences from Classic LiteStep](#differences-from-classic-litestep)
- [Getting Started](#getting-started)
- [Theme Engine V2 at a Glance](#theme-engine-v2-at-a-glance)
- [Extending LiteStep](#extending-litestep)
- [Community & Support](#community--support)
- [License](#license)

## Highlights
- **Windows 10/11 core** - Ships with a Windows 10 manifest, targets `_WIN32_WINNT=0x0A00`, and optimizes shell services for current platforms.
- **Theme Engine V2** - Author elegant layouts with readable `.lsx` markup and pair them with `.lsxstyle` skins for modern glass, blur, and accent effects.
- **High-performance LSAPI** - A built-in task executor and shared services keep modules responsive without spawning one-off threads.
- **Modular build pipeline** - Unified MSBuild property sheets simplify module packaging for both x64 and Win32 releases while keeping CI artifacts tidy.
- **Polished defaults** - The bundled theme delivers a translucent Windows 11-inspired desktop with curated modules and keyboard shortcuts out of the box.

## System Requirements
- Windows 10 (version 1903 or newer) or Windows Server 2016. Earlier operating systems are not supported by this fork.
- Visual Studio 2022 with the v143 toolset, or an equivalent MSBuild toolchain.
- Optional: Windows SDK 10.0 for module development and Doxygen for API documentation.

## Architecture
```text
                   +----------------------------+
                   |   Theme Files (.lsx)       |
                   +-------------+--------------+
                                 |
                                 v
         +-----------------------+-----------------------+
         |             Theme Engine V2                   |
         |  parsing + validation + layout + binding      |
         +----+---------------+---------------+---------+
              |               |               |
              v               v               v
      +-------+-----+   +-----+------+   +----+----------------+
      | LiteStep    |   | LSAPI Core |   | TaskExecutor Pool  |
      | Core Host   |   | Services   |   | async work queues  |
      +-------+-----+   +-----+------+   +----+----------------+
              |               |
              v               v
      +-------+------+   +----+-------------------------------+
      | Modules &    |   | Shell Integrations (taskbar, tray,|
      | Extensions   |   | hotkeys, notifications, startup)  |
      +--------------+   +-----------------------------------+
                                 |
                                 v
                   +----------------------------+
                   |   Windows 10 / 11 APIs     |
                   +----------------------------+
```

## Differences from Classic LiteStep
Andrew Cranston (AndrewJC / Andrew C) led a multi-stage modernization that realigns LiteStep with Windows 10-era expectations:

- **Windows 10 baseline** - Commit `210dd44` raises the manifest and `_WIN32_WINNT` values to 0x0A00, removing legacy shims while `docs/manual.txt` now states Windows 10/Server 2016 as the minimum supported platforms. Earlier versions of Windows are no longer targeted.
- **Modern toolchain** - Commit `23c33c4` retargets the solution to Visual Studio 2022 and the v143 toolset, while `modules/BuildProperties/*.props` (5a6ca4b) introduce shared MSBuild property sheets to align module outputs, warning levels, and platform directories.
- **Responsive LSAPI** - Commit `b15c1ad` introduces a pooled task executor so modules queue asynchronous work instead of launching raw threads, improving stability during shell startup and recycle scenarios.
- **Startup and overlay experience** - Commits `94862ed`, `307b80a`, `9966a53`, and `34837dd` add overlay launch modes, Explorer coexistence prompts, automatic shutdown of previous instances, and first-run shell configuration flows tailored for modern Windows sessions.
- **Windows-aware input and notifications** - Commits `cf14d07` and `93ffc74` extend nKey to understand Windows-key combinations, and `1208371` adopts the Windows 10 tray notification model for reliable toast and overflow behavior.
- **Diagnostics-first approach** - Commit `08e5a06` bolts in structured logging, richer module diagnostics, and a refreshed `StartupRunner`, reducing the guesswork when debugging theme or module issues.
- **Declarative theming overhaul** - Commit `9f840b2` lands Theme Engine V2 and a translucent default theme, separating structure (`theme.lsx`) from style (`theme.lsxstyle`) and demonstrating modern quick-launch, taskbar, and tray layouts tuned for Windows 11 aesthetics.

Together these changes deliver a Windows 10+ native shell experience while intentionally dropping support for Windows 7, Vista, XP, and earlier.

## Getting Started

### 1. Download or Clone
```bash
git clone <repository-url>
cd litestep
```
(If modules are submodules, run `git submodule update --init`.)

### 2. Build with Visual Studio
1. Open `litestep.sln`.
2. Select `x64` or `Win32` and choose `Debug` or `Release`.
3. Build; binaries land in `bin/<Configuration>_<Platform>/`.

### 3. Run the Shell
1. Copy `bin/Release_x64` into a staging directory.
2. Launch `LiteStep.exe` once to register services and warm the cache.
3. Use `Run Overlay.lnk` to test the shell in a window before replacing Explorer.

## Theme Engine V2 at a Glance
Structure and style now live in separate files:

```lsl
#panel { // Start button
   name = "StartButton"
   layout = "stack"
   children = [
      #button { icon = "start_icon.png" tooltip = "Start" style = "startbutton" }
   ]
}
```

```css
#startbutton {
   radius = 8
   background = radialGradient(center, THEME_COLOR_ACCENT, THEME_COLOR_ACCENT_MINOR)
}
```

See `docs/themev2.txt` for the full language reference, migration tips, and selector guide.

## Extending LiteStep
- Build new modules against `sdk/include/lsapi.h` and link with the provided LSAPI libraries.
- Use the task executor for background work instead of creating unmanaged threads.
- Emit module metadata so Theme Engine V2 can auto-bind layout nodes to your components.

## Community & Support
- Read `docs/manual.txt` for installation guidance, troubleshooting steps, and bang references.
- Track historical changes or migration paths with `docs/changes.txt` and `docs/themev2.txt`.
- Share themes and modules with the LiteStep community (forums and Discord coming soon).

## License
LiteStep NG is released under the GNU General Public License v2. Review `docs/license.txt` for full licensing terms.
