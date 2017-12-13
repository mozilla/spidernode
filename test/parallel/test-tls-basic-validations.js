'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const tls = require('tls');

assert.throws(() => tls.createSecureContext({ ciphers: 1 }),
              /TypeError: Ciphers must be a string/);

assert.throws(() => tls.createServer({ ciphers: 1 }),
              /TypeError: Ciphers must be a string/);

assert.throws(() => tls.createSecureContext({ key: 'dummykey', passphrase: 1 }),
              /TypeError: Pass phrase must be a string/);

assert.throws(() => tls.createServer({ key: 'dummykey', passphrase: 1 }),
              /TypeError: Pass phrase must be a string/);

assert.throws(() => tls.createServer({ ecdhCurve: 1 }),
              /TypeError: ECDH curve name must be a string/);

common.expectsError(() => tls.createServer({ handshakeTimeout: 'abcd' }),
                    {
                      code: 'ERR_INVALID_ARG_TYPE',
                      type: TypeError,
                      message: 'The "timeout" argument must be of type number'
                    }
);

assert.throws(() => tls.createServer({ sessionTimeout: 'abcd' }),
              /TypeError: Session timeout must be a 32-bit integer/);

assert.throws(() => tls.createServer({ ticketKeys: 'abcd' }),
              /TypeError: Ticket keys must be a buffer/);

assert.throws(() => tls.createServer({ ticketKeys: Buffer.alloc(0) }),
              /TypeError: Ticket keys length must be 48 bytes/);

assert.throws(() => tls.createSecurePair({}),
              /Error: First argument must be a tls module SecureContext/);

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertALPNProtocols(buffer, out);
  out.ALPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.ALPNProtocols.equals(Buffer.from('efgh')));
}

{
  const buffer = Buffer.from('abcd');
  const out = {};
  tls.convertNPNProtocols(buffer, out);
  out.NPNProtocols.write('efgh');
  assert(buffer.equals(Buffer.from('abcd')));
  assert(out.NPNProtocols.equals(Buffer.from('efgh')));
}

{
  const buffer = new Uint8Array(Buffer.from('abcd'));
  const out = {};
  tls.convertALPNProtocols(buffer, out);
  assert(out.ALPNProtocols.equals(Buffer.from('abcd')));
}

{
  const buffer = new Uint8Array(Buffer.from('abcd'));
  const out = {};
  tls.convertNPNProtocols(buffer, out);
  assert(out.NPNProtocols.equals(Buffer.from('abcd')));
}
