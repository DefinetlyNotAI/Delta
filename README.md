# Delta

Delta is a Windows psych-horror alternate-reality-game (ARG) experience. It is
implemented in C with the Win32 API and CMake, and intentionally combines
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
- A Windows C compiler supported by CMake; the project is verified with
  MinGW-w64/Ninja
- Windows system libraries supplied by the Windows SDK or MinGW-w64:
  - ARG: WinMM, SAPI, CryptoAPI, Shell, OLE, and GDI+
  - TREE: Shell, OLE, and GDI+

Configure and build:

```powershell
cmake --preset cmake-debug
cmake --build cmake/cmake-build-debug
```

The build produces `ARG.exe` and `TREE.exe`.

`ARG.exe` requires a valid Base64-encoded `tree.dat` beside it. The
distribution builder creates this payload from the freshly built `TREE.exe`
automatically.

## Create a distribution

Use the included PowerShell builder from the repository root:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\cli\build.ps1 -Configuration Release
```

The script:

1. Configures and builds the `ARG` and `TREE` CMake targets.
2. Creates `dist/Delta_yyyyMMdd_HHmmss/`, adding a numeric suffix if needed.
3. Copies only ARG runtime files and assets.
4. Encodes `TREE.exe` into `tree.dat` using standard Base64.
5. Removes the intermediate `TREE.exe` from the distribution.
6. Writes a package `README.txt` containing SHA-256 hashes for `ARG.exe` and
   `tree.dat`.

`dist/` is intentionally ignored by Git.

Do not manually copy an unpacked `TREE.exe` into a package. The main program
expects `tree.dat` and validates the decoded executable before installing it.

## Runtime layout

```text
Delta_<timestamp>/
├── ARG.exe
├── tree.dat                 # Base64-encoded TREE executable
└── assets/
    ├── img/
    │   ├── static.jpg
    │   ├── countdown/
    │   │   └── 0.png ... 5.png
    │   └── creep/
    ├── mus/
    │   ├── static.mp3
    │   ├── undoitall.mp3
    │   └── whisper.mp3
    └── text/
        └── poems.txt
```

TREE embeds `assets/img/tree/tree.jpg` and `assets/text/tree.txt` as Windows resources.
Those two source assets therefore do not need to exist beside the installed
Startup copy.

## Experience flow

| Stage | Behavior                                                                                                                                                                       |
|-------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| 1     | Random ambient static, whisper, and generated-machine audio events.                                                                                                            |
| 2     | Timed visual glitches, color/noise blocks, screen inversion, and display-only rotation transforms.                                                                             |
| 3a    | Actual CMD, PowerShell, and Notepad windows type corrupted text.                                                                                                               |
| 3b    | Eight reserved poem blocks from `assets/text/poems.txt` are displayed in actual text hosts.                                                                                    |
| 4     | Eight separately reserved poem blocks are spoken through local SAPI TTS; consented mouse inversion and keyboard blocking occur only briefly.                                   |
| 5     | Consent-gated Calculator, Paint, Notepad, Edge InPrivate, and local read-only command windows are created and split by the overlay.                                            |
| 6     | A black click-filter overlay exposes two transparent `:)` regions; seven blocked clicks advance the experience.                                                                |
| 7     | `undoitall.mp3` plays for 3m47s while wallpapers, static overlays, reminders, TREE extraction, and the final countdown run.                                                    |
| TREE  | On the next boot, the installed TREE program displays its embedded image, types its embedded story, removes its own Startup copy, then invokes its non-crashing terminal stub. |

## Controls and cleanup

- **F10 during ARG**: stops the ambient audio worker and MCI/WinMM playback,
  stops static and glitch visuals, restores the saved wallpaper, unhooks
  temporary mouse/keyboard effects, stops TTS, closes only tracked ARG-created
  child processes, and removes only the Startup copy installed during the
  current run.
- **F10 or Escape during TREE**: ends TREE, schedules removal only when the
  running executable is exactly the current-user Startup `tree.exe`, and exits
  through the same non-crashing terminal contract.
- Optional effects default to **No**. Declining an option keeps the experience
  moving without enabling that effect.

The ambient audio worker is explicitly stopped and joined before Step 7 takes
exclusive control of finale audio. This prevents an earlier ambient event from
racing the finale and stopping its MCI playback.

## Step 7 and TREE details

The final main-stage scheduler is message-loop driven; it does not use blocking
sleep loops for its wallpaper/countdown progression.

It provides these exact special wallpaper sequences:

- `happy1.png` through `happy9.png`: 100 ms per frame, 27 frames total
  (three 0.9-second cycles).
- `ht1.png` through `ht4.png`: 500 ms per frame, one 2-second cycle.
- `thisisgreen1.webp` through `thisisgreen4.webp`: 200 ms per frame, 16
  frames total (four 0.8-second cycles).

Every wallpaper transition plays a 50 ms static overlay. Countdown images 5
through 0 are displayed for one second each with 100 ms static.

Ambient static audio also paints `assets/img/static.jpg` through the program overlay
and rotates it 180 degrees on the static timer.

Before the countdown, `ARG.exe`:

1. Decodes `tree.dat` using `CryptStringToBinaryA`.
2. Verifies the decoded data has valid DOS and PE signatures.
3. Writes the executable through a temporary file.
4. Atomically replaces the local `tree.exe`.
5. Copies it to the current user's Startup directory.

Missing, malformed, empty, or non-PE payloads are recoverable preflight
failures; a corrupt executable is not installed.

TREE is a separate Win32 executable. Its image and story are compiled into
`TREE.exe` as `RCDATA` resources through `tree.rc`.

When TREE finishes, it schedules deletion only when its running executable path
exactly matches the expected current-user Startup `tree.exe`. Running a
development copy elsewhere does not cause that copy to be deleted.

## Architecture

The main ARG runtime is split by subsystem rather than maintained as one large
translation unit.

| File             | Purpose                                                                                   |
|------------------|-------------------------------------------------------------------------------------------|
| `src/`           | All C sources and headers, including the ARG subsystems and TREE resource script.         |
| `tree.dat`       | Base64-encoded TREE payload consumed by ARG; regenerate it with `cli/build.ps1`.          |
| `cli/build.ps1`  | Reproducible timestamped package builder.                                                 |
| `CMakeLists.txt` | Builds the `ARG` and `TREE` Win32 targets and declares their system-library dependencies. |
| `assets/`        | Runtime audio, image, and text assets.                                                    |

### Runtime ownership

The ARG runtime deliberately tracks resources it creates so cleanup remains
scoped to the experience.

The text/application subsystem tracks spawned processes by PID and closes only
those tracked children. It does not enumerate and close arbitrary user
applications.

The overlay subsystem owns visual/input effects and restores temporary state
during shutdown.

The audio subsystem owns its ambient worker and waits for that worker to finish
before shared audio resources are reused or destroyed.

TREE has an independent lifetime and does not share ARG's process state. Its
required image and story are embedded directly in its executable.

## Poem contract

`assets/text/poems.txt` must contain at least **16 usable poem blocks**.

The ARG runtime reserves them as two independent groups:

- 8 poems for program-owned display windows.
- 8 poems for local SAPI TTS.

The reservation order is randomized so the two groups do not reuse the same
reserved entries during a run.

Blank lines delimit poem blocks. Keep each block within the runtime text-buffer
limit.

If fewer than 16 usable blocks are available, the poem-dependent sequence
cannot be initialized completely.

## Asset contract

Do not rename or omit these runtime paths without changing the corresponding
code and distribution builder:

- `assets/img/static.jpg`
- `assets/img/creep/`
- `assets/img/countdown/0.png` through `5.png`
- `assets/mus/static.mp3`
- `assets/mus/undoitall.mp3`
- `assets/mus/whisper.mp3`
- `assets/text/poems.txt`
- `tree.dat`

The `assets/img/creep/` directory must include every regular wallpaper used by Step 7
and the complete special sequences:

```text
happy1.png ... happy9.png
ht1.png ... ht4.png
thisisgreen1.webp ... thisisgreen4.webp
```

For TREE source builds, also keep:

```text
assets/img/tree/tree.jpg
assets/text/tree.txt
src/tree.rc
```

Rebuild TREE after changing either embedded asset so the updated resource is
compiled into the executable.

## Build targets

### ARG

`ARG` is a Windows GUI executable composed of:

```text
main.c
arg_state.c
arg_core.c
arg_audio.c
arg_overlay.c
arg_text.c
arg_step7.c
state.h
```

Its Windows system-library dependencies are:

```text
winmm
ole32
sapi
crypt32
shell32
gdiplus
```

### TREE

`TREE` is a separate Windows GUI executable composed of:

```text
tree.c
tree.rc
```

Its direct system-library dependencies are:

```text
ole32
shell32
gdiplus
```

TREE does not require WinMM, SAPI, or CryptoAPI.

## Development and verification

Useful checks:

```powershell
# Configure
cmake --preset cmake-debug

# Normal build
cmake --build cmake/cmake-build-debug

# Strict compiler audit (MinGW/GCC)
cmake --preset cmake-audit

cmake --build cmake/cmake-build-audit

# Whitespace errors
git diff --check

# Package release
.\cli\build.ps1 -Configuration Release
```

A clean strict build is particularly useful after changing shared declarations
in `state.h`, because those declarations are consumed by several independent
translation units.

Do not use an interactive run as a routine test. The application intentionally
changes visual/audio state and may open consented child applications. Prefer
build, asset, payload, and package checks unless you are specifically
validating the experience on a disposable test environment.

## Troubleshooting

| Symptom                              | Check                                                                                                                                                        |
|--------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `tree.dat` is rejected               | Re-run `cli/build.ps1`; confirm it is beside `ARG.exe` and was not copied through a text editor that modified the Base64 data.                               |
| Wallpapers do not change             | Confirm the countdown/creep assets are present and Windows allows wallpaper changes. ARG preserves the current wallpaper before its first successful change. |
| Poems fail to initialize             | Confirm `assets/text/poems.txt` contains at least 16 non-empty poem blocks separated by blank lines.                                                         |
| An optional effect does not run      | Verify it was accepted in its consent dialog; all optional effects default to No.                                                                            |
| TREE cannot load its image/story     | Rebuild TREE after restoring `assets/img/tree/tree.jpg` and `assets/text/tree.txt`; both are embedded at compile time.                                       |
| Build cannot find GDI+ headers       | Use a Windows-oriented compiler/toolchain with the Windows SDK or MinGW-w64 headers and libraries installed.                                                 |
| TREE unexpectedly remains in Startup | Confirm TREE reached normal cleanup and that the running copy is exactly the current-user Startup `tree.exe`.                                                |
| Finale audio stops unexpectedly      | Rebuild from the refactored source; the ambient worker must be stopped and joined before Step 7 assumes control of finale audio.                             |

## Scope and limitations

- Delta is Windows-only.
- It contains no network upload or telemetry.
- The only next-boot handoff is the disclosed current-user Startup TREE finale.
- Optional disruptive effects require explicit consent.
- ARG tracks and cleans up only processes it creates.
- TREE only schedules self-removal when running from its exact expected Startup
  path.
- The `BSOD()` names are narrative terminal stubs; they do not implement an
  operating-system crash.
- The project does not ship a license file. Confirm ownership and distribution
  rights for all included media before publishing or redistributing it.
