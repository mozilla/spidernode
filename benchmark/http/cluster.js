'use strict';
const common = require('../common.js');
const PORT = common.PORT;

const cluster = require('cluster');
if (cluster.isMaster) {
  var bench = common.createBenchmark(main, {
    // unicode confuses ab on os x.
    type: ['bytes', 'buffer'],
    len: [4, 1024, 102400],
    c: [50, 500]
  });
} else {
  const port = parseInt(process.env.PORT || PORT);
  require('../fixtures/simple-http-server.js').listen(port);
}

function main(conf) {
  process.env.PORT = PORT;
  var workers = 0;
  const w1 = cluster.fork();
  const w2 = cluster.fork();

  cluster.on('listening', function() {
    workers++;
    if (workers < 2)
      return;

    setTimeout(function() {
      const path = `/${conf.type}/${conf.len}`;

      bench.http({
        path: path,
        connections: conf.c
      }, function() {
        w1.destroy();
        w2.destroy();
      });
    }, 100);
  });
}
