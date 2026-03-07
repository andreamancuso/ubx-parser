# ubx-parser

A native Node.js addon for parsing [u-blox UBX binary protocol](https://www.u-blox.com/) messages (GNSS/GPS). Built with [node-addon-api](https://github.com/nodejs/node-addon-api) (N-API) and wraps the [CommsChampion](https://github.com/commschamp) ecosystem for protocol definitions.

Supports 315+ UBX message types with sub-millisecond parsing and auto-generated TypeScript definitions. Includes two complementary classes: `UbxParser` for streaming and `UbxLog` for file-based navigation.

## Installation

```bash
npm install ubx-parser
```

Prebuilt binaries are included for Windows (x64, ARM64), macOS (ARM64), and Linux (x64, ARM64). No C++ compiler needed for supported platforms.

## Usage

### Streaming parser

```js
const { UbxParser } = require('ubx-parser');

const parser = new UbxParser();

parser.on('NAV-PVT', (msg) => {
  console.log(`Position: ${msg.lat}, ${msg.lon}`);
});

parser.on('message', (msg) => {
  console.log(`${msg.name}: ${JSON.stringify(msg)}`);
});

// Feed data as it arrives (handles partial messages and frame boundaries)
parser.feed(chunk);
```

### One-shot parsing

```js
const { UbxParser } = require('ubx-parser');

const messages = UbxParser.parse(new Uint8Array([
  0xb5, 0x62, 0x01, 0x07, /* ... UBX binary data ... */
]));

for (const msg of messages) {
  console.log(msg.name, msg);
}
```

### File-based log reader

```js
const { UbxLog } = require('ubx-parser');

const log = await UbxLog.open('recording.ubx');

console.log(`Total messages: ${log.count()}`);
console.log(`NAV-PVT count: ${log.count('NAV-PVT')}`);
console.log(`Message types: ${log.messageTypes().join(', ')}`);

// Navigate forward
const msg = log.next('NAV-PVT');
console.log(msg);

// Get the 5th NAV-PVT message (0-based)
const fifth = log.get('NAV-PVT', 4);

// Seek to beginning and iterate
log.seek(0);
let m;
while ((m = log.next()) !== null) {
  console.log(m.name);
}

log.close();
```

`UbxLog` builds a SQLite index on first open (saved as a companion `.idx` file). Subsequent opens reuse the index for instant access. Messages are deep-parsed on demand.

## When to use which class

| | `UbxParser` | `UbxLog` |
|---|---|---|
| **Use case** | Streaming / real-time data | Post-processing log files |
| **Input** | Byte chunks (feed incrementally) | File path |
| **Navigation** | Forward only (event-driven) | Random access, cursor-based |
| **Memory** | Buffers only partial frames | SQLite index + on-demand parsing |

## API

### `new UbxParser()`

Creates a streaming parser instance. Extends `EventEmitter`.

### `parser.feed(chunk: Uint8Array | Buffer): UbxMessage[]`

Feeds raw bytes into the parser. Buffers partial messages internally and returns an array of fully parsed messages. Emits events for each parsed message.

### Events

- **`'message'`** `(msg: UbxMessage)` — Emitted for every parsed message.
- **`'NAV-PVT'`**, **`'NAV-POSLLH'`**, etc. — Emitted for specific message types by name.

### `UbxParser.parse(data: Uint8Array): UbxMessage[]`

Static method. Parses a complete buffer and returns all decoded messages.

### `UbxParser.schema(): object[]`

Static method. Returns field metadata for all supported UBX message types. Used internally by the type generation script.

### `UbxLog.open(filePath: string): Promise<UbxLog>`

Static async factory. Opens a `.ubx` file, builds or reuses a companion `.idx` index, and resolves with a ready `UbxLog` instance.

### `log.count(name?: string): number`

Returns total message count, or count of a specific message type.

### `log.messageTypes(): string[]`

Returns an array of distinct message type names found in the file.

### `log.next(name?: string): UbxMessage | null`

Advances the cursor and returns the next message (optionally filtered by type). Returns `null` at end.

### `log.prev(name?: string): UbxMessage | null`

Moves the cursor backward and returns the previous message. Returns `null` at beginning.

### `log.seek(pos: number): void`

Positions the cursor. `seek(0)` resets to the beginning. `seek(-1)` moves to the end (for backward iteration with `prev()`).

### `log.get(name: string, ordinal: number): UbxMessage | null`

Returns the Nth message (0-based) of a given type, without affecting the cursor.

### `log.close(): void`

Closes the file handle.

## TypeScript

Full TypeScript definitions are included with typed interfaces for all 315+ message types and type-narrowing event listeners.

```ts
import { UbxParser, NavPvtUblox89 } from 'ubx-parser';

const parser = new UbxParser();
parser.on('NAV-PVT', (msg: NavPvtUblox89) => {
  console.log(msg.lat, msg.lon, msg.height);
});
```

## Platform support

Prebuilt binaries are included for:

| Platform | Architecture |
|---|---|
| Windows | x64, ARM64 |
| macOS | ARM64 (Apple Silicon) |
| Linux | x64, ARM64 |

Requires Node.js >= 18. Uses N-API v9 for ABI stability across Node.js versions.

### Help wanted

So far this project has only been tested on Windows x64. Testing and feedback on other platforms (especially macOS and Linux ARM64) is very welcome. Please open an issue if you run into problems.

## Building from source

For contributors or unsupported platforms:

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/andreamancuso/ubx-parser.git
cd ubx-parser

# Install dependencies and compile
npm install --ignore-scripts
npm run compile

# Regenerate TypeScript definitions
npm run generate-types
```

Requires CMake (3.16+) and a C++ compiler with C++17 support (Visual Studio 2019+, GCC, or Clang).

### Windows prebuilds

Windows prebuilds are compiled locally and committed to git. Run from a Developer Command Prompt:

```bash
# x64
npm run build:prebuild:win32-x64

# ARM64 (cross-compile, requires MSVC ARM64 build tools)
npm run build:prebuild:win32-arm64
```

## Acknowledgements

This project is built on top of the [CommsChampion](https://github.com/commschamp) ecosystem by [Alex Robenko](https://github.com/arobenko):

- **[comms](https://github.com/commschamp/comms)**: Header-only C++ library for implementing binary communication protocols.
- **[cc.ublox.generated](https://github.com/commschamp/cc.ublox.generated)**: Auto-generated u-blox protocol definitions for the comms framework.

The CommsChampion ecosystem provides an elegant, type-safe approach to protocol implementation that made it possible to support all 315+ UBX message types with minimal hand-written code.

## License

MPL-2.0
