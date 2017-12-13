// Measure the time it takes for the HTTP client to send a request body.
'use strict';

const common = require('../common.js');
const http = require('http');

const bench = common.createBenchmark(main, {
  dur: [5],
  type: ['asc', 'utf', 'buf'],
  len: [32, 256, 1024],
  method: ['write', 'end']
});

function main(conf) {
  const dur = +conf.dur;
  const len = +conf.len;

  var encoding;
  var chunk;
  switch (conf.type) {
    case 'buf':
      chunk = Buffer.alloc(len, 'x');
      break;
    case 'utf':
      encoding = 'utf8';
      chunk = 'ü'.repeat(len / 2);
      break;
    case 'asc':
      chunk = 'a'.repeat(len);
      break;
  }

  var nreqs = 0;
  const options = {
    headers: { 'Connection': 'keep-alive', 'Transfer-Encoding': 'chunked' },
    agent: new http.Agent({ maxSockets: 1 }),
    host: '127.0.0.1',
    port: common.PORT,
    path: '/',
    method: 'POST'
  };

  const server = http.createServer(function(req, res) {
    res.end();
  });
  server.listen(options.port, options.host, function() {
    setTimeout(done, dur * 1000);
    bench.start();
    pummel();
  });

  function pummel() {
    const req = http.request(options, function(res) {
      nreqs++;
      pummel();  // Line up next request.
      res.resume();
    });
    if (conf.method === 'write') {
      req.write(chunk, encoding);
      req.end();
    } else {
      req.end(chunk, encoding);
    }
  }

  function done() {
    bench.end(nreqs);
    process.exit(0);
  }
}
