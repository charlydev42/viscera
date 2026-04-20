# CLAUDE.md — Parasite FM Synth

Read this first. It's the orientation pack for any Claude session picking up
work on this plugin. Everything below is load-bearing — if a section
contradicts older memory, CLAUDE.md wins.

## Project identity

- **Name**: Parasite (previously "Viscera" / "Blood Bucket 2"). Also ships a
  sister build called **Parasite Legacy** (different AU code, different bundle).
- **Brand**: Voidscan Audio (`voidscan-audio.com`). Old Thunderdolphin brand is
  decommissioning — don't add new references, update URLs you find.
- **Genre**: FM synth plugin (VST3 + AU + Standalone on macOS, VST3 + Standalone
  on Windows). Apple Silicon native, cross-compiled x86_64 via CMake.
- **Engine**: JUCE **8.0.4** via CMake FetchContent. Never downgrade: JUCE 7.0.9
  is broken on macOS 15+ (`CGWindowListCreateImage` deprecated).

## Build / install

```bash
# Configure once
cmake -B build -G Xcode

# Release — also copies the built VST3/AU into the user plugin folders
cmake --build build --config Release --target Parasite_VST3 Parasite_AU

# A single target for quick iterations
cmake --build build --config Release --target Parasite_VST3
```

Installed plugin paths after a Release build:
- `~/Library/Audio/Plug-Ins/VST3/Parasite.vst3`
- `~/Library/Audio/Plug-Ins/Components/Parasite.component`

Windows builds run via GitHub Actions (`.github/workflows/`) — the HMAC
implementation is cross-platform (RFC 2104 with `juce::SHA256`, no CommonCrypto).

## Signal flow (audio thread)

```
MIDI → FMVoice (per voice, up to 8)
       │
       ├─ Mod1 → Mod2 → Carrier → XOR → HemoFold → SVF → DC blocker
       │                                  ↑            ↑
       │                                  │            │
       │         Wavefolder amount ──────┘            │
       │         Filter cutoff + res ─────────────────┘
       │
       └─ output
             │
             ▼
PluginProcessor::processBlock (per block, after voice render)
  → VolumeShaper → Liquid chorus → Rubber comb → Delay → Reverb → Saturation
```

### Global LFO system
- **3 assignable LFOs** (`LFO1..3`), each with **8 routing slots**.
- APVTS params per LFO: `LFOn_RATE`, `LFOn_WAVE`, `LFOn_SYNC`, `LFOn_RETRIG`,
  `LFOn_VEL`, and for each slot `s` (1-8): `LFOn_DESTs`, `LFOn_AMTs`.
- LFOs run at **block rate** in `PluginProcessor::processBlock`, modulation
  sums accumulated into `VoiceParams.lfoMod*` atomics.
- `FMVoice::renderNextBlock` reads those atomics once per block and routes
  through a `SmoothedValue` per destination (5ms ramp) so the modulation is
  zipper-free per-sample.
- GUI: drag from `LFOWaveDisplay` → drop on a `ModSlider` knob to assign.
  LFOSection learn mode (clicking "+" then a knob) does the same thing via
  a different UX path.

### Curve editors (harmonics, shaper, LFO custom curve)
Undoable via Cmd+Z — three mechanisms:
- **Harmonics (MOD1/MOD2/CAR)**: 32 hidden `AudioParameterFloat` params per
  table (`MOD1_H00..31`, etc.). Bar edits go through `setValueNotifyingHost`,
  the `CurveListener` in `PluginProcessor` propagates to the `HarmonicTable`.
- **Shaper**: same pattern, 32 `SHAPER_S00..31` hidden params.
- **LFO custom curves**: variable-length control points; undo via a bespoke
  `LFOCurveEdit : juce::UndoableAction` (in `Source/gui/LFOSection.cpp`) that
  snapshots the full `std::vector<CurvePoint>` before/after and calls
  `LFO::setCurvePoints` in perform/undo.

## Directory map

- `Source/PluginProcessor.{h,cpp}` — APVTS, voice pool, effects chain, state
  save/restore with schema migrations, license/cloud wiring.
- `Source/PluginEditor.{h,cpp}` — top-level UI, dark-mode toggle & cross-
  instance propagation, error toast, `ModSliderContextProvider`.
- `Source/dsp/` — pure-C++ DSP classes (`FMVoice`, `LFO`, `VolumeShaper`,
  `HarmonicTable`, `SVFilter`, `DCBlocker`, `XOR`, `AudioVisualBuffer`, …).
- `Source/gui/` — JUCE components: section panels, editors, overlays, theme.
- `Source/license/` — `LicenseManager` + `LicenseConfig.h` + obfuscated secret.
- `Source/cloud/` — `CloudPresetManager` (HTTP + retry queue + sync).
- `Source/util/Logger.{h,cpp}` — `bb::Logger` singleton + crash handler.

## Architectural rules (honor these — they're load-bearing)

1. **Audio thread is real-time**. No locks, no heap allocs, no `juce::String`,
   no `std::function` copies in `processBlock` or `renderNextBlock`. Every
   param read is `atomic<float>*->load()`.
2. **Continuous params on the audio thread go through `SmoothedValue`**. Block-
   rate steps = zipper noise on automation. See `FMVoice::prepareToPlay` for
   the established `smoothX` / `smoothGLfoX` pattern.
3. **State is versioned.** Every save writes `schemaVersion` via
   `ParasiteProcessor::kCurrentSchemaVersion`. Every load runs
   `applyStateMigrations` which chains migrators by `fromVersion`. If you rename
   a param or change a range, **add a v(n)→v(n+1) migrator** and bump
   `kCurrentSchemaVersion`. Never break old presets silently.
4. **Cross-instance state is explicit.** Two instances of Parasite in a DAW
   must not interfere. If you add new per-instance GUI state, put it on
   `ModSliderContext` (discovered via `findParentComponentOfClass<
   ModSliderContextProvider>`) — do NOT use `static inline`. Static state for
   this plugin means "shared across all instances in the process", and that's
   only acceptable for immutable constants.
5. **Shared preference files are atomic.** Any write to `favorites.txt`,
   `ui_prefs.txt`, user preset XML, or cloud cache JSON goes through
   `juce::TemporaryFile` + `overwriteTargetFileWithTemporary()` (atomic rename
   on POSIX). Multiple instances will race otherwise.
6. **License enforces 7-day offline grace** (`kOfflineGraceMs` in
   `LicenseConfig.h`) and has clock-skew protection (if `lastVerified_` is in
   the future, revoke). Don't soften either without a good reason.
7. **Cloud is best-effort with a persistent retry queue.** Failed uploads land
   in `~/Library/Application Support/Voidscan/Parasite/cloud_pending.json` and
   drain after the next successful sync. Use `httpRequestWithRetries` not
   `httpRequest` directly.
8. **Every load path validates.** `parseXML` null check, root-tag check, log
   via `BB_LOG_ERROR`, set `processor.setLastLoadError(...)`, preserve current
   state on any failure. Editor's timer polls the error and shows a toast.

## Rituals before declaring done

Fast path (code change that doesn't touch DSP / state / cloud):
```bash
cmake --build build --config Release --target Parasite_VST3
```

Full path (anything touching DSP, param layout, state, cloud, or license):
```bash
cmake --build build --config Release --target Parasite_VST3 Parasite_AU

# Validate
/tmp/pluginval.app/Contents/MacOS/pluginval --strictness-level 7 \
  --validate ~/Library/Audio/Plug-Ins/VST3/Parasite.vst3
auval -v aumu VsPa VdSn
```

`pluginval --strictness-level 10` flags 2-3 `AudioParameterBool` restore tests
that are known pluginval quirks (not real bugs). Strictness 7 is the industry
bar — it must PASS. auval must PASS.

## Observability

Logs: `~/Library/Logs/Parasite/parasite.log` (rotating, 1 MiB × 5). Crash
dumps land beside as `crash-YYYYMMDD-HHMMSS.log`. First thing to ask for when
debugging a user report: "Please send `~/Library/Logs/Parasite/` contents."

Useful log macros:
```cpp
#include "util/Logger.h"
BB_LOG_INFO ("Short event message");
BB_LOG_WARN ("Transient failure — " + context);
BB_LOG_ERROR("Hard failure — " + reason);
```

## AU / VST3 codes

- Voidscan brand (current): `aumu VsPa VdSn` / VST3 bundle id
  `com.voidscanaudio.parasite`.
- Legacy build ("Parasite Legacy"): separate bundle id; keeps old user presets
  loadable side-by-side during the brand transition.

## Backend (see also memory/project_backend.md)

- Prod: `https://api.voidscan-audio.com` (Contabo VPS, Cloudflare proxied)
- Local repo: `/Volumes/Sanctus SSD/Global Server/Code/voidscan-backend/`
- Stack: Fastify + Prisma + PostgreSQL, PM2, Nginx reverse proxy, Certbot SSL.
- Plugin C++ client points at the URL via `LicenseConfig.h::kApiBaseUrl`.

## Known gotchas

- **IDE "juce not found" diagnostics are false positives.** The clang linter
  doesn't know JUCE headers are in the build dir. Only real errors are
  compiler errors from `cmake --build`.
- **SineTable cannot be constexpr with `std::sin`.** Use a Meyers singleton
  instead (`static const SineTable& sineTable() { static SineTable t; return t; }`).
- **JUCE 7.0.9 is broken on macOS 15+** (`CGWindowListCreateImage` obsoleted).
  We're on 8.0.4 — keep it there or newer.
- **Multiple plugin instances**: the LaF global default was a prior bug source.
  Always clear it only if we're still the default (`&getDefault() == &lookAndFeel`).
  Similarly, the FlubberVisualizer is per-instance but listens to shared dark
  mode; don't add new shared mutable state.
- **APVTS `replaceState` fires param listeners.** `CurveListener` has a
  `suppressCurveListener` atomic that MUST be raised around any
  `apvts.replaceState(tree)` call, or the listener writes stale H##/S## values
  back into the authoritative `HarmonicTable` / `VolumeShaper` state. See
  `setStateInformation` / `loadUserPreset` / `loadPresetFromXml` for the idiom.

## When adding a new APVTS param

1. Add the `AudioParameter*` in `createParameterLayout()`.
2. If it's continuous and read per-sample on the audio thread, add a matching
   `SmoothedValue` and a `setTargetValue` in the per-block setup of
   `FMVoice::renderNextBlock` (or the effects chain).
3. Cache the raw pointer in `cacheParameterPointers()` if read often.
4. If the param affects stored DSP state that isn't the param itself (curves,
   custom tables), extend `serializeCustomData` / `deserializeCustomData`.
5. Bump `kCurrentSchemaVersion` and add a migrator that injects a sensible
   default for presets that predate the param.
6. Rebuild + run `pluginval --strictness-level 7` + `auval -v`.

## When adding a new GUI section

1. Create `Source/gui/MySection.{h,cpp}` following the pattern of existing
   sections (constructor takes `APVTS&`, owns attachments, stops its timer in
   the destructor).
2. Add the `.cpp` to `CMakeLists.txt` under `target_sources(Parasite PRIVATE …)`.
3. If the section uses `ModSlider`, the parent chain already provides
   `ModSliderContext` — no extra wiring needed. ModSlider auto-resolves via
   `parentHierarchyChanged`.
4. For any extension that binds state across instances (like LFOSection did
   for learn mode), extend `ModSliderContext` — don't add new statics.

## Things we don't do (yet)

- We do NOT attempt to pass `pluginval --strictness-level 10`. Known-quirk
  `AudioParameterBool` failures aren't a real ship-blocker at that level.
- We do NOT currently have tooltips on knobs. If you add them, make sure the
  string tables are static `const` so they don't allocate per frame.
- We do NOT have an in-app update check. Version info lives in logs and
  crash dumps only.
- Accessibility (screen reader, keyboard nav) is **not** wired up. Deliberate
  deferral until we ship v1 to paying customers.

## Style / tone

- The user (Charly, `charlygmn@gmail.com`) writes in French; match their
  language. Technical identifiers stay English.
- Prefer surgical fixes over refactors. If you MUST refactor, plan it first
  and surface the plan before touching code.
- Always build + verify after non-trivial changes. Don't declare done on a
  dirty diff.
- Log files and state migrations are the two things users notice when they
  go wrong — be careful with both.
