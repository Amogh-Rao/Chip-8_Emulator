# CHIP-8 Emulator in C

A from-scratch CHIP-8 interpreter written in C11. It emulates the full CHIP-8 instruction set, renders to the terminal, reads the keyboard, and runs real ROMs like Pong.


## What is CHIP-8?

CHIP-8 is a tiny virtual machine from the mid-1970s, designed so games could be written once and run on different hobbyist computers. It has no physical hardware of its own: a CHIP-8 "computer" exists only as a spec, and an emulator like this one pretends to be it by tracking its state and executing its instructions.

The machine has:

| Component | Details |
|---|---|
| Memory | 4 KB (4096 bytes) |
| Registers | Sixteen 8-bit registers, V0 to VF (VF doubles as a flag) |
| Index register `I` | 16-bit, holds a memory address |
| Program counter | 16-bit, address of the next instruction |
| Stack | 16 levels of return addresses |
| Display | 64×32 monochrome, drawn with XOR sprites |
| Input | 16-key hex keypad |
| Timers | Delay and sound timers, counting down at 60 Hz |

## Features

- All 34 standard opcodes (the historical `0NNN` machine-code call is ignored, as in virtually every interpreter)
- Built-in hex font, XOR sprite drawing with collision detection (`VF`), and edge clipping
- Carry, borrow, and shift flags for the `8XY_` arithmetic family
- Fixed-timestep main loop: instructions run in batches per frame, and both timers tick at 60 Hz independent of CPU speed
- Terminal rendering and live keyboard input
- ROM path as a command-line argument, with size validation and error handling
- Compiles cleanly with `-Wall -Wextra -std=c11`

## Build and run

**Requirements:** Windows, `gcc` (TDM-GCC or another MinGW-w64 toolchain), and `make`.

```
make
.\main path\to\rom.ch8
```

With no argument, it loads a default ROM from the current folder. Use a terminal at least **128 columns by 33 rows** (each pixel is drawn as two characters so the picture isn't squished). Press **Esc** to quit.

### Controls

The CHIP-8 keypad is mapped onto the left side of the keyboard:

```
CHIP-8 keypad        Your keyboard
 1  2  3  C            1  2  3  4
 4  5  6  D            Q  W  E  R
 7  8  9  E            A  S  D  F
 A  0  B  F            Z  X  C  V
```

## Testing

Verified against the [Timendus CHIP-8 test suite](https://github.com/Timendus/chip8-test-suite):
