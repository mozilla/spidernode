'use strict';
const common = require('../common.js');
const path = require('path');

const bench = common.createBenchmark(main, {
  path: [
    '',
    '.',
    '//server',
    'C:\\baz\\..',
    'C:baz\\..',
    'bar\\baz'
  ],
  n: [1e6]
});

function main(conf) {
  const n = +conf.n;
  const p = path.win32;
  const input = String(conf.path);

  bench.start();
  for (var i = 0; i < n; i++) {
    p.isAbsolute(input);
  }
  bench.end(n);
}
