'use strict';

const common = require('../common');

if (!common.enoughTestMem) {
  common.skip('Insufficient memory for HTTP benchmark test');
  return;
}

// Minimal test for http benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

// Because the http benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

const child = fork(runjs, ['--set', 'dur=0.1',
                           '--set', 'n=1',
                           '--set', 'len=1',
                           '--set', 'c=1',
                           '--set', 'chunks=0',
                           '--set', 'benchmarker=test-double',
                           'http'],
                   {env: {NODEJS_BENCHMARK_ZERO_ALLOWED: 1}});
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
