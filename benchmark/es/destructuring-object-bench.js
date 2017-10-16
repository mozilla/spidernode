'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  method: ['normal', 'destructureObject'],
  millions: [100]
});

function runNormal(n) {
  var i = 0;
  const o = { x: 0, y: 1 };
  bench.start();
  for (; i < n; i++) {
    /* eslint-disable no-unused-vars */
    const x = o.x;
    const y = o.y;
    const r = o.r || 2;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n / 1e6);
}

function runDestructured(n) {
  var i = 0;
  const o = { x: 0, y: 1 };
  bench.start();
  for (; i < n; i++) {
    /* eslint-disable no-unused-vars */
    const { x, y, r = 2 } = o;
    /* eslint-enable no-unused-vars */
  }
  bench.end(n / 1e6);
}

function main(conf) {
  const n = +conf.millions * 1e6;

  switch (conf.method) {
    case '':
      // Empty string falls through to next line as default, mostly for tests.
    case 'normal':
      runNormal(n);
      break;
    case 'destructureObject':
      runDestructured(n);
      break;
    default:
      throw new Error('Unexpected method');
  }
}
