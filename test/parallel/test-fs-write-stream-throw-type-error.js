'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');

const example = path.join(common.tmpDir, 'dummy');

common.refreshTmpDir();

assert.doesNotThrow(function() {
  fs.createWriteStream(example, undefined);
});
assert.doesNotThrow(function() {
  fs.createWriteStream(example, null);
});
assert.doesNotThrow(function() {
  fs.createWriteStream(example, 'utf8');
});
assert.doesNotThrow(function() {
  fs.createWriteStream(example, {encoding: 'utf8'});
});

assert.throws(function() {
  fs.createWriteStream(example, 123);
}, /"options" must be a string or an object/);
assert.throws(function() {
  fs.createWriteStream(example, 0);
}, /"options" must be a string or an object/);
assert.throws(function() {
  fs.createWriteStream(example, true);
}, /"options" must be a string or an object/);
assert.throws(function() {
  fs.createWriteStream(example, false);
}, /"options" must be a string or an object/);
