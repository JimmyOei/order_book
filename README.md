# Order Book

This is a personal project on learning how to work with NASDAQ TotalView ITCH 5.0 market data feed, parse its binary files, and rebuild the full limit order book.

## Tools

Each tool is a small executable in `src/tools/` built on the shared ITCH parser
in `src/itch/`. All tools take the path to a raw (decompressed) ITCH 5.0 file
as their only argument.

| Tool      | Source                         | What it does                                                                                               |
|-----------|--------------------------------|------------------------------------------------------------------------------------------------------------|
| `counter` | `src/tools/message_counter.cc` | Prints how many messages of each type were in the binary. |

## Prerequisites

Developed on Ubuntu 24.04 (WSL2).

```sh
sudo apt install g++ gdb cmake ninja-build clang clang-format clang-tidy
```

Requires CMake ≥ 3.25 and a C++20 compiler (tested with GCC 13 and Clang 18).

## Data

The tools read raw NASDAQ TotalView-ITCH 5.0 files. The sample day used during
development is `12302019.NASDAQ_ITCH50.gz` from
<https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/> (~3.5 GB compressed, ~8.3 GB raw).

```sh
mkdir -p data
# download 12302019.NASDAQ_ITCH50.gz into data/, then:
gunzip -k data/12302019.NASDAQ_ITCH50.gz
```

## Build

Builds are driven by CMake presets; each preset gets its own directory under
`build/<preset>/`.

| Preset        | Build type     | Purpose                                                   |
|---------------|----------------|-----------------------------------------------------------|
| `debug`       | Debug          | Day-to-day development, with AddressSanitizer + UBSan      |
| `debug-clang` | Debug          | Same as `debug`, compiled with Clang                       |
| `release`     | Release        | `-O3 -march=native`; use for timings and benchmarks        |
| `profile`     | RelWithDebInfo | Optimised + debug info + frame pointers, for `perf record` |

```sh
cmake --preset debug           # configure (once, or after editing CMakeLists.txt)
cmake --build --preset debug   # compile
ctest --preset debug           # run tests
```

## Run

To run a tool, run the built executable followed by the path to the NASDAQ ITCH binary. Use time to time the execution. Example:

```sh
./build/debug/counter data/12302019.NASDAQ_ITCH50
time ./build/release/counter data/12302019.NASDAQ_ITCH50
```

## Formatting and linting

Code follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html),
with one deliberate deviation: exceptions are used for unrecoverable input errors.

Formatting is configured in `.clang-format`:

```sh
clang-format -i src/**/*.h src/**/*.cc        # format in place
clang-format --dry-run --Werror src/**/*.h src/**/*.cc   # check only
```

Static analysis and naming rules are configured in `.clang-tidy`. It needs the
`compile_commands.json` that the build generates, so configure a preset first:

```sh
clang-tidy -p build/debug src/tools/*.cc
```

To suppress a single finding, put a justification comment and
`// NOLINTNEXTLINE(<check-name>)` on the line above it.

## Experiments

### Timing Message Counter 32942ea
`time ./build/release/counter data/12302019.NASDAQ_ITCH50`
```sh
real    0m9.616s
user    0m2.246s
sys     0m3.821s
```