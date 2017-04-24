'use strict';

const common = require('../common');
const assert = require('assert');
const cluster = require('cluster');
const net = require('net');

let serverClosed = false;

if (cluster.isWorker) {
  const server = net.createServer(function(socket) {
    // Wait for any data, then close connection
    socket.write('.');
    socket.on('data', common.noop);
  }).listen(0, common.localhostIPv4);

  server.once('close', function() {
    serverClosed = true;
  });

  // Although not typical, the worker process can exit before the disconnect
  // event fires. Use this to keep the process open until the event has fired.
  const keepOpen = setInterval(common.noop, 9999);

  // Check worker events and properties
  process.once('disconnect', function() {
    // disconnect should occur after socket close
    assert(serverClosed);
    clearInterval(keepOpen);
  });
} else if (cluster.isMaster) {
  // start worker
  const worker = cluster.fork();

  // Disconnect worker when it is ready
  worker.once('listening', function(address) {
    const socket = net.createConnection(address.port, common.localhostIPv4);

    socket.on('connect', function() {
      socket.on('data', function() {
        console.log('got data from client');
        // socket definitely connected to worker if we got data
        worker.disconnect();
        socket.end();
      });
    });
  });
}
