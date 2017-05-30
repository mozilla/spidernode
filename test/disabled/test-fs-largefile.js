// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

common.refreshTmpDir();

const filepath = path.join(common.tmpDir, 'large.txt');
const fd = fs.openSync(filepath, 'w+');
const offset = 5 * 1024 * 1024 * 1024; // 5GB
const message = 'Large File';

fs.truncateSync(fd, offset);
assert.strictEqual(fs.statSync(filepath).size, offset);
var writeBuf = Buffer.from(message);
fs.writeSync(fd, writeBuf, 0, writeBuf.length, offset);
var readBuf = Buffer.allocUnsafe(writeBuf.length);
fs.readSync(fd, readBuf, 0, readBuf.length, offset);
assert.strictEqual(readBuf.toString(), message);
fs.readSync(fd, readBuf, 0, 1, 0);
assert.strictEqual(readBuf[0], 0);

assert.doesNotThrow(
  () => { fs.writeSync(fd, writeBuf, 0, writeBuf.length, 42.000001); }
);
fs.close(fd);

// Normally, we don't clean up tmp files at the end of a test, but we'll make an
// exception for a 5 GB file.
process.on('exit', function() {
  fs.unlinkSync(filepath);
});
