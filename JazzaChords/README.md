# Jazza Chords

A chord progression instrument plugin (VST3, plus AU and Standalone on macOS), inspired by the OneMotion Chord Player web app and made for FL Studio.

- **Chord palette:** sus2, triad, sus4, 7th and 9th chords for every degree of the chosen key and scale (Major, Minor, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian). Click a chord to hear it. Drag it or double-click it to add it to the progression.
- **Progression strip:** drag chords to reorder them, or drag one back onto the palette to remove it. Right-click a chord to remove or duplicate it, or to change its length or chord type.
- **Styles:** Block, Quarters, Eighths, Dance, Arp Up, Arp Down, Arp Up-Down, Broken and Strum. There's also a voicing setting (Close, Smooth voice-leading or Open), an optional bass note, and an octave setting.
- **Built-in sound:** Piano, E-Piano, Pad or Organ. The plugin also outputs its MIDI, and you can play its synth from your keyboard.
- **Playback:** follows FL's transport when *Sync to host* is on, or press **Play** to loop on its own.
- **Drag MIDI into FL:** drag the **Drag MIDI** box into the Playlist or Channel Rack to drop the whole progression as a MIDI clip, in the current style and tempo. You can also drag a single chord tile out of the window.
- **Explore:** generates a random progression in the current key.

## Building

You need CMake 3.22+, plus Visual Studio 2022 (C++ workload) on Windows or Xcode on macOS.

```
cmake -B build
cmake --build build --config Release --parallel
```

The output goes to `build/JazzaChords_artefacts/Release/`. GitHub Actions builds both Windows and macOS on every push.

## Installing in FL Studio

Copy `Jazza Chords.vst3` to `C:\Program Files\Common Files\VST3\`. In FL, open *Options → Manage plugins → Find plugins*, then add **Jazza Chords** as an instrument in the Channel Rack.
