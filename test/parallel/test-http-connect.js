// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const server = http.createServer(common.mustNotCall());

server.on('connect', common.mustCall((req, socket, firstBodyChunk) => {
  assert.strictEqual(req.method, 'CONNECT');
  assert.strictEqual(req.url, 'google.com:443');

  socket.write('HTTP/1.1 200 Connection established\r\n\r\n');

  let data = firstBodyChunk.toString();
  socket.on('data', (buf) => {
    data += buf.toString();
  });

  socket.on('end', common.mustCall(() => {
    socket.end(data);
  }));
}));

server.listen(0, common.mustCall(function() {
  const req = http.request({
    port: this.address().port,
    method: 'CONNECT',
    path: 'google.com:443'
  }, common.mustNotCall());

  req.on('close', common.mustCall());

  req.on('connect', common.mustCall((res, socket, firstBodyChunk) => {
    // Make sure this request got removed from the pool.
    const name = `localhost:${server.address().port}`;
    assert(!http.globalAgent.sockets.hasOwnProperty(name));
    assert(!http.globalAgent.requests.hasOwnProperty(name));

    // Make sure this socket has detached.
    assert(!socket.ondata);
    assert(!socket.onend);
    assert.strictEqual(socket.listeners('connect').length, 0);
    assert.strictEqual(socket.listeners('data').length, 0);

    // the stream.Duplex onend listener
    // allow 0 here, so that i can run the same test on streams1 impl
    assert(socket.listeners('end').length <= 1);

    assert.strictEqual(socket.listeners('free').length, 0);
    assert.strictEqual(socket.listeners('close').length, 0);
    assert.strictEqual(socket.listeners('error').length, 0);
    assert.strictEqual(socket.listeners('agentRemove').length, 0);

    let data = firstBodyChunk.toString();
    socket.on('data', (buf) => {
      data += buf.toString();
    });

    socket.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'HeadBody');
      server.close();
    }));

    socket.write('Body');
    socket.end();
  }));

  // It is legal for the client to send some data intended for the server
  // before the "200 Connection established" (or any other success or
  // error code) is received.
  req.write('Head');
  req.end();
}));
