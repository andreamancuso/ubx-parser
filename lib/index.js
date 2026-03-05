'use strict';

const { EventEmitter } = require('events');
const addon = require('bindings')('ubx-parser');

class UbxParser extends EventEmitter {
  constructor() {
    super();
    this._buffer = Buffer.alloc(0);
  }

  feed(chunk) {
    this._buffer = Buffer.concat([this._buffer, Buffer.from(chunk)]);
    const result = addon.parse(this._buffer);
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
    return [...addon.parse(data)];
  }

  static schema() {
    return addon.schema();
  }
}

module.exports = { UbxParser };
