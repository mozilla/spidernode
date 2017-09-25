'use strict';
const common = require('../common.js');

const bench = common.createBenchmark(main, {
  thousands: [500],
});

function main(conf) {
  const iterations = +conf.thousands * 1e3;
  var count = 0;

  for (var i = 0; i < iterations; i++) {
    setTimeout(cb, 1);
  }

  bench.start();

  function cb() {
    count++;
    if (count === iterations)
      bench.end(iterations / 1e3);
  }
}
