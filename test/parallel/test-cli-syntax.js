'use strict';

const common = require('../common');
const assert = require('assert');
const { exec, spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const node = process.execPath;

// test both sets of arguments that check syntax
const syntaxArgs = [
  ['-c'],
  ['--check']
];

const syntaxErrorRE = /^SyntaxError: Unexpected identifier$/m;
const notFoundRE = /^Error: Cannot find module/m;

// test good syntax with and without shebang
[
  'syntax/good_syntax.js',
  'syntax/good_syntax',
  'syntax/good_syntax_shebang.js',
  'syntax/good_syntax_shebang',
  'syntax/illegal_if_not_wrapped.js'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);

    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.ifError(err);
      assert.strictEqual(stdout, '');
      assert.strictEqual(stderr, '');
    }));
  });
});

// test bad syntax with and without shebang
[
  'syntax/bad_syntax.js',
  'syntax/bad_syntax',
  'syntax/bad_syntax_shebang.js',
  'syntax/bad_syntax_shebang'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);
    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 1);

      // no stdout should be produced
      assert.strictEqual(stdout, '');

      // stderr should have a syntax error message
      assert(syntaxErrorRE.test(stderr), `${syntaxErrorRE} === ${stderr}`);

      // stderr should include the filename
      assert(stderr.startsWith(file), `${stderr} starts with ${file}`);
    }));
  });
});

// test file not found
[
  'syntax/file_not_found.js',
  'syntax/file_not_found'
].forEach(function(file) {
  file = fixtures.path(file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    const _args = args.concat(file);
    const cmd = [node, ..._args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      // no stdout should be produced
      assert.strictEqual(stdout, '');

      // stderr should have a module not found error message
      assert(notFoundRE.test(stderr), `${notFoundRE} === ${stderr}`);

      assert.strictEqual(err.code, 1);
    }));
  });
});

// should not execute code piped from stdin with --check
// loop each possible option, `-c` or `--check`
syntaxArgs.forEach(function(args) {
  const stdin = 'throw new Error("should not get run");';
  const c = spawnSync(node, args, { encoding: 'utf8', input: stdin });

  // no stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');
  assert.strictEqual(c.stderr, '');

  assert.strictEqual(c.status, 0);
});

// should throw if code piped from stdin with --check has bad syntax
// loop each possible option, `-c` or `--check`
syntaxArgs.forEach(function(args) {
  const stdin = 'var foo bar;';
  const c = spawnSync(node, args, { encoding: 'utf8', input: stdin });

  // stderr should include '[stdin]' as the filename
  assert(c.stderr.startsWith('[stdin]'), `${c.stderr} starts with ${stdin}`);

  // no stdout or stderr should be produced
  assert.strictEqual(c.stdout, '');

  // stderr should have a syntax error message
  assert(syntaxErrorRE.test(c.stderr), `${syntaxErrorRE} === ${c.stderr}`);

  assert.strictEqual(c.status, 1);
});

// should throw if -c and -e flags are both passed
['-c', '--check'].forEach(function(checkFlag) {
  ['-e', '--eval'].forEach(function(evalFlag) {
    const args = [checkFlag, evalFlag, 'foo'];
    const cmd = [node, ...args].join(' ');
    exec(cmd, common.mustCall((err, stdout, stderr) => {
      assert.strictEqual(err instanceof Error, true);
      assert.strictEqual(err.code, 9);
      assert(
        stderr.startsWith(
          `${node}: either --check or --eval can be used, not both`
        )
      );
    }));
  });
});
