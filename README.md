# HORIZON (plugin)

An atmospheric piano under a live aurora, by Steve Turner: the HORIZON piano synth from his Mixtape program, as a plugin for
DAWs (Ableton Live, Logic, GarageBand, Reaper, Cubase, FL Studio, Bitwig…).

- **Mac:** Audio Unit (AU) and VST3, plus a stand-alone app. One download works on Intel and Apple Silicon Macs, macOS 11 or later.
- **Windows:** VST3 (64-bit), plus a stand-alone app.

This first version is a proof of concept: the Salamander Grand piano played from MIDI (velocity and the sustain pedal), with three
knobs your DAW can automate: **LEVEL**, **HALL** (reverb) and **RELEASE**. The window is HORIZON's night sky over the mountains, its
aurora following how loud you play.

## Download

The newest build is always here (no GitHub account needed):
**https://github.com/steturne/horizon-plugin/releases/tag/latest-build**

| File | What it is |
|---|---|
| `HORIZON-Mac-AU.zip` | the Audio Unit, for Ableton Live, Logic, GarageBand on a Mac |
| `HORIZON-Mac-VST3.zip` | the VST3, for other Mac DAWs (Live 10.1 and later, Reaper, Cubase, Bitwig…) |
| `HORIZON-Mac-App.zip` | HORIZON on its own, no DAW needed |
| `HORIZON-Windows-VST3.zip` | the VST3, for Windows DAWs |
| `HORIZON-Windows-App.zip` | HORIZON on its own on Windows |

## Installing on a Mac (AU, e.g. Ableton Live)

1. Download `HORIZON-Mac-AU.zip` and double-click it: you get **HORIZON.component**.
2. In Finder, press **Shift+Cmd+G**, type `~/Library/Audio/Plug-Ins/Components` and press Return.
   Drag **HORIZON.component** into that folder.
3. Open Ableton Live (if it was already open, **quit it and open it again**: Live only looks for new plugins as it starts). In
   **Preferences > Plug-Ins** (Live 9: **File/Folder**) turn on **Use Audio Units**. Live checks its plugins (a minute, the first time).
4. The plugin isn't signed by Apple (that costs $99 a year), so the Mac blocks it the first time ("cannot be opened because Apple
   cannot check it…"). Open **System Preferences > Security & Privacy > General**: near the bottom it names HORIZON, with
   **Allow Anyway** (or **Open Anyway**). Click it, then **quit Live and open it again**.

   (Or, before step 3: open **Terminal** (Applications > Utilities), paste this line and press Return; then the Mac doesn't ask.)
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/HORIZON.component
   ```
5. In Live's browser: **Plug-Ins > Audio Units > Steve Turner > HORIZON**. Drag it onto a MIDI track, arm the track and play.
   The window opens with the wrench/spanner icon on the device.

The first note may take a few seconds while the piano loads (the window says **TUNING…**).

**Window size:** it opens big (most of the screen). Make it bigger or smaller with **− +** at the top right, or drag the little
corner at the bottom right. It remembers its size with the song.

## Installing on Windows (VST3)

1. Download `HORIZON-Windows-VST3.zip`, right-click it > **Extract All**: you get a folder **HORIZON.vst3**.
2. Move the whole **HORIZON.vst3** folder into `C:\Program Files\Common Files\VST3` (or any folder your DAW scans for VST3).
3. Rescan plugins in your DAW. HORIZON is under **Steve Turner**.

## How it is built

Everything is compiled by GitHub Actions (`.github/workflows/build.yml`): nothing has to be installed on your own computer.
Each push builds the Mac and Windows versions, tests them with **pluginval** (and Apple's **auval** for the AU), and puts the zips
on the **latest-build** release.

- `core/` the sound engine in plain C++ (no JUCE), so the same code can later be compiled to WebAssembly for Mixtape and the web
- `plugin/` the JUCE 8 wrapper: parameters, MIDI, loading the piano, the window
- `face/` the window: a web page (HORIZON's sky shader, Steve's picture, the knobs), built into the plugin
- `samples/Salamander Grand/` the piano: 4 loudness layers × 30 notes (every third key), 22 MB of MP3

**Where the piano lives:** inside the plugin itself, in its `Contents/Resources/Salamander Grand` folder (a Mac plugin, and a
Windows VST3, is really a folder). So there is one thing to install and nothing can get separated, but the recordings stay
ordinary files: the build doesn't have to turn 22 MB of sound into program code, and a bigger piano can replace them later.

## Credits and licence

- The piano: **Salamander Grand Piano V3** (Yamaha C5) recorded by **Alexander Holm**, licensed
  [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/). The MP3 versions: Jan Forst's `@audio-samples/piano-mp3-*`
  packages (MIT). See `samples/Salamander Grand/`.
- The night picture is Steve Turner's own.
- Built with [JUCE](https://juce.com) 8, used under the AGPLv3; so this plugin's code is also AGPLv3 (see `LICENSE`).
  To sell HORIZON one day, the JUCE part would move to a JUCE commercial licence.
