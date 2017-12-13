'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [100000]
}, {
  flags: ['--expose-internals']
});

function main(conf) {
  const FreeList = require('internal/freelist');
  const n = conf.n;
  const poolSize = 1000;
  const list = new FreeList('test', poolSize, Object);
  var i;
  var j;
  const used = [];

  // First, alloc `poolSize` items
  for (j = 0; j < poolSize; j++) {
    used.push(list.alloc());
  }

  bench.start();

  for (i = 0; i < n; i++) {
    // Return all the items to the pool
    for (j = 0; j < poolSize; j++) {
      list.free(used[j]);
    }

    // Re-alloc from pool
    for (j = 0; j < poolSize; j++) {
      list.alloc();
    }
  }

  bench.end(n);
}
