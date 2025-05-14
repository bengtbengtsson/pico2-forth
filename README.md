# 🧮 Minimal Forth Interpreter (C / RP2040 / macOS)

This is a minimal, educational Forth interpreter written in C, designed to run on both macOS and the Raspberry Pi Pico 2 (RP2040). It serves as a platform to explore Forth concepts, low-level computing, and dual-target development without relying on external libraries or runtime environments.

---

## ✨ Highlights

- Clean ANSI C implementation — portable and readable
- Runs both as a native CLI (`forth_host`) and on bare-metal RP2040 (`forth_pico`)
- Shared interpreter core (`forth_core.c`) across both platforms
- Uses CMake for clean build separation (macOS vs. Pico)
- Includes a Python + pytest test suite that simulates REPL input via PTY
- No `#ifdef` hacks — one interpreter, two platforms

---

## 📁 File Structure

    src/
    ├── forth_core.c         # Core interpreter (shared)
    ├── forth_core.h
    ├── forth_host.c         # macOS / test harness entry point
    └── pico_main.c          # RP2040 entry point

    tests/
    └── test_forth.py        # PTY-based test runner using pytest

---

## 🛠 Prerequisites

### 📦 Pico SDK Setup

To build the firmware for Raspberry Pi Pico 2, you need to install and configure the official pico-sdk:

    git clone https://github.com/raspberrypi/pico-sdk.git
    cd pico-sdk
    git submodule update --init

You can then either:
- Set the environment variable `PICO_SDK_PATH` to the path of the SDK (e.g., `export PICO_SDK_PATH=~/pico-sdk`)
- Or copy the SDK into your project and reference it from `CMakeLists.txt`

---

### 🍎 macOS Toolchain (Homebrew)

To build the native and Pico targets on macOS, install the required packages:

    brew install cmake
    brew install gcc-arm-embedded
    brew install python
    brew install pytest

Make sure the compiler is available in your path:

    arm-none-eabi-gcc --version

---

## 🖧 Interacting via USB Serial (minicom)

Once the firmware is flashed onto the Pico 2, you can communicate with the Forth interpreter over USB using `minicom` (or any serial terminal):

    brew install minicom

To start a session:

    minicom -b 115200 -D /dev/tty.usbmodemXXXX

Replace `/dev/tty.usbmodemXXXX` with the actual device path (you can list devices with `ls /dev/tty.*`).

You should see the `ok>` prompt and be able to enter Forth commands. To exit minicom, press `Ctrl-A` then `Z`, then `X`.

Make sure `stdio_usb` is enabled in your firmware with `pico_enable_stdio_usb(...)` in your `CMakeLists.txt`.

