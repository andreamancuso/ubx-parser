'use strict';

const path = require('path');
const { EventEmitter } = require('events');
const ubx = require('node-gyp-build')(path.join(__dirname, '..'));

class UbxParser extends EventEmitter {
  constructor() {
    super();
    this._buffer = Buffer.alloc(0);
  }

  feed(chunk) {
    this._buffer = Buffer.concat([this._buffer, Buffer.from(chunk)]);
    const result = ubx.parse(this._buffer);
    const consumed = result.consumed || 0;
    if (consumed > 0) {
      this._buffer = this._buffer.subarray(consumed);
    }
    const messages = [...result];
    for (const msg of messages) {
      this.emit(msg.name, msg);
      this.emit('message', msg);
    }
    return messages;
  }

  static parse(data) {
    return [...ubx.parse(data)];
  }

  static schema() {
    return ubx.schema();
  }
}

const NativeUbxLog = ubx.UbxLog;

class UbxLog {
  /** @type {import('../types').NativeUbxLog} */
  _native;

  constructor(native) {
    this._native = native;
  }

  static async open(filePath) {
    const native = await NativeUbxLog.open(filePath);
    return new UbxLog(native);
  }

  count(name) {
    return name !== undefined ? this._native.count(name) : this._native.count();
  }

  messageTypes() {
    return this._native.messageTypes();
  }

  async next(name) {
    const result = name !== undefined ? this._native.next(name) : this._native.next();
    return result instanceof Promise ? await result : result;
  }

  async prev(name) {
    const result = name !== undefined ? this._native.prev(name) : this._native.prev();
    return result instanceof Promise ? await result : result;
  }

  seek(pos) {
    this._native.seek(pos);
  }

  async get(name, ordinal) {
    const result = this._native.get(name, ordinal);
    return result instanceof Promise ? await result : result;
  }

  close() {
    this._native.close();
  }
}

module.exports = { UbxParser, UbxLog };
