'use strict';
// just like test/gc/http-client-timeout.js,
// but using a net server/client instead

const common = require('../common');

function serverHandler(sock) {
  sock.setTimeout(120000);
  sock.resume();
  sock.on('close', function() {
    clearTimeout(timer);
  });
  sock.on('error', function(err) {
    assert.strictEqual(err.code, 'ECONNRESET');
  });
  const timer = setTimeout(function() {
    sock.end('hello\n');
  }, 100);
}

const net = require('net');
const weak = require(`./build/${common.buildType}/binding`);
const assert = require('assert');
const todo = 500;
let done = 0;
let count = 0;
let countGC = 0;

console.log(`We should do ${todo} requests`);

const server = net.createServer(serverHandler);
server.listen(0, getall);

function getall() {
  if (count >= todo)
    return;

  const req = net.connect(server.address().port);
  req.resume();
  req.setTimeout(10, function() {
    req.destroy();
    done++;
    global.gc();
  });

  count++;
  weak(req, afterGC);

  setImmediate(getall);
}

for (let i = 0; i < 10; i++)
  getall();

function afterGC() {
  countGC++;
}

setInterval(status, 100).unref();

function status() {
  global.gc();
  console.log('Done: %d/%d', done, todo);
  console.log('Collected: %d/%d', countGC, count);
  if (countGC === todo) server.close();
}
