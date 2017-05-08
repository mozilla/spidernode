'use strict';
require('../common');
const assert = require('assert');
const http = require('http');

const agent = new http.Agent({
  keepAlive: true,
  keepAliveMsecs: 1000,
  maxSockets: 2,
  maxFreeSockets: 2
});

const server = http.createServer(function(req, res) {
  res.end('hello world');
});

function get(path, callback) {
  return http.get({
    host: 'localhost',
    port: server.address().port,
    agent: agent,
    path: path
  }, callback);
}

let count = 0;
function done() {
  if (++count !== 2) {
    return;
  }
  const freepool = agent.freeSockets[Object.keys(agent.freeSockets)[0]];
  assert.strictEqual(freepool.length, 2,
                     `expect keep 2 free sockets, but got ${freepool.length}`);
  agent.destroy();
  server.close();
}

server.listen(0, function() {
  get('/1', function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', function() {
      process.nextTick(done);
    });
  });

  get('/2', function(res) {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
    res.on('end', function() {
      process.nextTick(done);
    });
  });
});
