'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const crypto = require('crypto');

assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('foo')),
  true,
  'should consider equal strings to be equal'
);

assert.strictEqual(
  crypto.timingSafeEqual(Buffer.from('foo'), Buffer.from('bar')),
  false,
  'should consider unequal strings to be unequal'
);

assert.throws(function() {
  crypto.timingSafeEqual(Buffer.from([1, 2, 3]), Buffer.from([1, 2]));
}, 'should throw when given buffers with different lengths');

assert.throws(function() {
  crypto.timingSafeEqual('not a buffer', Buffer.from([1, 2]));
}, 'should throw if the first argument is not a buffer');

assert.throws(function() {
  crypto.timingSafeEqual(Buffer.from([1, 2]), 'not a buffer');
}, 'should throw if the second argument is not a buffer');
