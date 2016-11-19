'use strict';
var common = require('../common');
var path = require('path');
var assert = require('assert');

try {
  require(path.join(common.fixturesDir, 'invalid.json'));
} catch (err) {
  var re = common.engineSpecificMessage({
    v8: /test[/\\]fixtures[/\\]invalid.json: Unexpected string/,
    chakracore:
    /test[\/\\]fixtures[\/\\]invalid.json: JSON.parse Error: Expected '}'/
  });
  var i = err.message.match(re);
  assert.notStrictEqual(null, i, 'require() json error should include path');
}
