'use strict';
const common = require('../../common');
var assert = require('assert');
const binding = require(`./build/${common.buildType}/binding`);
assert.equal('world', binding());
console.log('binding.hello() =', binding());
