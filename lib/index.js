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

module.exports = { UbxParser };
