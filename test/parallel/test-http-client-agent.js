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
require('../common');
const assert = require('assert');
const http = require('http');

let name;
const max = 3;
let count = 0;

const server = http.Server(function(req, res) {
  if (req.url === '/0') {
    setTimeout(function() {
      res.writeHead(200);
      res.end('Hello, World!');
    }, 100);
  } else {
    res.writeHead(200);
    res.end('Hello, World!');
  }
});
server.listen(0, function() {
  name = http.globalAgent.getName({ port: this.address().port });
  for (let i = 0; i < max; ++i) {
    request(i);
  }
});

function request(i) {
  const req = http.get({
    port: server.address().port,
    path: `/${i}`
  }, function(res) {
    const socket = req.socket;
    socket.on('close', function() {
      ++count;
      if (count < max) {
        assert.strictEqual(http.globalAgent.sockets[name].includes(socket),
                           false);
      } else {
        assert(!http.globalAgent.sockets.hasOwnProperty(name));
        assert(!http.globalAgent.requests.hasOwnProperty(name));
        server.close();
      }
    });
    res.resume();
  });
}

process.on('exit', function() {
  assert.strictEqual(count, max);
});
