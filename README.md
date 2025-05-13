# 🧮 Minimal Forth Interpreter (C / RP2040 / macOS)

This is a minimal, educational Forth interpreter written in C, designed to run on both macOS and the Raspberry Pi Pico 2 (RP2040). It serves as a platform to explore Forth concepts, low-level computing, and dual-target development without relying on external libraries or runtime environments.

---

## ✨ Highlights

- **Clean ANSI C** implementation — portable and readable
- Runs both as a native CLI (`forth_host`) and on bare-metal RP2040 (`forth_pico`)
- Shared interpreter core (`forth_core.c`) across both platforms
- Uses **CMake** for clean build separation (macOS vs. Pico)
- Includes a **Python + pytest** test suite that simulates REPL input via PTY
- No `#ifdef` hacks — one interpreter, two platforms

---

## 📁 File Structure

```text
src/
├── forth_core.c         # Core interpreter (shared)
├── forth_core.h
├── forth_host.c         # macOS / test harness entry point
└── pico_main.c          # RP2040 entry point

tests/
└── test_forth.py        # PTY-based test runner using pytest

