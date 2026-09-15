# uemu-ng — tiny RISC-V system emulator

<table>
  <tr>
    <td><img src="./assets/edk2_boot.png" alt="EDK II boot" width="100%"></td>
    <td><img src="./assets/edk2_uiapp.png" alt="EDK II UIApp" width="100%"></td>
  </tr>
</table>

**Uotan RISC-V Emulator - Next Generation** (*uemu-ng*) is a small rv64gc system emulator. It is a refactored version of [uemu](https://github.com/Uotan-Dev/uotan_riscv_emu). While **uemu** served as a proof-of-concept, the **NG** version focuses on strict architectural compliance and robust device emulation.

A UEFI firmware implementation (EDK2) for **uemu-ng** is available at [Uotan-Dev/edk2-uemu](https://github.com/Uotan-Dev/edk2-uemu).

**uemu-ng** supports the following RISC-V ISA features:
* RV64I base ISAs, v2.1
* Zifencei extension, v2.0
* Zicsr extension, v2.0
* Zicntr extension, v2.0
* M extension, v2.0
* A extension, v2.1
* F extension, v2.2
* D extension, v2.2
* C extension, v2.0
* Svadu extension, v1.0
* Svade extension, v1.0
* Zca extension, v1.0
* Zcd extension, v1.0

**uemu-ng** includes the following memory-mapped devices:

| Device | Address Range | Description |
|--------|---------------|-------------|
| CLINT | 0x2000000-0x200ffff | Core Local Interruptor |
| TestIntrGen | 0x40000000-0x40000fff | Sail-style interrupt generator for ACT tests |
| PLIC | 0xc000000-0xcffffff | Platform-Level Interrupt Controller |
| SiFiveTest | 0x100000-0x100fff | Test device for shutdown/reboot |
| NS16550 UART | 0x10000000-0x100000ff | Serial console |
| SimpleFB | 0x50000000-0x502fffff | Framebuffer (3MB, 1024x768) |
| VirtIO-Block | 0x10001000-0x10001fff | Block device interface |
| pflash-cfi01 | 0x20000000-0x21ffffff<br>0x22000000-0x23ffffff | CFI parallel flash with Intel command set |
| GoldfishEvents | 0x10002000-0x10002fff | Input event device |
| GoldfishRTC | 0x101000-0x1010ff | Real-time clock |
| GoldfishBattery | 0x10003000-0x10003fff | Battery status |
| BCM2835Rng | 0x10004000-0x1000400f | Random number generator |
| NemuConsole | 0x10008000-0x10008007 | Debug console from [NEMU](https://github.com/NJU-ProjectN/nemu) |

## Continuous Integration Status

| Status (main) | Description |
| :-----------: | :---------: |
| [![build status](https://github.com/Uotan-Dev/uotan_riscv_emu-ng/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Uotan-Dev/uotan_riscv_emu-ng/actions/workflows/ci.yml?query=branch%3Amain) | Build and Test |


## Boot Demo

### [Debian](https://www.debian.org/)
![Booting Debian on uemu-ng](./assets/debian.png)

## Building
## Prerequisites

* **CMake**: 3.20 or later
* **C++ Compiler**: C++23 compatible
* **C Compiler**: C17 compatible

### Required dependencies:

* CLI11 - Command line parsing
* libfdt (`pkg-config` name `libfdt`, Debian/Ubuntu package `libfdt-dev`) - device tree generation
* SDL3 & SDL3_image - Graphics and windowing
* `riscv64-unknown-elf-gcc`, `riscv64-unknown-elf-objcopy`, `riscv64-unknown-elf-objdump`

### Build Instructions
```bash
# Clone the repository
git clone <repository-url>
cd uemu-ng

# Initialize submodules
git submodule update --init --recursive

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build . --config Release -j$(nproc)
```

## Usage
```
uemu-ng: RISC-V Emulator


uemu [OPTIONS]


OPTIONS:
  -h,     --help              Print this help message and exit
  -v,     --version           Display program version information and exit
  -f,     --file TEXT:FILE    ELF file to load (required unless --dump-dtb is used)
  -m,     --memory UINT:INT in [64 - 16384] [512]
                              DRAM size in MB
  -d,     --disk TEXT         Disk file to use
          --flash0 TEXT       Flash0 file to use
          --flash1 TEXT       Flash1 file to use
          --dump-dtb TEXT     Write the generated DTB to a file
  -t,     --timeout INT:NONNEGATIVE [0]
                              Execution timeout in milliseconds (0 = no timeout)
          --headless          Run in headless mode (no UI window)
```

## Device tree

The machine description is no longer a checked-in `.dts`: **uemu-ng** builds it
from the board configuration at startup (`include/board_config.hpp`), so the
tree firmware receives and the devices the emulator creates always agree.  The
blob is written to ordinary DRAM and handed over in `a1` (with `a0` holding the
boot hart id, 0), as the RISC-V boot ABI expects.

`--dump-dtb` writes the tree the run would use, so it can be inspected with the
usual device tree tools:

```bash
# Without --file it just writes the tree and exits
uemu --dump-dtb uemu.dtb

# Or dump the tree of a normal boot
uemu --headless --file firmware.elf --dump-dtb uemu.dtb

dtc -I dtb -O dts uemu.dtb
```

## Known Issues

* **No JIT**: It lacks Just-In-Time compilation; every instruction is fetched and decoded individually, so it is slower than **uemu**.
* **ACT4 compliance**: A few ACT4 compliance tests still fail; see [ACT4 Compliance Testing](#act4-compliance-testing).

## ACT4 Compliance Testing

**uemu-ng** uses the [riscv-arch-test](https://github.com/riscv/riscv-arch-test) (ACT4) suite for RISC-V ISA compliance verification. The test configuration files are located in `thirdparty/act_config/`.

The following extensions are excluded from testing:
* `SsstrictS`, `SsstrictSm`, `SsstrictU` — strict s-mode coverage
* `Sscounterenw` — scounteren write behavior

The following tests currently fail:

* `Sm_mcsr_cntr` — covers `mtime`/`time` (`Zicntr`); the CLINT derives `mtime` from the host wall clock, so the time source is not deterministic and only intermittently matches the timer timing ACT4 assumes.
* `InterruptsS`, `InterruptsSSm`, `InterruptsU` — interrupt delivery is still affected by interrupt latency and timer timing; upstream tracks related known issues in [riscv-arch-test#2146](https://github.com/riscv/riscv-arch-test/issues/2146).

These failures are timing-dependent, so they are not all treated as confirmed architectural bugs in **uemu-ng**.

## TODO

Due to the limitation of the author's ability, 
planned and ongoing work for future versions of **uemu** includes:

* **Emulation for more devices** - mouse, GPU etc.
* **JIT compilation** — introduce a 2-Tier JIT for improved performance.

## Acknowledgments

**uemu-ng** is inspired by and benefits from a number of outstanding open-source projects, educational courses, and contributors from the systems programming community.

### Open-source Projects

This project draws design ideas, implementation references, and architectural inspiration from the following open-source emulators and system projects:

- [qemu/qemu](https://github.com/qemu/qemu)
- [NJU-ProjectN/nemu](https://github.com/NJU-ProjectN/nemu)
- [sysprog21/rv32emu](https://github.com/sysprog21/rv32emu)
- [snnbyyds/semu](https://github.com/snnbyyds/semu)
- [bane9/rv64gc-emu](https://github.com/bane9/rv64gc-emu)

### Courses and Educational Resources

Special thanks to:

- [@cocowhy1013](https://github.com/cocowhy1013)
- [@jiangyy](https://github.com/jiangyy)
- [@sashimi-yzh](https://github.com/sashimi-yzh)

for their course  
[*Introduction to Computer Systems*](https://nju-projectn.github.io/ics-pa-gitbook/).

### Contributions and Support

- [Wuhan Youtan Network Technology Co., Ltd.](https://www.uotan.cn/) — for providing support during the development of this project.

## License

Copyright 2025-2026 Nuo Shen, Nanjing University

Copyright 2026 UOTAN

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the [LICENSE](LICENSE) file for the specific language governing permissions and limitations under the License.
