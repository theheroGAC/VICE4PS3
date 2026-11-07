# VICE4PS3

**VICE4PS3** is an optimized, native Commodore 64 emulator for the Sony PlayStation 3.  
Based on VICE (Versatile Commodore Emulator) 3.3, built with the open-source **PSL1GHT v2** SDK and **SDL2**.

---

## Features

- **Full Commodore 64 Emulation**:
  - Cycle-exact MOS 6510/8500 CPU and CIA timers.
  - Complete VIC-II video controller emulation with smooth 50 Hz (PAL) and 60 Hz (NTSC) display modes.
  - Comprehensive media support: `.d64`, `.d71`, `.d81`, `.t64`, `.tap`, `.prg`, `.p00`, and `.crt` cartridge formats (including EasyFlash images).

- **Authentic Cycle-Exact Audio (ReSID Engine)**:
  - Default **ReSID** cycle-exact MOS 6581 and MOS 8580 emulation.
  - Native 1:1 bit-perfect 48,000 Hz stereo output via PS3 hardware audio subsystem.
  - Fixed-clock audio synchronization (`SOUND_ADJUST_EXACT`) with zero pitch warble or wow-and-flutter.
  - Anti-aliased interpolation (`SidResidSampling = 1`) eliminating digital high-frequency harshness.
  - Fully modeled analog filter sweeps and digital sample/speech playback.

- **High-Performance HD GUI**:
  - Modern, hardware-accelerated 1080p/720p interface rendered with FreeType typography.
  - Intuitive tabbed configuration: Display & Video, Audio & SID, Controllers & Input, Drive / Tape / Cartridge management, and Built-in File Browser.

- **Optimized C64 Virtual Keyboard**:
  - Full Commodore 64 PETSCII layout with dedicated function and cursor keys.
  - Precalculated text geometry and fast-fill rendering running at a rock-solid 50/60 FPS without emulation slowdown.
  - Quick toggle via **L1** or **L2**.

- **Console-Optimized Controller Shortcuts**:
  - **Instant Joystick Port Toggle (R3)**: Click the Right Analog Stick at any time to swap between **Port 1** and **Port 2** with an on-screen toast notification.
  - **START Button Mapped to SPACE**: Press START to immediately begin games or skip cracktros/trainer screens.

- **Storage & Savestates**:
  - Multiple save state slots with screenshot preview.
  - Support for mounting disks across Drive 8 and Drive 9 simultaneously.
  - Integrated ZIP archive extraction on the fly.

---

## Default Controls

### In-Game (DualShock 3 / Sixaxis)

| PS3 Button | C64 Function | Notes |
|---|---|---|
| **D-Pad / Left Stick** | Joystick Direction | Up, Down, Left, Right |
| **Cross (X)** | Joystick Fire | Primary fire button |
| **Circle (O)** | C64 `SPACE` | Space bar |
| **Square (□)** | C64 `RETURN` | Enter / Return key |
| **Triangle (△)** | C64 `RUN/STOP` | Stop / Break |
| **START** | C64 `SPACE` / Start | Quick-start games and dismiss intros |
| **SELECT** | Open Emulator Menu | Access settings and file browser |
| **L1 / L2** | Virtual Keyboard | Toggle on-screen C64 keyboard |
| **R3 (Right Stick Click)** | **Swap Joystick Port** | Toggle between Port 1 and Port 2 |
| **R1** | Quick Save State | Saves current progress |
| **R2** | Quick Load State | Restores saved state |

### In-Menu Navigation

| PS3 Button | Menu Function |
|---|---|
| **D-Pad Up / Down** | Move selection cursor |
| **Cross (X)** | Select / Toggle setting / Open folder / Mount image |
| **Square (□)** | Mount to Drive 1 (Unit 8) |
| **Triangle (△)** | Mount to Drive 2 (Unit 9) / Load defaults |
| **Circle (O) / SELECT** | Close Menu and resume emulation |

### Virtual Keyboard

| PS3 Button | Keyboard Function |
|---|---|
| **D-Pad / Left Stick** | Move keyboard cursor |
| **Cross (X)** | Press highlighted key |
| **Square (□)** | Toggle SHIFT lock |
| **Triangle (△)** | Send RUN/STOP |
| **L1 / L2 / Circle (O)** | Close Virtual Keyboard |

---

## Directory Structure

On your PS3 console, files are organized under:

```text
/dev_hdd0/game/VICEP3001/USRDIR/
  ├── EBOOT.BIN           (Main application executable)
  ├── vice.ini            (Configuration file)
  ├── data/
  │   ├── games/          (Place your C64 roms and disk images here)
  │   ├── saves/          (Saved states and SRAM data)
  │   └── covers/         (Box art / screenshots)
  └── resources/          (C64 ROMs: basic, kernal, chargen, drive ROMs)
```

You can also browse and run games directly from any FAT32 USB drive connected to `/dev_usb000/` or `/dev_usb001/`.

---

## Audio Configuration

VICE4PS3 includes full support for both revisions of the MOS Technology SID synthesizer:

- **MOS 6581**: The original SID chip found in "breadbin" Commodore 64 units. Characterized by warm analog filter curves and non-linear distortion.
- **MOS 8580**: The revision found in newer Commodore 64C models. Features cleaner channel separation, linear volume scaling, and crisp highs.

You can switch models on the fly in the **AUDIO** tab of the settings menu.

---

## Building from Source

### Prerequisites

1. **PS3 Toolchain**: [PSL1GHT v2 / ps3dev SDK](https://github.com/ps3dev/ps3dev)
   - Ensure `$PS3DEV` and `$PSL1GHT` environment variables are properly exported.
   - PPU GCC compiler: `powerpc64-ps3-elf-gcc` / `powerpc64-ps3-elf-g++`.
2. **CMake** (v3.16 or higher).
3. **Make / Ninja**.
4. **Python 3**.

### Compilation Steps

1. Clone or copy the repository:
   ```bash
   git clone https://github.com/<your-username>/VICE4PS3.git
   cd VICE4PS3
   ```

2. Compile the C64 core:
   ```bash
   ./build.sh c64
   ```

3. Generate the installation package:
   ```bash
   ./package.sh
   ```

4. The output `.pkg` files will be generated in `dist/`:
   - `dist/VICE4PS3.pkg` (Standard retail/package format)
   - `dist/VICE4PS3.gnpdrm.pkg` (NPDRM signed package for CFW / PS3HEN)

---

## License

- **VICE**: GNU General Public License v2 (GPLv2).
- **PSL1GHT**: Open-source PlayStation 3 SDK, BSD-compatible license.
- Individual library components (SDL2, FreeType, ReSID) are licensed under their respective open-source licenses.
