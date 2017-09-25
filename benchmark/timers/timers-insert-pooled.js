'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  thousands: [500],
});

function main(conf) {
  const iterations = +conf.thousands * 1e3;

  bench.start();

  for (var i = 0; i < iterations; i++) {
    setTimeout(() => {}, 1);
  }

  bench.end(iterations / 1e3);
}
