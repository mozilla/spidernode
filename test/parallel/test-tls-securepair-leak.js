// Flags: --expose-gc --no-deprecation
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { createSecureContext } = require('tls');
const { createSecurePair } = require('_tls_legacy');

const before = process.memoryUsage().external;
{
  const context = createSecureContext();
  const options = {};
  for (let i = 0; i < 1e4; i += 1)
    createSecurePair(context, false, false, false, options).destroy();
}
global.gc();
const after = process.memoryUsage().external;

// It's not an exact science but a SecurePair grows .external by about 45 kB.
// Unless AdjustAmountOfExternalAllocatedMemory() is called on destruction,
// 10,000 instances make it grow by well over 400 MB.  Allow for some slop
// because objects like buffers also affect the external limit.
assert(after - before < 25 << 20);
