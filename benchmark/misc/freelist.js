'use strict';

var common = require('../common.js');

var bench = common.createBenchmark(main, {
  n: [100000]
});

function main(conf) {
  // Using internal/freelist requires node to be run with --expose_internals
  // switch. common.js will do that when calling main(), so we require
  // this module here
  const FreeList = require('internal/freelist').FreeList;
  var n = conf.n;
  var poolSize = 1000;
  var list = new FreeList('test', poolSize, Object);
  var i;
  var j;
  var used = [];

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
