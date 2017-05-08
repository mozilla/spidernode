'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const async_wrap = process.binding('async_wrap');
const uv = process.binding('uv');
const assert = require('assert');
const dgram = require('dgram');
const fs = require('fs');
const net = require('net');
const tls = require('tls');
const providers = Object.keys(async_wrap.Providers);
let flags = 0;

// Make sure all asserts have run at least once.
process.on('exit', () => assert.strictEqual(flags, 0b111));

function init(id, provider) {
  this._external;  // Test will abort if nullptr isn't properly checked.
  switch (providers[provider]) {
    case 'TCPWRAP':
      assert.strictEqual(this.fd, uv.UV_EINVAL);
      flags |= 0b1;
      break;
    case 'TLSWRAP':
      assert.strictEqual(this.fd, uv.UV_EINVAL);
      flags |= 0b10;
      break;
    case 'UDPWRAP':
      assert.strictEqual(this.fd, uv.UV_EBADF);
      flags |= 0b100;
      break;
  }
}

async_wrap.setupHooks({ init });
async_wrap.enable();

const checkTLS = common.mustCall(function checkTLS() {
  const options = {
    key: fs.readFileSync(`${common.fixturesDir}/keys/ec-key.pem`),
    cert: fs.readFileSync(`${common.fixturesDir}/keys/ec-cert.pem`)
  };
  const server = tls.createServer(options, common.noop)
    .listen(0, function() {
      const connectOpts = { rejectUnauthorized: false };
      tls.connect(this.address().port, connectOpts, function() {
        this.destroy();
        server.close();
      });
    });
});

const checkTCP = common.mustCall(function checkTCP() {
  net.createServer(common.noop).listen(0, function() {
    this.close(checkTLS);
  });
});

dgram.createSocket('udp4').close(checkTCP);
