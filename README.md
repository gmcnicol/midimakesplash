# Midi Make Splash

Midi Make Splash is a VST3 plug-in built from the JSFX source in `jsfx/WaterSplash.jsfx`.
The CMake build also creates a `midi_splash_smoke` console target that validates the plug-in and its bundled assets.

## macOS

### Prerequisites

Install:

- [Git](https://git-scm.com/)
- [CMake 3.24 or newer](https://cmake.org/download/)
- [Ninja](https://ninja-build.org/)
- [Xcode Command Line Tools](https://developer.apple.com/xcode/resources/)
- A VST3 host such as [REAPER](https://www.reaper.fm/)

Install common build tools with Homebrew:

```sh
brew install cmake ninja git
xcode-select --install
```

During configuration, CMake fetches [`ysfx`](https://github.com/jpcima/ysfx) and [`mrgee-fx`](https://github.com/gmcnicol/mrgee-fx) automatically from GitHub. `mrgee-fx` is fetched from the `main` branch.

### Build

From the repository root:

```sh
make configure
make build
```

This creates the VST3 bundle at:

```text
build/MidiSpash_artefacts/RelWithDebInfo/VST3/Midi Make Splash.vst3
```

To run the smoke test:

```sh
make smoke
```

### Installation

The default install location for the current user is:

```text
~/Library/Audio/Plug-Ins/VST3/Midi Make Splash.vst3
```

Install and verify the plug-in with:

```sh
make deploy
```

`make deploy` builds the VST3 target, runs the smoke test, copies the bundle into the user VST3 folder, removes macOS quarantine metadata, verifies the installed binary, and removes stale REAPER VST cache entries for this plug-in when present.

After installing, restart REAPER if it was open. If the plug-in does not appear, run **Preferences > Plug-ins > VST > Re-scan failed plug-ins** in REAPER.

## Windows

### Prerequisites

Install:

- [Git for Windows](https://git-scm.com/download/win)
- [CMake 3.24 or newer](https://cmake.org/download/)
- [Ninja](https://ninja-build.org/)
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/) with the **Desktop development with C++** workload
- A VST3 host such as [REAPER](https://www.reaper.fm/)

During configuration, CMake fetches [`ysfx`](https://github.com/jpcima/ysfx) and [`mrgee-fx`](https://github.com/gmcnicol/mrgee-fx) automatically from GitHub. `mrgee-fx` is fetched from the `main` branch.

Open a **Developer PowerShell for VS 2022** window so the Microsoft C++ compiler is available.

### Build

From the repository root:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --target MidiSpash_VST3 --config RelWithDebInfo
```

This creates the VST3 bundle at:

```text
build/MidiSpash_artefacts/RelWithDebInfo/VST3/Midi Make Splash.vst3
```

To build and run the smoke test:

```powershell
cmake --build build --target midi_splash_smoke --config RelWithDebInfo
.\build\midi_splash_smoke_artefacts\RelWithDebInfo\midi_splash_smoke.exe
```

### Installation

The standard system-wide VST3 location is:

```text
C:\Program Files\Common Files\VST3\Midi Make Splash.vst3
```

Copy the built bundle from PowerShell running as Administrator:

```powershell
New-Item -ItemType Directory -Force "$env:CommonProgramFiles\VST3" | Out-Null
Remove-Item -Recurse -Force "$env:CommonProgramFiles\VST3\Midi Make Splash.vst3" -ErrorAction SilentlyContinue
Copy-Item -Recurse "build\MidiSpash_artefacts\RelWithDebInfo\VST3\Midi Make Splash.vst3" "$env:CommonProgramFiles\VST3\"
```

Restart REAPER if it was open. If the plug-in does not appear, open **Options > Preferences > Plug-ins > VST**, confirm that `C:\Program Files\Common Files\VST3` is in the scan paths, then run **Re-scan**.
