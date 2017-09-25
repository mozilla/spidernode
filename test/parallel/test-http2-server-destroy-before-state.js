// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const server = http2.createServer();

// Test that stream.state getter returns and empty object
// if the stream session has been destroyed
server.on('stream', common.mustCall(onStream));

function onStream(stream, headers, flags) {
  stream.session.destroy();
  assert.deepStrictEqual(Object.create(null), stream.state);
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });

  req.on('response', common.mustNotCall());
  req.resume();
  req.on('end', common.mustCall(() => {
    server.close();
    client.destroy();
  }));
  req.end();

}));
