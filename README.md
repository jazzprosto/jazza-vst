# TriFX

> This repo also contains **[Jazza Chords](JazzaChords/)**, a chord progression instrument for FL Studio with drag-and-drop MIDI. It lives in its own folder with its own build.

A VST3 effect plugin (also AU and Standalone on macOS) with three effects in series:

**Distortion → Delay → Reverb**

| Section    | Controls                                                    |
|------------|-------------------------------------------------------------|
| Distortion | On, Type (Soft / Hard / Fold), Drive, Tone, Output, Mix      |
| Delay      | On, Ping-Pong, Time (1–2000 ms), Feedback, Tone, Mix         |
| Reverb     | On, Size, Damping, Width, Mix                               |

The distortion runs with 4x oversampling to reduce aliasing. The plugin reports the small latency this adds, so the host can compensate. Turning a section off fades it out smoothly, so you won't hear clicks.

Built with [JUCE](https://juce.com) 8, which CMake downloads automatically.

## Building with GitHub Actions (easiest)

Push this folder to a GitHub repository. The workflow in `.github/workflows/build.yml` builds the plugin on Windows and on macOS (one universal binary for Apple Silicon and Intel). Download the zips from the run's **Artifacts** section.

## Building locally

Requirements: CMake 3.22 or newer, and Git.
- **Windows:** Visual Studio 2022 with the "Desktop development with C++" workload.
- **macOS:** Xcode, or at least the Xcode Command Line Tools.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

The output goes to `build/TriFX_artefacts/Release/`.

## Installing

- **Windows:** copy `TriFX.vst3` to `C:\Program Files\Common Files\VST3\`
- **macOS:** copy `TriFX.vst3` to `~/Library/Audio/Plug-Ins/VST3/` and `TriFX.component` to `~/Library/Audio/Plug-Ins/Components/`

Then rescan plugins in your DAW.

On macOS, a downloaded build is ad-hoc signed but not notarized, so Gatekeeper may block it. To clear the quarantine flag, run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/TriFX.vst3 ~/Library/Audio/Plug-Ins/Components/TriFX.component
```
