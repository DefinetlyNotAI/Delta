# Delta

Delta is a Windows psych-horror alternate-reality-game (ARG) experience. It is
implemented in C with the Win32 API and CMake, and it intentionally combines
flashing visuals, audio, text windows, local TTS, temporary input effects, and
a final next-boot TREE finale.

> **Safety notice**
>
> This is not a background utility. Before starting, the program presents an
> explicit terms-of-service dialog and asks separately before enabling optional
> mouse, keyboard, application-launch, browser, and local-information effects.
> Save work first. Press **F10** at any time to abort the main experience and
> run cleanup. The terminal `BSOD()` functions are deliberate stubs that exit
> successfully; they do **not** crash Windows.

## Quick start

### Use a packaged build

Run `ARG.exe` from a generated `dist/Delta_<timestamp>/` folder. Keep its
folder structure intact: `tree.dat`, `img/`, `mus/`, and `text/` are runtime
dependencies.

### Build from source

Requirements:

- Windows 10 or later
- CMake 3.20 or later
- A Windows C compiler supported by CMake (the project is verified with
  MinGW-w64/Ninja)
- Windows multimedia, SAPI, Shell, Crypto, OLE, and GDI+ system libraries
  (included with supported Windows versions)

Configure and build:

```powershell
cmake -S . -B cmake-build-debug
cmake --build cmake-build-debug
```

The build produces `ARG.exe` and `TREE.exe`. `ARG.exe` requires a valid
Base64-encoded `tree.dat` beside it. The distribution builder below creates
that payload automatically.

## Create a distribution

Use the included PowerShell builder from the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\build-dist.ps1 -Configuration Release
```

The script:

1. Configures and builds the `ARG` and `TREE` CMake targets.
2. Creates `dist/Delta_yyyyMMdd_HHmmss/` (adding a numeric suffix if needed).
3. Copies only ARG runtime files and assets.
4. Encodes `TREE.exe` into `tree.dat` with standard Base64.
5. Removes the intermediate `TREE.exe`.
6. Writes a package `README.txt` containing SHA-256 hashes for `ARG.exe` and
   `tree.dat`.

`dist/` is intentionally ignored by Git. Do not manually copy an unpacked
`TREE.exe` into a package; the main program expects `tree.dat` and validates
the decoded executable before installing it.

## Runtime layout

```text
Delta_<timestamp>/
├── ARG.exe
├── tree.dat                 # Base64-encoded TREE executable
├── img/
│   ├── static.jpg
│   ├── countdown/0.png ... 5.png
│   └── creep/
├── mus/
│   ├── static.mp3
│   ├── undoitall.mp3
│   └── whisper.mp3
└── text/
    └── poems.txt
```

TREE embeds `img/tree/tree.jpg` and `text/tree.txt` as Windows resources, so
those two source assets are not required beside the installed Startup copy.

## Experience flow

| Stage | Behavior                                                                                                                                                                       |
|-------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1     | Random ambient static, whisper, and generated-machine audio events.                                                                                                            |
| 2     | Timed visual glitches, color/noise blocks, screen inversion, and display-only rotation transforms.                                                                             |
| 3a    | Program-owned CMD/Notepad windows type corrupted text.                                                                                                                         |
| 3b    | Half of `text/poems.txt` is displayed in program-owned text windows.                                                                                                           |
| 4     | The other poem half is spoken through local SAPI TTS; consented mouse inversion and keyboard blocking occur only briefly.                                                      |
| 5     | Consent-gated Calculator, Paint, Notepad, Edge InPrivate, and local read-only command windows are created and split by the overlay.                                            |
| 6     | A black click-filter overlay exposes two transparent `:)` regions; seven blocked clicks advance the experience.                                                                |
| 7     | `undoitall.mp3` plays for 3m47s while wallpapers, static overlays, reminders, TREE extraction, and the final countdown run.                                                    |
| TREE  | On the next boot, the installed TREE program displays its embedded image, types its embedded story, removes its own Startup copy, then invokes its non-crashing terminal stub. |

## Controls and cleanup

- **F10 during ARG**: stops MCI/WinMM audio, stops static and glitch visuals,
  restores the saved wallpaper, unhooks temporary mouse/keyboard effects,
  stops TTS, closes only tracked ARG-created child processes, and removes only
  the Startup copy installed during the current run.
- **F10 during TREE**: ends TREE, schedules removal only when the running
  executable is exactly the current-user Startup `tree.exe`, and exits through
  the same non-crashing terminal contract.
- Optional effects default to **No**. Declining an option keeps the experience
  moving without enabling that effect.

## Step 7 and TREE details

The final main-stage scheduler is message-loop driven; it does not use blocking
sleep loops. It provides these exact special wallpaper sequences:

- `happy1.png` through `happy9.png`: 100 ms per frame, 27 frames total
  (three 0.9-second cycles).
- `ht1.png` through `ht4.png`: 500 ms per frame, one 2-second cycle.
- `thisisgreen1.webp` through `thisisgreen4.webp`: 200 ms per frame, 16
  frames total (four 0.8-second cycles).

Every wallpaper transition plays a 50 ms static overlay. Countdown images 5
through 0 are displayed for one second each with 100 ms static. Static audio
also paints `img/static.jpg` through the program overlay and rotates it 180°
on the static timer.

Before the countdown, `ARG.exe` decodes `tree.dat` using
`CryptStringToBinaryA`, verifies DOS/PE signatures, writes through a temporary
file, atomically replaces `tree.exe`, and copies it to the current user's
Startup directory. Missing, malformed, or non-PE payloads are recoverable
preflight failures: no corrupt executable is installed.

## Architecture

| File             | Purpose                                                                                                                         |
|------------------|---------------------------------------------------------------------------------------------------------------------------------|
| `worker.c`       | Main ARG application, state progression, overlays, effects, audio, consent, cleanup, and TREE handoff.                          |
| `tree.c`         | Standalone TREE finale. Uses embedded resources, a fullscreen Win32 window, timer-driven typing, and Startup-only self-removal. |
| `tree.rc`        | Embeds TREE image and story as `RCDATA` resources.                                                                              |
| `tree.dat`       | Base64 payload consumed by `worker.c`; regenerate with `build-dist.ps1`.                                                        |
| `build-dist.ps1` | Reproducible timestamped package builder.                                                                                       |
| `CMakeLists.txt` | Builds the `ARG` and `TREE` Win32 targets.                                                                                      |
| `text/poems.txt` | Main-stage poem source; must supply at least eight usable blocks.                                                               |
| `text/tree.txt`  | TREE story source embedded at TREE build time.                                                                                  |

The main overlay tracks every spawned text/application process by PID and only
closes those tracked children. It never enumerates and closes arbitrary user
applications.

## Asset contract

Do not rename or omit these runtime paths without changing the code and
builder:

- `img/static.jpg`
- `img/creep/` (including every special-cycle filename listed above)
- `img/countdown/0.png` through `5.png`
- `mus/static.mp3`, `mus/undoitall.mp3`, `mus/whisper.mp3`
- `text/poems.txt`
- `tree.dat`

For TREE source builds, also keep `img/tree/tree.jpg` and `text/tree.txt`.
Rebuild TREE after changing either file so the updated resources are embedded.

## Development and verification

Useful checks:

```powershell
# Normal build
cmake --build cmake-build-debug

# Strict compiler audit (MinGW/GCC)
cmake -S . -B cmake-build-audit -DCMAKE_BUILD_TYPE=Debug `
  '-DCMAKE_C_FLAGS=-Wall -Wextra -Wpedantic -Werror'
cmake --build cmake-build-audit

# Whitespace errors
git diff --check

# Package release
.\build-dist.ps1 -Configuration Release
```

Do not use an interactive run as a routine test. It intentionally changes
visual/audio state and may open consented child applications. Prefer build,
asset, payload, and package checks unless you are specifically validating the
experience on a disposable test environment.

## Troubleshooting

| Symptom                         | Check                                                                                                                                                        |
|---------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `tree.dat` is rejected          | Re-run `build-dist.ps1`; confirm it is beside `ARG.exe` and was not copied through a text editor that changed Base64 data.                                   |
| Wallpapers do not change        | Confirm the countdown/creep assets are present and Windows allows wallpaper changes. ARG preserves the current wallpaper before its first successful change. |
| An optional effect does not run | Verify it was accepted in its consent dialog; all optional effects default to No.                                                                            |
| TREE cannot load image/story    | Rebuild TREE after restoring `img/tree/tree.jpg` and `text/tree.txt`; they are embedded at compile time.                                                     |
| Build cannot find GDI+ headers  | Use a Windows-oriented compiler/toolchain with the Windows SDK or MinGW-w64 headers and libraries installed.                                                 |

## Scope and limitations

- This project is Windows-only.
- It contains no network upload, telemetry, or persistence beyond the disclosed
  current-user Startup TREE handoff.
- The `BSOD()` names are narrative terminal stubs, not operating-system crash
  implementations.
- The project does not ship a license file. Confirm ownership and distribution
  rights for all included media before publishing or redistributing it.
