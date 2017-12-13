'use strict';
const common = require('../common.js');
const querystring = require('querystring');
const inputs = require('../fixtures/url-inputs.js').searchParams;

const bench = common.createBenchmark(main, {
  type: Object.keys(inputs),
  n: [1e6],
});

function main(conf) {
  const type = conf.type;
  const n = conf.n | 0;
  const input = inputs[type];
  var i;
  // Execute the function a "sufficient" number of times before the timed
  // loop to ensure the function is optimized just once.
  if (type === 'multicharsep') {
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');

    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input, '&&&&&&&&&&');
    bench.end(n);
  } else {
    for (i = 0; i < n; i += 1)
      querystring.parse(input);

    bench.start();
    for (i = 0; i < n; i += 1)
      querystring.parse(input);
    bench.end(n);
  }
}
