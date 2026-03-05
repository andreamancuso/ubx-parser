# ubx-parser

A native Node.js addon that parses [u-blox UBX binary protocol](https://www.u-blox.com/) messages. Built with [node-addon-api](https://github.com/nodejs/node-addon-api) (N-API) and wraps the [cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated) + [comms](https://github.com/commschamp/comms) libraries.

Supports all ~315 UBX message types with auto-generated TypeScript definitions.

## Prerequisites

- Node.js (v18+)
- C++ compiler (Visual Studio 2019+ on Windows, GCC/Clang on Linux/macOS)
- CMake (3.16+)

## Setup

```bash
# Clone with submodules
git clone --recurse-submodules <repo-url>
cd ubx-parser

# Install dependencies and compile the native addon
npm install
```

## Build

```bash
# Rebuild after C++ changes
npm run build

# Generate TypeScript type definitions
npm run generate-types
```

## Usage

```js
const ubx = require('ubx-parser');

const messages = ubx.parse(new Uint8Array([
  0xb5, 0x62, 0x01, 0x07, /* ... UBX binary data ... */
]));

for (const msg of messages) {
  console.log(msg.name, msg);
}
```

### API

- **`parse(data: Uint8Array): UbxMessage[]`** — Parse raw UBX binary data into an array of typed message objects. Each message has a `name` field (e.g. `"NAV-PVT (ublox-8/9)"`) and all protocol fields as properties.

- **`schema(): object[]`** — Returns introspected type metadata for all supported UBX message types. Used by `generate-types` to produce TypeScript definitions.

## TypeScript

Run `npm run generate-types` after building to produce `types.d.ts` with interfaces for all ~315 message types.

## Project Status

Pre-alpha. The core parsing works for all UBX message types but the API may change.
