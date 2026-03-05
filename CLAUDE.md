# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A native Node.js addon (N-API) that parses u-blox GPS/GNSS binary protocol (UBX) messages. Built as a C++ addon using [node-addon-api](https://github.com/nodejs/node-addon-api) and wraps the [cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated) + [comms](https://github.com/commschamp/comms) libraries. Early-stage project (pre-alpha).

## Build & Run

```bash
# First-time setup (clones git submodules + installs + compiles)
git submodule update --init --recursive
npm install          # runs cmake-js compile via the install script

# Rebuild after C++ changes
npx cmake-js compile

# Run the test script
node hello.js
```

The build produces `build/Release/ubx-parser.node` — a native `.node` binary loaded via the `bindings` package.

## Architecture

**Native addon entry point:** `hello.cc` — Registers two N-API functions:
- `AcceptByteArray(Uint8Array)` — Takes raw UBX bytes, creates a `Parser`, returns a JS object with parsed fields (e.g. `{lon, lat}`)
- `CreateByteArray(Array<number>)` — Converts a JS number array to a Uint8Array (utility)

**UBX parser:** `parser.h` / `parser.cpp` — C++ class that uses the comms protocol framework to decode UBX frames. Uses handler-based dispatch: each supported message type gets a `handle()` overload that extracts fields into the N-API result object.

**Currently supported UBX messages:**
- `NAV-PVT` — Sets `lon`/`lat` on the result object
- `NAV-POSLLH` — Prints lon/lat to stdout (not yet wired to result object)
- All other messages are silently ignored via the catch-all `handle(InMessage&)`

**To add a new UBX message type:** Add its include in `parser.h`, add the type alias (e.g. `using InNavFoo = cc_ublox::message::NavFoo<InMessage>`), add it to the `AllInMessages` tuple, and implement a `handle()` overload in `parser.cpp`.

## Dependencies (git submodules in `deps/`)

- `deps/comms/` — [CommsChampion](https://github.com/commschamp/comms) protocol framework (header-only C++ library)
- `deps/cc.ublox.generated/` — Auto-generated u-blox protocol definitions for the comms framework

## Key Details

- C++14 standard, N-API version 9
- Build system: CMake via `cmake-js` (not node-gyp)
- The `bindings` npm package handles finding the compiled `.node` file at runtime
