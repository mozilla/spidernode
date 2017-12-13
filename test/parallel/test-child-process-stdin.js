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

const spawn = require('child_process').spawn;

const cat = spawn('cat');
cat.stdin.write('hello');
cat.stdin.write(' ');
cat.stdin.write('world');

assert.strictEqual(true, cat.stdin.writable);
assert.strictEqual(false, cat.stdin.readable);

cat.stdin.end();

let response = '';

cat.stdout.setEncoding('utf8');
cat.stdout.on('data', function(chunk) {
  console.log(`stdout: ${chunk}`);
  response += chunk;
});

cat.stdout.on('end', common.mustCall());

cat.stderr.on('data', common.mustNotCall());

cat.stderr.on('end', common.mustCall());

cat.on('exit', common.mustCall(function(status) {
  assert.strictEqual(0, status);
}));

cat.on('close', common.mustCall(function() {
  assert.strictEqual('hello world', response);
}));
