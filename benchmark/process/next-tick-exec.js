'use strict';
const common = require('../common.js');
const bench = common.createBenchmark(main, {
  millions: [5]
});

function main(conf) {
  var n = +conf.millions * 1e6;

  bench.start();
  for (var i = 0; i < n; i++) {
    process.nextTick(onNextTick, i);
  }
  function onNextTick(i) {
    if (i + 1 === n)
      bench.end(+conf.millions);
  }
}
