'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

// Http2ServerRequest should always end readable stream
// even on GET requests with no body

const server = h2.createServer();
server.listen(0, common.mustCall(function() {
  const port = server.address().port;
  server.once('request', common.mustCall(function(request, response) {
    assert.strictEqual(request.complete, false);
    request.on('data', () => {});
    request.on('end', common.mustCall(() => {
      assert.strictEqual(request.complete, true);
      response.on('finish', common.mustCall(function() {
        // the following tests edge cases on request socket
        // right after finished fires but before backing
        // Http2Stream is destroyed
        assert.strictEqual(request.socket.readable, request.stream.readable);
        assert.strictEqual(request.socket.readable, false);

        server.close();
      }));
      response.end();
    }));
  }));

  const url = `http://localhost:${port}`;
  const client = h2.connect(url, common.mustCall(function() {
    const headers = {
      ':path': '/foobar',
      ':method': 'GET',
      ':scheme': 'http',
      ':authority': `localhost:${port}`
    };
    const request = client.request(headers);
    request.resume();
    request.on('end', common.mustCall(function() {
      client.destroy();
    }));
    request.end();
  }));
}));
