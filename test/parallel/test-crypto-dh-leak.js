// Flags: --expose-gc
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

const before = process.memoryUsage().rss;
{
  const dh = crypto.createDiffieHellman(common.hasFipsCrypto ? 1024 : 256);
  const publicKey = dh.generateKeys();
  const privateKey = dh.getPrivateKey();
  for (let i = 0; i < 5e4; i += 1) {
    dh.setPublicKey(publicKey);
    dh.setPrivateKey(privateKey);
  }
}
global.gc();
const after = process.memoryUsage().rss;

// RSS should stay the same, ceteris paribus, but allow for
// some slop because V8 mallocs memory during execution.
assert(after - before < 5 << 20);
