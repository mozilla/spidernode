'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

const bench = common.createBenchmark(main, { n: [5e7] });

function main(conf) {
  const n = conf.n | 0;

  const ee = new EventEmitter();

  for (var k = 0; k < 5; k += 1) {
    ee.on('dummy0', function() {});
    ee.on('dummy1', function() {});
  }

  bench.start();
  for (var i = 0; i < n; i += 1) {
    const dummy = (i % 2 === 0) ? 'dummy0' : 'dummy1';
    ee.listenerCount(dummy);
  }
  bench.end(n);
}
