'use strict';
// Original test written by Jakub Lekstan <kuebzky@gmail.com>
const common = require('../common');

// FIXME add sunos support
if (common.isSunOS) {
  return common.skip(`Unsupported platform [${process.platform}]`);
}

const assert = require('assert');
const exec = require('child_process').exec;
const path = require('path');

// The title shouldn't be too long; libuv's uv_set_process_title() out of
// security considerations no longer overwrites envp, only argv, so the
// maximum title length is possibly quite short.
let title = 'test';

assert.notStrictEqual(process.title, title);
process.title = title;
assert.strictEqual(process.title, title);

// Test setting the title but do not try to run `ps` on Windows.
if (common.isWindows) {
  return common.skip('Windows does not have "ps" utility');
}

// To pass this test on alpine, since Busybox `ps` does not
// support `-p` switch, use `ps -o` and `grep` instead.
const cmd = common.isLinux ?
            `ps -o pid,args | grep '${process.pid} ${title}' | grep -v grep` :
            `ps -p ${process.pid} -o args=`;

exec(cmd, common.mustCall((error, stdout, stderr) => {
  assert.ifError(error);
  assert.strictEqual(stderr, '');

  // freebsd always add ' (procname)' to the process title
  if (common.isFreeBSD)
    title += ` (${path.basename(process.execPath)})`;

  // omitting trailing whitespace and \n
  assert.strictEqual(stdout.replace(/\s+$/, '').endsWith(title), true);
}));
