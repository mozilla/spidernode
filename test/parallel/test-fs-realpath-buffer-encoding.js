'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const string_dir = fs.realpathSync(common.fixturesDir);
const buffer_dir = Buffer.from(string_dir);

const encodings = ['ascii', 'utf8', 'utf16le', 'ucs2',
                   'base64', 'binary', 'hex'];
const expected = {};
encodings.forEach((encoding) => {
  expected[encoding] = buffer_dir.toString(encoding);
});


// test sync version
let encoding;
for (encoding in expected) {
  const expected_value = expected[encoding];
  let result;

  result = fs.realpathSync(string_dir, { encoding: encoding });
  assert.strictEqual(result, expected_value);

  result = fs.realpathSync(string_dir, encoding);
  assert.strictEqual(result, expected_value);

  result = fs.realpathSync(buffer_dir, { encoding: encoding });
  assert.strictEqual(result, expected_value);

  result = fs.realpathSync(buffer_dir, encoding);
  assert.strictEqual(result, expected_value);
}

let buffer_result;
buffer_result = fs.realpathSync(string_dir, { encoding: 'buffer' });
assert.deepStrictEqual(buffer_result, buffer_dir);

buffer_result = fs.realpathSync(string_dir, 'buffer');
assert.deepStrictEqual(buffer_result, buffer_dir);

buffer_result = fs.realpathSync(buffer_dir, { encoding: 'buffer' });
assert.deepStrictEqual(buffer_result, buffer_dir);

buffer_result = fs.realpathSync(buffer_dir, 'buffer');
assert.deepStrictEqual(buffer_result, buffer_dir);

// test async version
for (encoding in expected) {
  const expected_value = expected[encoding];

  fs.realpath(
    string_dir,
    { encoding: encoding },
    common.mustCall((err, res) => {
      assert.ifError(err);
      assert.strictEqual(res, expected_value);
    })
  );
  fs.realpath(string_dir, encoding, common.mustCall((err, res) => {
    assert.ifError(err);
    assert.strictEqual(res, expected_value);
  }));
  fs.realpath(
    buffer_dir,
    { encoding: encoding },
    common.mustCall((err, res) => {
      assert.ifError(err);
      assert.strictEqual(res, expected_value);
    })
  );
  fs.realpath(buffer_dir, encoding, common.mustCall((err, res) => {
    assert.ifError(err);
    assert.strictEqual(res, expected_value);
  }));
}

fs.realpath(string_dir, { encoding: 'buffer' }, common.mustCall((err, res) => {
  assert.ifError(err);
  assert.deepStrictEqual(res, buffer_dir);
}));

fs.realpath(string_dir, 'buffer', common.mustCall((err, res) => {
  assert.ifError(err);
  assert.deepStrictEqual(res, buffer_dir);
}));

fs.realpath(buffer_dir, { encoding: 'buffer' }, common.mustCall((err, res) => {
  assert.ifError(err);
  assert.deepStrictEqual(res, buffer_dir);
}));

fs.realpath(buffer_dir, 'buffer', common.mustCall((err, res) => {
  assert.ifError(err);
  assert.deepStrictEqual(res, buffer_dir);
}));
