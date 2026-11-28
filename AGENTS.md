# AGENTS.md

Orientation for coding agents working on this repository. User-facing docs are in `README.md`; build details, the
preset format and the full action API are in `DEVELOPERS.md` (read the relevant section before changing those areas).

## What this is

SOURCE is a sampler that loads sounds from Freesound. The active part of the repo is the **sampler engine and native
UI** in `SourceSampler/`: a JUCE 9 audio plugin (CMake) built as macOS AU/VST3/Standalone, Linux Standalone/VST3/LV2,
Windows Standalone/VST3, and an iPad Standalone + AUv3. Releases are on GitHub (`v1.x`, version in
`SourceSampler/CMakeLists.txt`).

Other top-level folders are older parts of the project (hardware unit on the Elk/Raspberry Pi platform, Push 2
controller, Python bindings): `elk_platform/`, `SourceShepherdPush2Controller/`, `pysource/`, `fabfile.py`,
`installer/`. They talk to the engine through the same action messages; don't break those (see "Actions" below).

## Layout (SourceSampler/)

- `Source/SourceSampler.{h,cpp}`: the sampler: state (`juce::ValueTree state`), presets, global settings, action
  handling (`actionListenerCallback`), Freesound queries, tempo.
- `Source/SourceSamplerSound.{h,cpp}`: `SourceSound` (a sound of the preset: parameters, MIDI CC / MPE mappings,
  modulators, loading/downloading) and `SourceSamplerSound` (one loaded sample in the synth).
- `Source/SourceSamplerVoice.{h,cpp}`: rendering, per-voice MPE overrides and modulators (LFO/ADSR/steps), state
  published for the UI (play position, modulator history).
- `Source/SourceSamplerSynthesiser.{h,cpp}`: the `juce::Synthesiser` (voices, MPE routing, current BPM).
- `Source/PluginProcessor.{h,cpp}`: plugin wrapper; picks the tempo source each block (Link > host > global BPM).
- `Source/SourceLink.{h,cpp}`: Ableton Link / Link Audio (iPad standalone only, `SOURCE_LINK`).
- `Source/SourceFxChain.*`, `FreesoundSearch.*`, `FreesoundAPI.*`, `DownloadPool.h`, `Tsne.h`: effects, search,
  Freesound API, background downloads, t-SNE for the search map.
- `Source/defines_source.h`: action names, IDs (`SourceIDs::`), defaults (`SourceDefaults::`).
- `Source/helpers_source.h`: state builders and pure helpers (note layouts, MPE curves, modulator math, sync divisions).
- `Source/SourceParameters.h` + `data_for_code_gen.csv` + `generate_code.py`: per-sound parameters are code-generated.
  To add/change one, edit the CSV and run `python generate_code.py -i` (it rewrites the "auto-generated code" blocks).
- `Source/UI/`: native editor. `NativeEditor` (header, tabs, General tab), `SoundEditorComponent` (sound parameters and
  the Modulators list: modulators + MPE/MIDI CC mappings with scopes), `SoundListComponent`, `SearchPanel` +
  `SoundMapComponent`, `FxPanel`, `SettingsPanel` (iPad: audio/MIDI and Link), `SourceLookAndFeel.h` (colours, cards,
  icon buttons).
- `Tests/`: JUCE unit tests (one executable, `SourceSamplerTests`). `SamplerRenderTests.cpp` renders audio through the
  real synth with a generated sine file; `HelpersTests.cpp` covers pure helpers.
- `3rdParty/`: submodules (JUCE, signalsmith-stretch, link, ...). `Builds/` and `SourceSampler.jucer` are legacy.

## Build and test (macOS)

```
git submodule update --init --recursive
cp SourceSampler/Source/api_key.example.h SourceSampler/Source/api_key.h   # add a Freesound API key (gitignored)

cd SourceSampler
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug          # once
cmake --build build --target SourceSamplerTests SourceSampler_Standalone
./build/SourceSamplerTests_artefacts/Debug/SourceSamplerTests  # ends with "ALL TESTS PASSED"
```

- Release build + tests + install of AU/VST3/app: `./build_and_install.sh`.
- ThreadSanitizer build dir: `build-tsan` (see `DEVELOPERS.md` for the flags). Run it after threading changes.
- iPad: `./build_ios.sh --simulator --install` (booted simulator) or
  `TEAM_ID=<team id> ./build_ios.sh --device --install` (connected or network-paired iPad). `MULTICAST=1` adds the
  multicast entitlement Link needs on a real iPad (requires Apple approval). Ask the user for the team ID.
- Linux/Windows: `linux/build_linux.sh`, `linux/build_in_docker.sh`, `windows/build_windows.sh`; CI builds them in
  `.github/workflows/linux-release.yml`. Releases: `make_release.sh` (see `DEVELOPERS.md`).
- Scripts must run on macOS's bash 3.2 (no `mapfile`, careful with empty arrays).

## Checking UI changes

- Debug standalone hooks (env vars, see `NativeEditor.cpp`): `SOURCE_DEBUG_SNAPSHOT=<png>` renders the editor to a
  PNG (with `_SIZE=WxH`, `_SCALE`, `_DELAY`), `SOURCE_DEBUG_TAB`, `SOURCE_DEBUG_LOAD_PRESET`, `SOURCE_DEBUG_ACTIONS`
  (run action messages), `SOURCE_DEBUG_ADD_FILE`, `SOURCE_DEBUG_SEARCH`, `SOURCE_DEBUG_SETTINGS_TAB=1`.
- The iPad simulator works well for interactive checks (landscape only; the app autosaves its session, so state
  survives reinstalls).

## Architecture rules (important)

- **State**: everything persistent lives in the `ValueTree` (`SourceSampler::state`: global settings, `PRESET`, its
  `SOUND`s with `SOUND_SAMPLE`, `MIDI_CC_MAPPING`, `MPE_MAPPING` and `MODULATOR` children). Presets are this tree saved
  as XML; keep the format backwards compatible (new properties need defaults; old presets from the hardware unit must
  still load).
- **Actions**: UIs (native, web, Python, hardware) change the sampler only through action messages
  (`"/action_name:arg1;arg2;..."`, constants in `defines_source.h`, handled in `SourceSampler::actionListenerCallback`).
  Add new arguments at the end and give missing ones defaults. Document new actions in `DEVELOPERS.md`.
- **Threads**: the state is only modified on the message thread. The audio thread never allocates, locks (only
  try-locks spin locks) or reads `CachedValue`s that the message thread writes without a copy: it uses snapshots
  (see `copyMpeMappings`/`copyModulators`) and atomics. Background work (loading, downloads, queries, stretching)
  posts results back to the message thread. The UI reads voice state only through atomics published by the voices
  (`getPublishedState`, modulator history, MPE values) via `SourceSampler::getVoicesInfo`/`forEachVoicePlayingSound`.
- **Modulation**: MIDI CC mappings set the sound's parameter value (all notes, saved in the preset). MPE mappings
  replace a parameter per note. Modulators (LFO/ADSR/steps) add an offset in the parameter's normalised range per
  voice (min = max = 0 means none). Modulators and mappings share a limit of 16 per sound. Voices of sounds with
  modulators render in 32-sample chunks.
- **Tempo**: synced LFOs/steps use `SourceSamplerSynthesiser::getCurrentBpm()`, set every block by the processor.

## Working conventions

- Add tests for engine changes (`Tests/`), run the full test executable before committing, and build the other
  affected targets (AU/VST3 for processor changes, the iPad build for `SOURCE_LINK`/iOS code).
- Work on a feature branch; the user asks explicitly before merging into `master` or pushing.
- Never commit `api_key.h`, tokens or other secrets. Never set GitHub secrets yourself (the user does it).
- Keep `DEVELOPERS.md` (and `README.md` for user-visible features) in sync with changes.
- Licensing: Link is GPL (fine for builds made from source; not App Store compatible as is).
