'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');

const buf = Buffer.from('test');

const onMessage = common.mustCall((err, bytes) => {
  assert.ifError(err);
  assert.strictEqual(bytes, buf.length);
}, 6);

const expectedError =
  /^TypeError: Invalid arguments: address must be a nonempty string or falsy$/;

const client = dgram.createSocket('udp4').bind(0, () => {
  const port = client.address().port;

  // valid address: false
  client.send(buf, port, false, onMessage);

  // valid address: empty string
  client.send(buf, port, '', onMessage);

  // valid address: null
  client.send(buf, port, null, onMessage);

  // valid address: 0
  client.send(buf, port, 0, onMessage);

  // valid address: undefined
  client.send(buf, port, undefined, onMessage);

  // valid address: not provided
  client.send(buf, port, onMessage);

  // invalid address: object
  assert.throws(() => {
    client.send(buf, port, []);
  }, expectedError);

  // invalid address: nonzero number
  assert.throws(() => {
    client.send(buf, port, 1);
  }, expectedError);

  // invalid address: true
  assert.throws(() => {
    client.send(buf, port, true);
  }, expectedError);
});

client.unref();
