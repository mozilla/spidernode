'use strict';
require('../common');

// This test ensures that the stream implementation correctly handles values
// for highWaterMark which exceed the range of signed 32 bit integers.

const assert = require('assert');
const stream = require('stream');

// This number exceeds the range of 32 bit integer arithmetic but should still
// be handled correctly.
const ovfl = Number.MAX_SAFE_INTEGER;

const readable = stream.Readable({ highWaterMark: ovfl });
assert.strictEqual(readable._readableState.highWaterMark, ovfl);

const writable = stream.Writable({ highWaterMark: ovfl });
assert.strictEqual(writable._writableState.highWaterMark, ovfl);
