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
npm run build

# Generate TypeScript definitions
npm run generate-types

# Run the test script
node index.js
```

Note: `npm run build` requires a C++ toolchain (e.g. Visual Studio Developer Command Prompt on Windows).

The build produces `build/Release/ubx-parser.node` — a native `.node` binary loaded via the `bindings` package.

## Architecture

**Native addon entry point:** `ubx.cc` — Registers N-API functions:
- `parse(Uint8Array)` — Takes raw UBX bytes, creates a `Parser`, returns an array of parsed message objects
- `schema()` — Returns type metadata for all supported UBX messages (used by codegen)

**UBX parser:** `parser.h` / `parser.cpp` — C++ class that uses the comms protocol framework to decode UBX frames. Uses a generic template `handle<TMsg>()` that dispatches all ~315 concrete message types, extracting fields via `FieldToJs` visitor into N-API result objects.

**Field visitors:**
- `field_visitor.h` (`FieldToJs`) — Converts comms field values to JavaScript (N-API) values
- `schema_visitor.h` (`SchemaFieldVisitor`) — Extracts field type metadata for TypeScript codegen

**Type generation:** `scripts/generate-types.js` — Calls `schema()`, converts to TypeScript interfaces, writes `types.d.ts`

## Dependencies (git submodules in `deps/`)

- `deps/comms/` — [CommsChampion](https://github.com/commschamp/comms) protocol framework (header-only C++ library)
- `deps/cc.ublox.generated/` — Auto-generated u-blox protocol definitions for the comms framework

## Key Details

- C++17 standard, N-API version 9
- Build system: CMake via `cmake-js` (not node-gyp)
- Precompiled headers enabled for the heavy comms/ublox headers
- The `bindings` npm package handles finding the compiled `.node` file at runtime
- Template-heavy code is consolidated in `parser.cpp` to minimize compile times; `parser.h` contains only declarations
