'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const spawn = require('child_process').spawn;
const defaultCoreList = require('crypto').constants.defaultCoreCipherList;

function doCheck(arg, check) {
  let out = '';
  arg = arg.concat([
    '-pe',
    'require("crypto").constants.defaultCipherList'
  ]);
  spawn(process.execPath, arg, {})
    .on('error', common.mustNotCall())
    .stdout.on('data', function(chunk) {
      out += chunk;
    }).on('end', function() {
      assert.strictEqual(out.trim(), check);
    }).on('error', common.mustNotCall());
}

// test the default unmodified version
doCheck([], defaultCoreList);

// test the command line switch by itself
doCheck(['--tls-cipher-list=ABC'], 'ABC');
