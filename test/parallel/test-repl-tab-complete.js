// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const assert = require('assert');

// We have to change the directory to ../fixtures before requiring repl
// in order to make the tests for completion of node_modules work properly
// since repl modifies module.paths.
process.chdir(common.fixturesDir);

const repl = require('repl');

function getNoResultsFunction() {
  return common.mustCall((err, data) => {
    assert.ifError(err);
    assert.deepStrictEqual(data[0], []);
  });
}

const works = [['inner.one'], 'inner.o'];
const putIn = new common.ArrayStream();
const testMe = repl.start('', putIn);

// Some errors are passed to the domain, but do not callback
testMe._domain.on('error', function(err) {
  assert.ifError(err);
});

// Tab Complete will not break in an object literal
putIn.run(['.clear']);
putIn.run([
  'var inner = {',
  'one:1'
]);
testMe.complete('inner.o', getNoResultsFunction());

testMe.complete('console.lo', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
}));

// Tab Complete will return globally scoped variables
putIn.run(['};']);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will not break in an ternary operator with ()
putIn.run([
  'var inner = ( true ',
  '?',
  '{one: 1} : '
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// Tab Complete will return a simple local variable
putIn.run([
  'var top = function() {',
  'var inner = {one:1};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

// When you close the function scope tab complete will not return the
// locally scoped variable
putIn.run(['};']);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// Tab Complete will return a complex local variable
putIn.run([
  'var top = function() {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the function
// has parameters
putIn.run([
  'var top = function(one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the
// scope is nested inside an immediately executed function
putIn.run([
  'var top = function() {',
  '(function test () {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// def has the params and { on a separate line
putIn.run([
  'var top = function() {',
  'r = function test (',
  ' one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// currently does not work, but should not break, not the {
putIn.run([
  'var top = function() {',
  'r = function test ()',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// currently does not work, but should not break
putIn.run([
  'var top = function() {',
  'r = function test (',
  ')',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// make sure tab completion works on non-Objects
putIn.run([
  'var str = "test";'
]);
testMe.complete('str.len', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['str.length'], 'str.len']);
}));

putIn.run(['.clear']);

// tab completion should not break on spaces
const spaceTimeout = setTimeout(function() {
  throw new Error('timeout');
}, 1000);

testMe.complete(' ', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [[], undefined]);
  clearTimeout(spaceTimeout);
}));

// tab completion should pick up the global "toString" object, and
// any other properties up the "global" object's prototype chain
testMe.complete('toSt', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['toString'], 'toSt']);
}));

// Tab complete provides built in libs for require()
putIn.run(['.clear']);

testMe.complete('require(\'', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  repl._builtinLibs.forEach(function(lib) {
    assert.notStrictEqual(data[0].indexOf(lib), -1, `${lib} not found`);
  });
}));

testMe.complete('require(\'n', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  assert.strictEqual(data.length, 2);
  assert.strictEqual(data[1], 'n');
  assert.notStrictEqual(data[0].indexOf('net'), -1);
  // It's possible to pick up non-core modules too
  data[0].forEach(function(completion) {
    if (completion)
      assert(/^n/.test(completion));
  });
}));

{
  const expected = ['@nodejsscope', '@nodejsscope/'];
  putIn.run(['.clear']);
  testMe.complete('require(\'@nodejs', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.deepStrictEqual(data, [expected, '@nodejs']);
  }));
}

// Make sure tab completion works on context properties
putIn.run(['.clear']);

putIn.run([
  'var custom = "test";'
]);
testMe.complete('cus', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['custom'], 'cus']);
}));

// Make sure tab completion doesn't crash REPL with half-baked proxy objects.
// See: https://github.com/nodejs/node/issues/2119
putIn.run(['.clear']);

putIn.run([
  'var proxy = new Proxy({}, {ownKeys: () => { throw new Error(); }});'
]);

testMe.complete('proxy.', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  assert(Array.isArray(data));
}));

// Make sure tab completion does not include integer members of an Array
putIn.run(['.clear']);

putIn.run(['var ary = [1,2,3];']);
testMe.complete('ary.', common.mustCall(function(error, data) {
  assert.strictEqual(data[0].indexOf('ary.0'), -1);
  assert.strictEqual(data[0].indexOf('ary.1'), -1);
  assert.strictEqual(data[0].indexOf('ary.2'), -1);
}));

// Make sure tab completion does not include integer keys in an object
putIn.run(['.clear']);
putIn.run(['var obj = {1:"a","1a":"b",a:"b"};']);

testMe.complete('obj.', common.mustCall(function(error, data) {
  assert.strictEqual(data[0].indexOf('obj.1'), -1);
  assert.strictEqual(data[0].indexOf('obj.1a'), -1);
  assert.notStrictEqual(data[0].indexOf('obj.a'), -1);
}));

// Don't try to complete results of non-simple expressions
putIn.run(['.clear']);
putIn.run(['function a() {}']);

testMe.complete('a().b.', getNoResultsFunction());

// Works when prefixed with spaces
putIn.run(['.clear']);
putIn.run(['var obj = {1:"a","1a":"b",a:"b"};']);

testMe.complete(' obj.', common.mustCall((error, data) => {
  assert.strictEqual(data[0].indexOf('obj.1'), -1);
  assert.strictEqual(data[0].indexOf('obj.1a'), -1);
  assert.notStrictEqual(data[0].indexOf('obj.a'), -1);
}));

// Works inside assignments
putIn.run(['.clear']);

testMe.complete('var log = console.lo', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
}));

// tab completion for defined commands
putIn.run(['.clear']);

testMe.complete('.b', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['break'], 'b']);
}));

const testNonGlobal = repl.start({
  input: putIn,
  output: putIn,
  useGlobal: false
});

const builtins = [['Infinity', '', 'Int16Array', 'Int32Array',
                   'Int8Array'], 'I'];

if (common.hasIntl) {
  builtins[0].push('Intl');
}
testNonGlobal.complete('I', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, builtins);
}));

// To test custom completer function.
// Sync mode.
const customCompletions = 'aaa aa1 aa2 bbb bb1 bb2 bb3 ccc ddd eee'.split(' ');
const testCustomCompleterSyncMode = repl.start({
  prompt: '',
  input: putIn,
  output: putIn,
  completer: function completer(line) {
    const hits = customCompletions.filter((c) => c.startsWith(line));
    // Show all completions if none found.
    return [hits.length ? hits : customCompletions, line];
  }
});

// On empty line should output all the custom completions
// without complete anything.
testCustomCompleterSyncMode.complete('', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    customCompletions,
    ''
  ]);
}));

// On `a` should output `aaa aa1 aa2` and complete until `aa`.
testCustomCompleterSyncMode.complete('a', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    'aaa aa1 aa2'.split(' '),
    'a'
  ]);
}));

// To test custom completer function.
// Async mode.
const testCustomCompleterAsyncMode = repl.start({
  prompt: '',
  input: putIn,
  output: putIn,
  completer: function completer(line, callback) {
    const hits = customCompletions.filter((c) => c.startsWith(line));
    // Show all completions if none found.
    callback(null, [hits.length ? hits : customCompletions, line]);
  }
});

// On empty line should output all the custom completions
// without complete anything.
testCustomCompleterAsyncMode.complete('', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    customCompletions,
    ''
  ]);
}));

// On `a` should output `aaa aa1 aa2` and complete until `aa`.
testCustomCompleterAsyncMode.complete('a', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    'aaa aa1 aa2'.split(' '),
    'a'
  ]);
}));

// tab completion in editor mode
const editorStream = new common.ArrayStream();
const editor = repl.start({
  stream: editorStream,
  terminal: true,
  useColors: false
});

editorStream.run(['.clear']);
editorStream.run(['.editor']);

editor.completer('co', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['con'], 'co']);
}));

editorStream.run(['.clear']);
editorStream.run(['.editor']);

editor.completer('var log = console.l', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['console.log'], 'console.l']);
}));
