'use strict';
const common = require('../common');
const path = require('path');
const spawn = require('child_process').spawn;
const assert = require('assert');

if (common.isChakraEngine) {
  console.log('1..0 # Skipped: This test is disabled for chakra engine ' +
  'because debugger support is not implemented yet.');
  return;
}

const fixture = path.join(
  common.fixturesDir,
  'debugger-util-regression-fixture.js'
);

const args = [
  'debug',
  `--port=${common.PORT}`,
  fixture
];

const proc = spawn(process.execPath, args, { stdio: 'pipe' });
proc.stdout.setEncoding('utf8');
proc.stderr.setEncoding('utf8');

let stdout = '';
let stderr = '';

let nextCount = 0;
let exit = false;

proc.stdout.on('data', (data) => {
  stdout += data;
  if (stdout.includes('> 1') && nextCount < 1 ||
      stdout.includes('> 2') && nextCount < 2 ||
      stdout.includes('> 3') && nextCount < 3 ||
      stdout.includes('> 4') && nextCount < 4) {
    nextCount++;
    proc.stdin.write('n\n');
  } else if (!exit && (stdout.includes('< { a: \'b\' }'))) {
    exit = true;
    proc.stdin.write('.exit\n');
  } else if (stdout.includes('program terminated')) {
    // Catch edge case present in v4.x
    // process will terminate after call to util.inspect
    common.fail('the program should not terminate');
  }
});

proc.stderr.on('data', (data) => stderr += data);

process.on('exit', (code) => {
  assert.equal(code, 0, 'the program should exit cleanly');
  assert.equal(stdout.includes('{ a: \'b\' }'), true,
               'the debugger should print the result of util.inspect');
  assert.equal(stderr, '', 'stderr should be empty');
});
