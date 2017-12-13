'use strict';

const common = require('../common.js');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  method: ['copy', 'rest', 'arguments'],
  millions: [100]
});

function copyArguments() {
  const len = arguments.length;
  const args = new Array(len);
  for (var i = 0; i < len; i++)
    args[i] = arguments[i];
  assert.strictEqual(args[0], 1);
  assert.strictEqual(args[1], 2);
  assert.strictEqual(args[2], 'a');
  assert.strictEqual(args[3], 'b');
}

function restArguments(...args) {
  assert.strictEqual(args[0], 1);
  assert.strictEqual(args[1], 2);
  assert.strictEqual(args[2], 'a');
  assert.strictEqual(args[3], 'b');
}

function useArguments() {
  assert.strictEqual(arguments[0], 1);
  assert.strictEqual(arguments[1], 2);
  assert.strictEqual(arguments[2], 'a');
  assert.strictEqual(arguments[3], 'b');
}

function runCopyArguments(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    copyArguments(1, 2, 'a', 'b');
  bench.end(n / 1e6);
}

function runRestArguments(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    restArguments(1, 2, 'a', 'b');
  bench.end(n / 1e6);
}

function runUseArguments(n) {

  var i = 0;
  bench.start();
  for (; i < n; i++)
    useArguments(1, 2, 'a', 'b');
  bench.end(n / 1e6);
}

function main(conf) {
  const n = +conf.millions * 1e6;

  switch (conf.method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'copy':
      runCopyArguments(n);
      break;
    case 'rest':
      runRestArguments(n);
      break;
    case 'arguments':
      runUseArguments(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
