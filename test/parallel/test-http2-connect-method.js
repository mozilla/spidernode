'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const net = require('net');
const http2 = require('http2');
const { URL } = require('url');

const {
  HTTP2_HEADER_METHOD,
  HTTP2_HEADER_AUTHORITY,
  HTTP2_HEADER_SCHEME,
  HTTP2_HEADER_PATH,
  NGHTTP2_CONNECT_ERROR
} = http2.constants;

const server = net.createServer(common.mustCall((socket) => {
  let data = '';
  socket.setEncoding('utf8');
  socket.on('data', (chunk) => data += chunk);
  socket.on('end', common.mustCall(() => {
    assert.strictEqual(data, 'hello');
  }));
  socket.on('close', common.mustCall());
  socket.end('hello');
}));

server.listen(0, common.mustCall(() => {

  const port = server.address().port;

  const proxy = http2.createServer();
  proxy.on('stream', common.mustCall((stream, headers) => {
    if (headers[HTTP2_HEADER_METHOD] !== 'CONNECT') {
      stream.rstWithRefused();
      return;
    }
    const auth = new URL(`tcp://${headers[HTTP2_HEADER_AUTHORITY]}`);
    assert.strictEqual(auth.hostname, 'localhost');
    assert.strictEqual(+auth.port, port);
    const socket = net.connect(auth.port, auth.hostname, () => {
      stream.respond();
      socket.pipe(stream);
      stream.pipe(socket);
    });
    socket.on('close', common.mustCall());
    socket.on('error', (error) => {
      stream.rstStream(NGHTTP2_CONNECT_ERROR);
    });
  }));

  proxy.listen(0, () => {
    const client = http2.connect(`http://localhost:${proxy.address().port}`);

    // confirm that :authority is required and :scheme & :path are forbidden
    common.expectsError(
      () => client.request({
        [HTTP2_HEADER_METHOD]: 'CONNECT'
      }),
      {
        code: 'ERR_HTTP2_CONNECT_AUTHORITY',
        message: ':authority header is required for CONNECT requests'
      }
    );
    common.expectsError(
      () => client.request({
        [HTTP2_HEADER_METHOD]: 'CONNECT',
        [HTTP2_HEADER_AUTHORITY]: `localhost:${port}`,
        [HTTP2_HEADER_SCHEME]: 'http'
      }),
      {
        code: 'ERR_HTTP2_CONNECT_SCHEME',
        message: 'The :scheme header is forbidden for CONNECT requests'
      }
    );
    common.expectsError(
      () => client.request({
        [HTTP2_HEADER_METHOD]: 'CONNECT',
        [HTTP2_HEADER_AUTHORITY]: `localhost:${port}`,
        [HTTP2_HEADER_PATH]: '/'
      }),
      {
        code: 'ERR_HTTP2_CONNECT_PATH',
        message: 'The :path header is forbidden for CONNECT requests'
      }
    );

    // valid CONNECT request
    const req = client.request({
      [HTTP2_HEADER_METHOD]: 'CONNECT',
      [HTTP2_HEADER_AUTHORITY]: `localhost:${port}`,
    });

    req.on('response', common.mustCall());
    let data = '';
    req.setEncoding('utf8');
    req.on('data', (chunk) => data += chunk);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(data, 'hello');
      client.destroy();
      proxy.close();
      server.close();
    }));
    req.end('hello');
  });
}));
