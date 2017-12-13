// When calling .end(buffer) right away, this triggers a "hot path"
// optimization in http.js, to avoid an extra write call.
//
// However, the overhead of copying a large buffer is higher than
// the overhead of an extra write() call, so the hot path was not
// always as hot as it could be.
//
// Verify that our assumptions are valid.
'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [1, 4, 8, 16],
  len: [1, 64, 256],
  c: [100]
});

function main(conf) {
  const http = require('http');
  const chunk = Buffer.alloc(conf.len, '8');

  const server = http.createServer(function(req, res) {
    function send(left) {
      if (left === 0) return res.end();
      res.write(chunk);
      setTimeout(function() {
        send(left - 1);
      }, 0);
    }
    send(conf.n);
  });

  server.listen(common.PORT, function() {
    bench.http({
      connections: conf.c
    }, function() {
      server.close();
    });
  });
}
