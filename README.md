# ft2-clone — Atari ST YM/SNDH Edition
### Based on the [8bitbubsy/ft2-clone](https://github.com/8bitbubsy/ft2-clone) — with added Atari ST YM and SNDH support

Create native Atari ST music in a Fasttracker II GUI!

This is a fork of the excellent [Fasttracker II clone by 8bitbubsy](https://github.com/8bitbubsy/ft2-clone), extended with support for the **Atari ST YM2149 sound chip** and the **SNDH music file format**. It brings the power and familiarity of the classic Fasttracker II tracker interface to Atari ST music production.

*What is Fasttracker II? Read about it on [Wikipedia](https://en.wikipedia.org/wiki/FastTracker_2).*

# Features
- **YM2149/AY-3-8910 PSG emulation** — full software emulation of the Atari ST sound chip with support for both AY and YM modes
- **Atari ST mode** — toggle between standard PCM tracker mode and Atari PSG mode; channels are locked to 3 and PSG-specific effects become available (noise period, mixer control, hardware envelope)
- **YM6 export** — export to YM6 format with proper LHA level-0 compression wrapper, compatible with standard YM players
- **SNDH export** — export to SNDH format with an embedded pre-assembled 68000 replayer, proper BRA dispatch table, TC50/TC60 timer tags, and HDNS padding
- **PSG instrument editor** — text-mode UI for editing PSG instruments: volume envelope, arpeggio table, pitch envelope, mixer flags, hardware envelope settings, noise period, and loop points
- **Disk Op integration** — YM6 and SNDH export radio buttons in the Disk Op save dialog
- **Atari-specific effects** — full set of mapped XM effects plus Atari-specific: `7xx` (noise period), `8xx` (mixer control), `9xx` (HW envelope shape), `Exx`/`Gxx` (envelope period)
- **Sidecar configuration** — Atari mode settings persisted separately as `FT2-ATARI.CFG`
- **Fasttracker II workflow** — all the familiar FT2 editing, instruments, and pattern-based composing you know and love
- Everything from the original ft2-clone (see below)

# Original ft2-clone Features
Fasttracker II clone for Windows/macOS/Linux

Aims to be a highly accurate clone of the classic Fasttracker II software for MS-DOS. \ 
The XM player itself has been directly ported from the original source code, for maximum accuracy. \ 
The code is partly my own, partly based on the original FT2 code.

## Improvements over original DOS version
- New sample editor features, like waveform generators and resonant filters
- The channel resampler/mixer uses floating-point arithmetics for less errors, and has extra interpolation options (4-point cubic spline and 8-point/16-point windowed-sinc)
- The sample loader supports AIFF/FLAC/OGG/MP3/BRR (SNES) samples and more WAV types than original FT2. It will also attempt to tune the sample (finetune and rel. note) to its playback frequency on load.
- It contains a new "Trim" feature, which will remove unused stuff to potentially make the module smaller
- Drag n' drop of modules/samples
- The waveform display in the sample editor shows peak based data when zoomed out
- Textboxes have a text marking option, where you can cut/copy/paste
- MOD/STM/S3M import has been slightly improved (S3M import is still not ideal, as it's not compatible with XM)
- Supports loading DIGI Booster (non-Pro) modules
- Supports loading Impulse Tracker modules (Awful support! Don't use this for playback)
- It supports loading XMs with stereo samples, uneven amount of channels, more than 32 channels, more than 16 samples per instrument, more than 128 patterns etc. The unsupported data will be mixed to mono/truncated.
- It has some small additions to make life easier (C4/middle-C Hz display in Instr. Ed., envelope point coordinate display, etc).

# Releases
none yet

# Screenshots



# Compiling the code
Full build instructions can be found in the repository (`HOW-TO-COMPILE.txt`).

**Windows (Visual Studio):**
Open `vs2019_project/ft2-clone.sln` in Visual Studio. The solution file is VS 2019 format but can be opened in newer versions (2022+). When opening in Visual Studio 2022 or later, VS will offer to retarget/upgrade the project — accept this to use modern, updated platform toolsets and Windows SDK versions. The solution targets both **x86** and **x64** in Debug and Release configurations.

**Linux:**
Keep in mind that the program may fail to compile on Linux, depending on your distribution and GCC version. \ 

PS: The source code is quite hackish and hardcoded. \ 
My first priority is to make an accurate clone, and not to make flexible and easily modifiable code.

Big parts of the code (except GUI) are directly ported from the original FT2 source code, with permission to use a BSD 3-Clause license.

# Credits
- Original Fasttracker II clone by [8bitbubsy](https://github.com/8bitbubsy/ft2-clone)
- Atari ST YM/SNDH extensions by [georgeflower](https://github.com/georgeflower)
- Atari ST YM/SNDH support informed by code and research from [tildearrow/furnace](https://github.com/tildearrow/furnace) — the YM2149 emulation references the Furnace/MAME AY-3-8910 implementation
