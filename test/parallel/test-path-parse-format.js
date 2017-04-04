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
require('../common');
const assert = require('assert');
const path = require('path');

const winPaths = [
  'C:\\path\\dir\\index.html',
  'C:\\another_path\\DIR\\1\\2\\33\\\\index',
  'another_path\\DIR with spaces\\1\\2\\33\\index',
  '\\foo\\C:',
  'file',
  '.\\file',
  'C:\\',
  '',

  // unc
  '\\\\server\\share\\file_path',
  '\\\\server two\\shared folder\\file path.zip',
  '\\\\teela\\admin$\\system32',
  '\\\\?\\UNC\\server\\share'
];

const winSpecialCaseParseTests = [
  ['/foo/bar', {root: '/'}]
];

const winSpecialCaseFormatTests = [
  [{dir: 'some\\dir'}, 'some\\dir\\'],
  [{base: 'index.html'}, 'index.html'],
  [{root: 'C:\\'}, 'C:\\'],
  [{name: 'index', ext: '.html'}, 'index.html'],
  [{dir: 'some\\dir', name: 'index', ext: '.html'}, 'some\\dir\\index.html'],
  [{root: 'C:\\', name: 'index', ext: '.html'}, 'C:\\index.html'],
  [{}, '']
];

const unixPaths = [
  '/home/user/dir/file.txt',
  '/home/user/a dir/another File.zip',
  '/home/user/a dir//another&File.',
  '/home/user/a$$$dir//another File.zip',
  'user/dir/another File.zip',
  'file',
  '.\\file',
  './file',
  'C:\\foo',
  '/',
  '',
  '.',
  '..',
  '/foo',
  '/foo.',
  '/foo.bar',
  '/.',
  '/.foo',
  '/.foo.bar',
  '/foo/bar.baz',
];

const unixSpecialCaseFormatTests = [
  [{dir: 'some/dir'}, 'some/dir/'],
  [{base: 'index.html'}, 'index.html'],
  [{root: '/'}, '/'],
  [{name: 'index', ext: '.html'}, 'index.html'],
  [{dir: 'some/dir', name: 'index', ext: '.html'}, 'some/dir/index.html'],
  [{root: '/', name: 'index', ext: '.html'}, '/index.html'],
  [{}, '']
];

const errors = [
  {method: 'parse', input: [null],
   message: /^TypeError: Path must be a string\. Received null$/},
  {method: 'parse', input: [{}],
   message: /^TypeError: Path must be a string\. Received {}$/},
  {method: 'parse', input: [true],
   message: /^TypeError: Path must be a string\. Received true$/},
  {method: 'parse', input: [1],
   message: /^TypeError: Path must be a string\. Received 1$/},
  {method: 'parse', input: [],
   message: /^TypeError: Path must be a string\. Received undefined$/},
  {method: 'format', input: [null],
   message:
      /^TypeError: Parameter "pathObject" must be an object, not object$/},
  {method: 'format', input: [''],
   message:
      /^TypeError: Parameter "pathObject" must be an object, not string$/},
  {method: 'format', input: [true],
   message:
      /^TypeError: Parameter "pathObject" must be an object, not boolean$/},
  {method: 'format', input: [1],
   message:
      /^TypeError: Parameter "pathObject" must be an object, not number$/},
];

checkParseFormat(path.win32, winPaths);
checkParseFormat(path.posix, unixPaths);
checkSpecialCaseParseFormat(path.win32, winSpecialCaseParseTests);
checkErrors(path.win32);
checkErrors(path.posix);
checkFormat(path.win32, winSpecialCaseFormatTests);
checkFormat(path.posix, unixSpecialCaseFormatTests);

// Test removal of trailing path separators
const trailingTests = [
  [ path.win32.parse,
    [['.\\', { root: '', dir: '', base: '.', ext: '', name: '.' }],
     ['\\\\', { root: '\\', dir: '\\', base: '', ext: '', name: '' }],
     ['\\\\', { root: '\\', dir: '\\', base: '', ext: '', name: '' }],
     ['c:\\foo\\\\\\',
      { root: 'c:\\', dir: 'c:\\', base: 'foo', ext: '', name: 'foo' }],
     ['D:\\foo\\\\\\bar.baz',
      { root: 'D:\\',
        dir: 'D:\\foo\\\\',
        base: 'bar.baz',
        ext: '.baz',
        name: 'bar'
      }
     ]
    ]
  ],
  [ path.posix.parse,
    [['./', { root: '', dir: '', base: '.', ext: '', name: '.' }],
     ['//', { root: '/', dir: '/', base: '', ext: '', name: '' }],
     ['///', { root: '/', dir: '/', base: '', ext: '', name: '' }],
     ['/foo///', { root: '/', dir: '/', base: 'foo', ext: '', name: 'foo' }],
     ['/foo///bar.baz',
      { root: '/', dir: '/foo//', base: 'bar.baz', ext: '.baz', name: 'bar' }
     ]
    ]
  ]
];
const failures = [];
trailingTests.forEach(function(test) {
  const parse = test[0];
  const os = parse === path.win32.parse ? 'win32' : 'posix';
  test[1].forEach(function(test) {
    const actual = parse(test[0]);
    const expected = test[1];
    const fn = `path.${os}.parse(`;
    const message = fn +
                    JSON.stringify(test[0]) +
                    ')' +
                    '\n  expect=' + JSON.stringify(expected) +
                    '\n  actual=' + JSON.stringify(actual);
    const actualKeys = Object.keys(actual);
    const expectedKeys = Object.keys(expected);
    let failed = (actualKeys.length !== expectedKeys.length);
    if (!failed) {
      for (let i = 0; i < actualKeys.length; ++i) {
        const key = actualKeys[i];
        if (expectedKeys.indexOf(key) === -1 || actual[key] !== expected[key]) {
          failed = true;
          break;
        }
      }
    }
    if (failed)
      failures.push('\n' + message);
  });
});
assert.strictEqual(failures.length, 0, failures.join(''));

function checkErrors(path) {
  errors.forEach(function(errorCase) {
    assert.throws(() => {
      path[errorCase.method].apply(path, errorCase.input);
    }, errorCase.message);
  });
}

function checkParseFormat(path, paths) {
  paths.forEach(function(element) {
    const output = path.parse(element);
    assert.strictEqual(typeof output.root, 'string');
    assert.strictEqual(typeof output.dir, 'string');
    assert.strictEqual(typeof output.base, 'string');
    assert.strictEqual(typeof output.ext, 'string');
    assert.strictEqual(typeof output.name, 'string');
    assert.strictEqual(path.format(output), element);
    assert.strictEqual(output.dir, output.dir ? path.dirname(element) : '');
    assert.strictEqual(output.base, path.basename(element));
    assert.strictEqual(output.ext, path.extname(element));
  });
}

function checkSpecialCaseParseFormat(path, testCases) {
  testCases.forEach(function(testCase) {
    const element = testCase[0];
    const expect = testCase[1];
    const output = path.parse(element);
    Object.keys(expect).forEach(function(key) {
      assert.strictEqual(output[key], expect[key]);
    });
  });
}

function checkFormat(path, testCases) {
  testCases.forEach(function(testCase) {
    assert.strictEqual(path.format(testCase[0]), testCase[1]);
  });
}
