# Architectural Map: Integrating Atari ST / YM2149 Mode into ft2-clone

---

## 1. Replayer Architecture: `tickReplayer()` Pipeline

### Entry Point and Call Site

`tickReplayer()` is declared in `ft2_replayer.h` and defined at `ft2_replayer.c:2362`. It is called from two places:

- **SDL audio callback** (`ft2_audio.c:857`): `audioCallback()` calls it once per tick boundary inside the sample-generation loop.
- **WAV renderer** (`ft2_wav_renderer.c:306`): `dump_TickReplayer()` calls it once per tick on a background thread.

### Tick Counter and Advance Logic

```c
// ft2_replayer.c:2387-2391
if (--song.tick == 0) {
    song.tick = song.speed;
    tickZero = true;
}
```

`song.tick` counts down from `song.speed` to 1. When it reaches 0 it is reloaded with `song.speed` and `tickZero` is set. This maps to one "row" of note data every `song.speed` ticks.

### Tick-Zero Path (new row)

When `tickZero && (song.pattDelTime2 == 0)` (`ft2_replayer.c:2395`):

1. **Fetch pattern row**: pointer `p = &pattern[song.pattNum][song.row * MAX_CHANNELS]` (`ft2_replayer.c:2406`). Patterns are stored flat: `note_t pattern[MAX_PATTERNS][MAX_PATT_LEN * MAX_CHANNELS]`. Each row occupies `MAX_CHANNELS` (32) `note_t` entries regardless of `song.numChannels`.

2. **`getNewNote(ch, p)`** (`ft2_replayer.c:1350`): For each channel:
   - Copies `vol`, `efx`, `efxData` into `ch`.
   - Detects portamento (`efx == 3/5`, vol-col `0xFx`): calls `preparePortamento()` which resolves target period via `note2PeriodLUT`.
   - If a plain note: calls `triggerNote()` → looks up `ins->note2SampleLUT[note-1]`, sets `ch->outPeriod = note2PeriodLUT[noteIndex]` (`ft2_replayer.c:592`), sets `ch->status |= CF_UPDATE_PERIOD | CS_TRIGGER_VOICE | CS_UPDATE_VOL | CS_UPDATE_PAN`.
   - Calls `handleEffects_TickZero(ch)`: dispatches volume-column effect via `VJumpTab_TickZero`, then dispatches effect column through `JumpTab_TickZero` (B, D, E, F, G, L at tick 0).

3. **`updateVolPanAutoVib(ch)`** (`ft2_replayer.c:1458`): Processes volume/pan envelopes (with interpolation), fadeout on key-off, auto-vibrato, and calculates `ch->fFinalVol` (a float 0.0–1.0), `ch->finalPan`, and `ch->finalPeriod`.

### Non-Zero Tick Path

`handleEffects_TickNonZero(ch)` (`ft2_replayer.c:2275`): dispatches via `VJumpTab_TickNonZero` (volume column slides/vibrato) and `JumpTab_TickNonZero` (0=arpeggio, 1=pitchSlideUp, 2=pitchSlideDown, 3=portamento, 4=vibrato, 5=portamento+volslide, 6=vibrato+volslide, 7=tremolo, A=volSlide, H=globalVolSlide, K=keyOff, P=panSlide, R=multiRetrig, T=tremor, E=sub-effects). Then `updateVolPanAutoVib(ch)`.

### `getNextPos()` (`ft2_replayer.c:2290`)

Advances `song.row`, handles `pattDelTime`, `pBreakFlag`, `posJumpFlag`, and song-loop wrapping.

### Period-to-Voice Conversion

After `tickReplayer()` the audio system calls `updateVoices()` (`ft2_audio.c:359`). For each channel with `status != 0`:
- `CF_UPDATE_PERIOD` → `v->delta = period2VoiceDelta(ch->finalPeriod)` (a 64-bit fixed-point step per sample).
- `CS_TRIGGER_VOICE` → `voiceTrigger(i, ch->smpPtr, ch->smpStartPos)` sets up the `voice_t` with base/loopStart/loopEnd etc.
- `CS_UPDATE_VOL/PAN` → `voiceUpdateVolumes(i, status)` sets `v->fTargetVolumeL/R` from `ch->fFinalVol` and `fSqrtPanningTable[panning]`.

> **Critical insight for Atari mode**: The YM replayer only needs to intercept *after* `tickReplayer()` has fully computed `ch->finalPeriod`, `ch->fFinalVol`, `ch->outVol`, `ch->noteNum`, `ch->instrNum`, and `ch->status`. It does NOT need to call `updateVoices()` or any mixer code.

---

## 2. Audio Pipeline: SDL → Mixer → WAV Renderer

### SDL Callback (`ft2_audio.c:830`)

```c
static void audioCallback(void *userdata, Uint8 *stream, int len) {
    // skips if editor.wavIsRendering
    while (samplesLeft > 0) {
        if (audio.tickSampleCounter == 0) {  // new tick boundary
            tickReplayer();
            updateVoices();
            fillVisualsSyncBuffer();
            // reload tickSampleCounter from BPM tables
        }
        doChannelMixing(bufferPosition, samplesToMix);
        bufferPosition += samplesToMix;
    }
    sendSamples16BitStereo(stream, len); // or 32-bit float
}
```

- `audio.tickSampleCounter` starts at `audio.samplesPerTickInt` (from `calcReplayerVars()` at `ft2_replayer.c:465`, which precomputes `audioFreq / (bpm/2.5)` for all BPM values).
- The fractional accumulator `audio.tickSampleCounterFrac` compensates for the non-integer samples-per-tick.

### Mixer (`ft2_audio.c:474`)

`doChannelMixing()` iterates `voice[]` and calls `mixFuncTab[offset](v, bufferPosition, samplesToMix)` — the function table is indexed by `(volRamp * mixOffsetBias) + voice.mixFuncOffset` where `mixFuncOffset = sample16Bit*15 + interpolationType*3 + loopType`.

The float mixing buffers `audio.fMixBufferL/R` accumulate per-sample data across all voices; `sendSamples*()` then normalizes, dithers (16-bit) and writes.

### WAV Renderer (`ft2_wav_renderer.c:330`)

`renderWavThread()` follows an identical tick loop:
```c
dump_TickReplayer();          // calls tickReplayer() + updateVoices()
mixReplayerTickToBuffer(tickSamples, ptr8, bitDepth);  // doChannelMixing → sendSamples → clears mix buffer
fwrite(ptr8, ...);
```
`mixReplayerTickToBuffer` (`ft2_audio.c:498`) is the bridge: it calls `doChannelMixing()` then `sendSamples*BitStereo()` to write directly into a caller-supplied buffer.

> **YM export template**: Replace `mixReplayerTickToBuffer()` with a `captureYMRegisters()` that, after `tickReplayer()`, reads `ch->finalPeriod`, `ch->outVol`, `ch->instrNum`, and channel-status flags from the 3 PSG channels and writes a 14-byte YM register frame (R0–R13).

---

## 3. Module Save System

### Save Thread (`ft2_module_saver.c:648`)

```c
static int32_t saveMusicThread(void *ptr) {
    pauseAudio();
    if (editor.moduleSaveMode == 1)     // MOD_SAVE_MODE_XM
        saveXM(editor.tmpFilenameU);
    else
        saveMOD(editor.tmpFilenameU);
    resumeAudio();
    return true;
}
```

`saveMusic()` (`ft2_module_saver.c:667`) copies the filename into `editor.tmpFilenameU`, then spawns `saveMusicThread` via `SDL_CreateThread` + `SDL_DetachThread`.

### Save Mode Selection

`editor.moduleSaveMode` (`ft2_structs.h:34`) is a `uint8_t` currently taking values:
- `MOD_SAVE_MODE_MOD = 0` (`ft2_diskop.h:16`)
- `MOD_SAVE_MODE_XM = 1` (`ft2_diskop.h:17`)
- `MOD_SAVE_MODE_WAV = 2` (`ft2_diskop.h:18`)

The disk op UI at `ft2_diskop.c:2044` clamps to ≤3 and maps to radiobuttons `RB_DISKOP_MOD_SAVEAS_MOD + moduleSaveMode`. A new value `MOD_SAVE_MODE_YM = 3` and `MOD_SAVE_MODE_SNDH = 4` could extend this enum.

The corresponding radiobuttons are `RB_DISKOP_MOD_SAVEAS_MOD`, `RB_DISKOP_MOD_SAVEAS_XM`, `RB_DISKOP_MOD_SAVEAS_WAV` in `ft2_radiobuttons.h:146–148`. Two more entries would be needed in the enum (before `NUM_RADIOBUTTONS`) and corresponding `radioButton_t` definitions in `ft2_radiobuttons.c`.

The `saveMusicThread` switch would add:
```c
else if (editor.moduleSaveMode == MOD_SAVE_MODE_YM)
    saveYM(editor.tmpFilenameU);
else if (editor.moduleSaveMode == MOD_SAVE_MODE_SNDH)
    saveSNDH(editor.tmpFilenameU);
```

### XM Save Structure (for comparison)

`saveXM()` (`ft2_module_saver.c:31`) writes:
1. `xmHdr_t` (packed) — song metadata + `orders[256]`.
2. Per-pattern: `xmPatHdr_t` + run-length packed `note_t` via `packPatt()`.
3. Per-instrument: `xmInsHdr_t` (with envelope, vib, fadeout data) + raw sample data after `samp2Delta()`.

---

## 4. Instrument System

### `instr_t` (`ft2_replayer.h:222–235`)

```c
typedef struct instr_t {
    bool midiOn, mute;
    uint8_t midiChannel, note2SampleLUT[96];   // note → sample index (0-based)
    uint8_t volEnvLength, panEnvLength;
    uint8_t volEnvSustain, volEnvLoopStart, volEnvLoopEnd;
    uint8_t panEnvSustain, panEnvLoopStart, panEnvLoopEnd;
    uint8_t volEnvFlags, panEnvFlags;           // ENV_ENABLED | ENV_SUSTAIN | ENV_LOOP
    uint8_t autoVibType, autoVibSweep, autoVibDepth, autoVibRate;
    uint16_t fadeout;
    int16_t volEnvPoints[12][2], panEnvPoints[12][2]; // [x,y] pairs, up to 12 points
    int16_t midiProgram, midiBend;
    int16_t numSamples;
    sample_t smp[16];                          // up to 16 samples per instrument
} instr_t;
```

All 128 instruments live in `instr[128+4]` (`ft2_replayer.c:50`); indices 0, 130, 131 are reserved for special placeholder instruments.

### `sample_t` (`ft2_replayer.h:207–220`)

```c
typedef struct sample_t {
    char name[22+1];
    bool isFixed;
    int8_t finetune, relativeNote, *dataPtr, *origDataPtr;
    uint8_t volume, flags, panning;  // flags = SAMPLE_16BIT|SAMPLE_STEREO|SAMPLE_ADPCM
    int32_t length, loopStart, loopLength;
    int8_t leftEdgeTapSamples8[MAX_TAPS*2];    // for interpolation
    int16_t leftEdgeTapSamples16[MAX_TAPS*2];
    int16_t fixedSmp[MAX_TAPS*2];
    int32_t fixedPos;
} sample_t;
```

### Volume Envelope Processing (`ft2_replayer.c:1488–1606`)

The volume envelope runs in `updateVolPanAutoVib()`. At each tick:
1. Increment `ch->volEnvTick`. When it reaches a point's X-coordinate, advance `ch->volEnvPos`.
2. Handle sustain (hold at sustain point) and loop (jump back to loop start).
3. Interpolate `ch->volEnvValue` between points using `ch->volEnvDelta = (yDiff << 8) / xDiff`.
4. Final `ch->fFinalVol = (globalVol * outVol * fadeoutVol) / (64*64*32768) * envVal / (64*256)`.

### Instrument Editor (`ft2_inst_ed.c`)

The editor provides:
- Volume envelope and pan envelope point editors (drag points, add/delete points).
- Per-sample volume, finetune, relative note, panning.
- Auto-vibrato (type/sweep/depth/rate).
- MIDI extension (channel, program, bend range).

### YM Instrument Mapping

For PSG instruments, the existing `instr_t` structure cannot map directly. A PSG instrument needs:
- **Arpeggio table**: a sequence of semitone offsets (up to 16 steps, loopable).
- **Pitch envelope**: 16-step envelope over pitch in semitones.
- **Volume envelope**: 4-bit (0–15) stepped envelope (not interpolated).
- **Hardware envelope**: period (0–0xFFFF, 3 bytes), shape (4 bits), on/off flag.
- **Noise/tone mix**: per-channel flags for tone/noise enabled.
- **Noise period**: 0–31 per instrument.

The simplest approach is to store this in a custom data structure pointed to by an unused field in `instr_t` (e.g., hijacking `midiProgram`/`midiBend` for flags), or to create a parallel `psgInstr_t` array indexed by instrument number and guarded by an Atari-mode flag.

---

## 5. Pattern Editor Constraints

### Channel Count

`song.numChannels` (`ft2_replayer.h:269`) is the live channel count. It is incremented/decremented by 2 via `pbAddChan()` / `pbSubChan()` (`ft2_pattern_ed.c:1837–1875`), clamped to 2–32. The lower bound is 4 for subtract (`ft2_pattern_ed.c:1858`), but could be lowered to 2, then further constrained to 3 in Atari mode.

Since pattern data always uses `MAX_CHANNELS=32` columns (`note_t pattern[MAX_PATTERNS][MAX_PATT_LEN * MAX_CHANNELS]`), limiting to 3 channels is purely a matter of:
- Blocking `pbAddChan`/`pbSubChan` in Atari mode.
- Setting `song.numChannels = 3` on mode entry.
- Displaying only 3 columns in the pattern editor.

### `note_t` (`ft2_replayer.h:183–190`)

```c
typedef struct pattNote_t {
    uint8_t note, instr, vol, efx, efxData;  // 5 bytes, packed
} note_t;
```

The `note` field is 1–96 (or 97 for NOTE_OFF). In Atari mode, channel-specific PSG attributes (noise period, mixer flags) could be placed in the `vol` (volume column) field or in `efxData` using new effect codes.

### Effect Display (`ft2_pattern_ed.c`)

Effects are displayed as hex characters per `note_t.efx` (0–35 maps to 0–9, A–Z). The display code in `ft2_pattern_draw.c` uses lookup tables — adding new YM-specific effect codes (e.g., noise period, mixer control, hardware envelope trigger) requires inserting them into the existing effect-code space (0–35 is already used) or defining new codes > 35 for the Atari mode only, recognized by the YM replayer but ignored in normal mode.

---

## 6. UI/Mode System

### Screen Show/Hide Pattern

All screens follow the same pattern — `ft2_gui.c` has `showTopScreen()`/`hideTopScreen()`, and per-screen functions like `showWavRenderer()` / `hideWavRenderer()` in `ft2_wav_renderer.c:149–184`:

```c
void showWavRenderer(void) {
    hideTopScreen();
    showTopScreen(false);
    ui.wavRendererShown = true;
    ui.scopesShown = false;
    drawWavRenderer();
}
void hideWavRenderer(void) {
    ui.wavRendererShown = false;
    hidePushButton(PB_WAV_RENDER); // repeat for all buttons
    hideRadioButtonGroup(RB_GROUP_WAV_RENDER_BITDEPTH);
    hideCheckBox(CB_WAV_RENDER_INDIVIDUAL_TRACKS);
    ui.scopesShown = true;
    drawScopeFramework();
}
```

A new **Atari Export Panel** would follow this identical pattern:
1. Add `bool atariExportShown` to `ui_t` (`ft2_structs.h`).
2. Add push-button IDs to `ft2_pushbuttons.h` (after `PB_WAV_END_DOWN`, before `NUM_PUSHBUTTONS`).
3. Add radiobutton IDs for format selection (.YM / .SNDH) to `ft2_radiobuttons.h`.
4. Implement `showAtariExport()`, `hideAtariExport()`, `drawAtariExport()`.

### Push Buttons (`ft2_pushbuttons.h:402–413`)

Each `pushButton_t` has `{x, y, w, h, preDelay, delayFrames, caption, caption2, callbackFuncOnDown, callbackFuncOnUp, state, bitmapFlag, visible, ...}`. They are registered in a global array in `ft2_pushbuttons.c` and drawn/tested by coordinate tests.

### Radio Buttons (`ft2_radiobuttons.h:213–221`)

Each `radioButton_t` has `{x, y, clickAreaWidth, group, callbackFunc, state, visible}`. Groups are defined as enum values above `NUM_RADIOBUTTONS`. One group (e.g., `RB_GROUP_ATARI_FORMAT`) would hold `RB_ATARI_FORMAT_YM` and `RB_ATARI_FORMAT_SNDH`.

### Disk Op Save Modes (`ft2_diskop.c:2040–2050`)

The module save mode is selected by radiobuttons `RB_DISKOP_MOD_SAVEAS_*`. Adding new save modes requires:
1. New enum values in `ft2_diskop.h` (`MOD_SAVE_MODE_YM = 3`, `MOD_SAVE_MODE_SNDH = 4`).
2. New entries in `ft2_radiobuttons.h` in `RB_GROUP_DISKOP_MOD_SAVEAS`.
3. Updated `ft2_diskop.c:2044` clamp and initialization logic.
4. Extension callback in the file manager's save dispatch at `ft2_diskop.c:898`.

---

## 7. Configuration System

### Config File Structure (`ft2_config.h:106–162`)

`config_t` is a packed struct exactly matching the FT2.CFG binary layout (size `CONFIG_FILE_SIZE = 1736` bytes). Most fields map to original FT2 config file byte offsets. The file is loaded/saved verbatim by `loadConfig()` / `saveConfig()` (`ft2_config.c`).

**Problem**: The struct is already at fixed positions. Adding new Atari mode preferences cannot extend it without breaking compatibility with existing config files. The options are:
1. Use the `uint8_t` free bits in `specialFlags` (bit 6 is noted as "free for use" in `ft2_config.h:68`).
2. Write a separate `atari.cfg` file using a parallel configuration structure.
3. Use `specialFlags2` which already holds `HARDWARE_MOUSE`, `STRETCH_IMAGE`, etc.

### Relevant Existing Fields

- `config.interpolation` — audio interpolation type (reused as a model).
- `config.specialFlags` — bit flags: `NO_VOLRAMP_FLAG=1`, `BITDEPTH_16=2`, `BITDEPTH_32=4`, `BUFFSIZE_512=8`, `BUFFSIZE_1024=16`, `BUFFSIZE_2048=32`, **bit 6 free**, `LINED_SCOPES=128`.
- `config.boostLevel` — used by WAV renderer as amp; could also be model for YM amp.

### Recommended Approach

Create a separate `atari_config_t` struct saved as a sidecar file (e.g., `FT2-ATARI.CFG`) with fields:
```c
typedef struct atariConfig_t {
    bool atariMode;              // global Atari PSG mode toggle
    uint8_t ymExportFormat;      // 0=YM5, 1=YM6, 2=SNDH
    uint16_t ymTimerFreq;        // 50 Hz = 50, 60 Hz = 60
    bool ymDigiDrums;            // enable digi-drum samples in SNDH
    char sndhAuthor[32];
    char sndhTitle[32];
} atariConfig_t;
```

---

## 8. Effect Processing: XM → YM2149 Mapping

### Complete Effect Tables

**Tick-zero effects** (`JumpTab_TickZero`, `ft2_replayer.c:986–1024`): B (position jump), D (pattern break), Exx (sub-effects), F (set speed/BPM), G (global vol), L (envelope pos).

**E sub-effects tick-zero** (`EJumpTab_TickZero`, `ft2_replayer.c:734–752`): E1x (fine pitch up), E2x (fine pitch down), E3x (portamento ctrl), E4x (vibrato ctrl), E6x (pattern loop), E7x (tremolo ctrl), EAx (fine vol up), EBx (fine vol down), ECx (note cut tick 0), EEx (pattern delay).

**Non-zero tick effects** (`JumpTab_TickNonZero`, `ft2_replayer.c:2235–2273`): 0 (arpeggio), 1 (pitch up), 2 (pitch down), 3 (portamento), 4 (vibrato), 5 (port+vol), 6 (vib+vol), 7 (tremolo), A (vol slide), H (global vol slide), K (key off), P (pan slide), R (multi-retrig), T (tremor), E9x (retrig), ECx (note cut), EDx (note delay).

**Volume column effects** (`VJumpTab_TickZero`, `ft2_replayer.c:1160–1165`): 0x10–0x50 set volume, 0x60–0x6F fine vol down, 0x70–0x7F fine vol up, 0x80–0x8F set vibrato speed, 0x90–0x9F N/A, 0xA0–0xAF vibrato, 0xB0–0xBF set pan, 0xC0–0xCF pan slide left, 0xD0–0xDF pan slide right, 0xF0–0xFF portamento.

### YM2149 Mapping Analysis

| XM Effect | YM2149 Mapping | Notes |
|-----------|----------------|-------|
| **0xx (Arpeggio)** | ✅ Direct: write alternate tone periods for 3 sub-ticks per tick | Period computed via `period2NotePeriod()` which already exists |
| **1xx (Pitch up)** | ✅ Direct: decrement tone period register by `param*4` per tick | YM period = `fMasterClock / (2 * 16 * freq)` |
| **2xx (Pitch down)** | ✅ Direct: increment tone period register | Same as above |
| **3xx (Portamento)** | ✅ Direct: slide tone period toward target | Retarget each tick |
| **4xx (Vibrato)** | ✅ Direct: modulate period with sine LUT | Existing `doVibrato()` computes `outPeriod` — just feed to YM register |
| **5xx (Port+VolSlide)** | ✅ Direct | Combine 3xx + Axx |
| **6xx (Vib+VolSlide)** | ✅ Direct | Combine 4xx + Axx |
| **7xx (Tremolo)** | ✅ Direct: modulate YM volume register | 4-bit output |
| **9xx (Sample Offset)** | ❌ N/A | No sample playback |
| **Axx (Vol slide)** | ✅ Direct: modify `ch->outVol`, clamp to 0–15 (4-bit) | Needs scale from 0–64 to 0–15 |
| **Bxx (Position jump)** | ✅ No change | Same song structure |
| **Cxx (Set volume)** | ✅ Direct with scaling | 0–64 → 0–15 |
| **Dxx (Pattern break)** | ✅ No change | Same song structure |
| **Exx sub-effects** | Mostly direct | E1x/E2x/E3x/E4x/E6x/EAx/EBx/ECx/EDx/EEx all translate |
| **Fxx (Speed/BPM)** | ✅ Direct | BPM sets tick frequency; YM uses 50Hz or tick-rate |
| **Gxx (Global vol)** | ✅ Scale to 0–15 | Applied as multiplier to all channel volumes |
| **Hxx (Global vol slide)** | ✅ With scaling | |
| **Kxx (Key off)** | ✅ Stop tone/noise for channel | |
| **Lxx (Envelope pos)** | ❌ Different semantics | YM hardware env is separate |
| **Pxx (Pan slide)** | ❌ YM has no panning | Could be ignored or repurposed for noise period |
| **Rxx (Multi-retrig)** | ⚠️ Partial | Retrigger works; volume modulation limited to 4-bit |
| **Txx (Tremor)** | ✅ Direct | On/off volume by tick count |
| **Vol col vibrato** | ✅ Direct | |

### YM2149-Specific Effects (New Codes Needed)

Since effects 0–35 are fully used (0–9, A–Z), new PSG-specific effects need new numeric codes beyond 35, or a re-mapping within an Atari-specific effect set.

Indices 18 (`I`), 19 (`J`), 22 (`M`), 23 (`N`), 28 (`S`), 30 (`U`) are all currently `dummy` in both `JumpTab_TickZero` and `JumpTab_TickNonZero` — available for repurposing:

| YM Effect | Code | Index | Description |
|-----------|------|-------|-------------|
| Set noise period | `Ixx` | 18 | Low 5 bits = noise period (0–31) |
| Set mixer (tone/noise) | `Jxx` | 19 | Low nibble = tone enable, high = noise enable |
| Trigger HW envelope | `Mxx` | 22 | Hardware envelope shape in low nibble |
| HW envelope period lo | `Nxx` | 23 | Low byte of 16-bit HW env period |
| HW envelope period hi | `Sxx` | 28 | High byte of 16-bit HW env period |
| Buzzer (PWM voice) | `Uxx` | 30 | AY "buzzer" mode with period modulation |

---

## 9. Complete Integration Architecture

### Component Map

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ft2-clone Core                              │
│                                                                     │
│  song_t / channel_t[32] / instr_t[128] / note_t patterns[]         │
│  ↓ tickReplayer() [ft2_replayer.c:2362]                             │
│    ├── getNewNote() → triggerNote() → ch->outPeriod, ch->status     │
│    ├── handleEffects_TickZero/NonZero()                             │
│    └── updateVolPanAutoVib() → ch->fFinalVol, ch->finalPeriod       │
│                                                                     │
│  Normal mode:           │  Atari mode (NEW):                        │
│  updateVoices()         │  captureYMRegisters()                     │
│  doChannelMixing()      │  write YM R0-R13 frame                    │
│  sendSamples*()  → SDL  │  append to .YM buffer / SNDH data         │
└─────────────────────────────────────────────────────────────────────┘
```

### New Files to Create

| File | Purpose |
|------|---------|
| `src/ft2_ym2149.c/h` | YM2149 register emulator: maps `ch->finalPeriod` → tone registers, `ch->outVol` → amplitude register |
| `src/ft2_ym_export.c/h` | YM file writer: ring-buffer of 14-byte register frames, writes .YM5/.YM6 header + LHA-compressed body |
| `src/ft2_sndh_export.c/h` | SNDH writer: 68000 replay code stubs, compact song data, SNDH header |
| `src/ft2_psg_instr_ed.c/h` | PSG instrument editor UI: noise period, mixer control, arpeggio table, 4-bit volume envelope |
| `src/ft2_atari_mode.c/h` | Mode toggle, channel constraint, effect remapping |

### Key Integration Points

1. **`ft2_audio.c:audioCallback()`** — add `if (atariMode) captureYMFrame(); else { updateVoices(); doChannelMixing(); }` after `tickReplayer()`.

2. **`ft2_wav_renderer.c:dump_TickReplayer()`** — mirror of above for offline export. The existing thread architecture (pauseAudio + background SDL_Thread) is directly reusable for YM/SNDH export.

3. **`ft2_module_saver.c:saveMusicThread()`** — add cases for `MOD_SAVE_MODE_YM` and `MOD_SAVE_MODE_SNDH`.

4. **`ft2_diskop.h/c`** — add `MOD_SAVE_MODE_YM=3`, `MOD_SAVE_MODE_SNDH=4`, two new `RB_DISKOP_MOD_SAVEAS_*` entries.

5. **`ft2_replayer.c:tickReplayer()`** — add Atari-specific dummy handling for `I`, `J`, `M`, `N`, `S`, `U` effects in the jump tables (they are already `dummy` — just replace with `setNoisePeriod`, `setMixer`, etc.).

6. **`ft2_replayer.h:channel_t`** — add PSG-specific state: `uint8_t noisePeriod`, `uint8_t mixerFlags`, `uint16_t hwEnvPeriod`, `uint8_t hwEnvShape` (or store in a parallel `psgChannel_t` array).

7. **`ft2_pattern_ed.c:pbAddChan()/pbSubChan()`** — check an `atariMode` flag and block channel count changes; lock to exactly 3 channels.

8. **`ft2_structs.h:editor_t`** — add `bool atariMode`, `uint8_t ymExportFormat`.

9. **`ft2_structs.h:ui_t`** — add `bool atariExportShown`.

10. **`ft2_config.h/c`** — add sidecar config file handling for `atariConfig_t`.

### Period-to-YM-Register Conversion

The YM2149 tone period register is:
```
YM_period = fMasterClock / (2 * 16 * freq_Hz)
```
where `fMasterClock` = 2MHz (ST), 1.75MHz (MSX), or 2.457MHz (CPC).

Given `ch->finalPeriod` (XM linear or Amiga period), convert via `dPeriod2Hz()` (`ft2_replayer.c:239`) and then:
```c
uint16_t ymPeriod = (uint16_t)(ymClockHz / (32.0 * dPeriod2Hz(ch->finalPeriod)));
ymR[0] = ymPeriod & 0xFF;          // channel A fine tune
ymR[1] = (ymPeriod >> 8) & 0x0F;   // channel A coarse tune
```

Volume conversion: `ch->outVol` is 0–64; YM amplitude is 0–15:
```c
ymR[8] = (ch->outVol * 15 + 32) / 64;  // R8/R9/R10 amplitude (no hardware env)
```

### .YM File Format Overview

The .YM5/.YM6 format is:
- 4-byte header `"YM5!"` or `"YM6!"`
- `"LeOnArD!"` string
- `uint32_t nbFrames` — total register frames
- `uint32_t songAttributes` — loop flag, interleaved flag
- `uint16_t nbDigiDrum` — 0 for pure PSG
- `uint32_t ymClock` — 2000000 for Atari
- `uint16_t playerHz` — 50 or 60
- `uint32_t loopFrame` — loop point
- `uint16_t extraDataSize` — 0
- Then frame data: interleaved (all R0 bytes, all R1 bytes, etc.) or sequential per frame
- LHA-compressed body (via libLHA or a bundled implementation)

The register capture buffer at 50 Hz, 3 minutes = 9000 frames × 14 bytes = 126KB before compression.

### .SNDH File Format Overview

SNDH is an Atari ST executable format:
- 4-byte `"SNDH"` identifier
- Tag-value pairs: `"TITL"`, `"COMM"`, `"RIPP"`, `"CONV"`, `"YEAR"`, `"TIME"`, `"TC50"` (50Hz timer), `"!#SN"` (sub-song count)
- Terminates with `"HDNS"` (end-of-header)
- Body: 68000 machine code (the replay routine) + song data

The replay routine is typically a 68000 assembly subroutine. For register-dump playback: load from a frame table and write to YM via the ST's hardware address (`$FF8800`/`$FF8802`). This subroutine is pre-assembled and embedded as a binary blob; the song data is the register frame table itself.

---

## 10. Summary of Critical Integration Points

| # | What | Where | Line | Notes |
|---|------|-------|------|-------|
| 1 | `tickReplayer()` output | `ft2_replayer.c` | 2362 | Core; outputs `ch->finalPeriod`, `ch->outVol`, `ch->status` |
| 2 | SDL callback tick loop | `ft2_audio.c` | 849–870 | Insert YM capture after `tickReplayer()` |
| 3 | WAV export tick loop | `ft2_wav_renderer.c` | 355–409 | Template for YM export thread |
| 4 | `mixReplayerTickToBuffer()` | `ft2_audio.c` | 498 | YM equivalent: `captureYMFrame()` |
| 5 | `editor.moduleSaveMode` | `ft2_structs.h` | 34 | Add YM=3, SNDH=4 |
| 6 | `saveMusicThread()` | `ft2_module_saver.c` | 656 | Add YM/SNDH cases |
| 7 | Save mode radiobuttons | `ft2_radiobuttons.h` | 146–148 | Add 2 new entries |
| 8 | `song.numChannels` lock | `ft2_pattern_ed.c` | 1839–1863 | Gate on `atariMode` |
| 9 | Effect jump tables | `ft2_replayer.c` | 986–1024, 2235–2273 | I/J/M/N/S/U are all `dummy` — free |
| 10 | `instr_t` / PSG extension | `ft2_replayer.h` | 222–235 | Parallel PSG instrument data |
| 11 | `config_t` extension | `ft2_config.h` | 105–162 | Sidecar file approach recommended |
| 12 | `ui_t` new screens | `ft2_structs.h` | 41–71 | Add `atariExportShown` |
| 13 | Note-to-period LUT | `ft2_replayer.c` | 589–592 | `note2PeriodLUT[noteIndex]` → `dPeriod2Hz()` → YM period |
| 14 | `dPeriod2Hz()` | `ft2_replayer.c` | 239–258 | Reuse for Hz → YM register math |
