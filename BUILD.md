# Build Guide

## Prerequisites

| Tool | Minimum Version | Notes |
|------|----------------|-------|
| **CMake** | 3.20+ | Build system generator |
| **C++ Compiler** | C++23 support | MSVC 19.38+, Clang 17+, or GCC 14+ |
| **Ninja** *(optional)* | 1.10+ | Faster builds (recommended) |

## Build Steps

### 1. Configure

```bash
# Default (system generator)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# With Ninja (recommended)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# Release build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# With tests and examples
cmake -S . -B build -DETHERZ_BUILD_TESTS=ON -DETHERZ_BUILD_EXAMPLES=ON
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ETHERZ_BUILD_TESTS` | `OFF` | Build unit test suite (`bin/etherz_tests`) |
| `ETHERZ_BUILD_EXAMPLES` | `OFF` | Build example programs |

### 2. Build

```bash
cmake --build build
```

### 3. Run

```bash
# Linux / macOS
./bin/etherz

# Windows
.\bin\etherz.exe
```

## Build Types

| Type | Flags (MSVC) | Flags (GCC/Clang) | Use Case |
|------|-------------|-------------------|----------|
| **Debug** | `/Zi /Ob0 /Od` | `-g -O0` | Development & debugging |
| **Release** | `/O2` | `-O3` | Production |

## Compiler Warnings

The project enables strict warnings by default:

| Compiler | Flags |
|----------|-------|
| **MSVC** | `/W4 /permissive- /utf-8` |
| **GCC/Clang** | `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` |

## Output Directories

| Artifact | Directory |
|----------|-----------|
| Executables | `bin/` |
| Static Libraries | `lib/` |
| Shared Libraries | `lib/` |

## Platform Notes

### Windows

- **WinSock2** is automatically linked via `#pragma comment(lib, "ws2_32.lib")` in `socket.hpp`
- UTF-8 console output requires calling `utf8_console()` (see `main.cpp`)
- Tested with MSVC (Visual Studio 2022) and Clang-CL

### Linux / macOS

- POSIX sockets are used (`<sys/socket.h>`, `<netinet/in.h>`)
- No additional libraries needed
- UTF-8 works out of the box on modern terminals

## Clean Build

```bash
# Remove build artifacts
cmake --build build --target clean

# Full clean (remove build directory)
rm -rf build bin lib
```

## IDE Integration

### Visual Studio Code

A `compile_commands.json` is generated in the project root for IntelliSense. Use the **CMake Tools** extension for the best experience.

### Visual Studio

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
```

Then open `build/etherz.sln`.

### CLion

Open the project folder directly — CLion auto-detects `CMakeLists.txt`.
