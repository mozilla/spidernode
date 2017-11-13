'use strict';
const common = require('../common');
const PassThrough = require('stream').PassThrough;
const assert = require('assert');
const Interface = require('readline').Interface;

class FakeInput extends PassThrough {}

function extend(k) {
  return Object.assign({ ctrl: false, meta: false, shift: false }, k);
}


const fi = new FakeInput();
const fo = new FakeInput();
new Interface({ input: fi, output: fo, terminal: true });

let keys = [];
fi.on('keypress', (s, k) => {
  keys.push(k);
});


function addTest(sequences, expectedKeys) {
  if (!Array.isArray(sequences)) {
    sequences = [ sequences ];
  }

  if (!Array.isArray(expectedKeys)) {
    expectedKeys = [ expectedKeys ];
  }

  expectedKeys = expectedKeys.map(extend);

  keys = [];

  sequences.forEach((sequence) => {
    fi.write(sequence);
  });
  assert.deepStrictEqual(keys, expectedKeys);
}

// Simulate key interval test cases
// Returns a function that takes `next` test case and returns a thunk
// that can be called to run tests in sequence
// e.g.
// addKeyIntervalTest(..)
//   (addKeyIntervalTest(..)
//      (addKeyIntervalTest(..)(noop)))()
// where noop is a terminal function(() => {}).

const addKeyIntervalTest = (sequences, expectedKeys, interval = 550,
                            assertDelay = 550) => {
  const fn = common.mustCall((next) => () => {

    if (!Array.isArray(sequences)) {
      sequences = [ sequences ];
    }

    if (!Array.isArray(expectedKeys)) {
      expectedKeys = [ expectedKeys ];
    }

    expectedKeys = expectedKeys.map(extend);

    const keys = [];
    fi.on('keypress', (s, k) => keys.push(k));

    const emitKeys = ([head, ...tail]) => {
      if (head) {
        fi.write(head);
        setTimeout(() => emitKeys(tail), interval);
      } else {
        setTimeout(() => {
          next();
          assert.deepStrictEqual(keys, expectedKeys);
        }, assertDelay);
      }
    };
    emitKeys(sequences);
  });
  return fn;
};

// regular alphanumerics
addTest('io.JS', [
  { name: 'i', sequence: 'i' },
  { name: 'o', sequence: 'o' },
  { name: undefined, sequence: '.' },
  { name: 'j', sequence: 'J', shift: true },
  { name: 's', sequence: 'S', shift: true },
]);

// named characters
addTest('\n\r\t', [
  { name: 'enter', sequence: '\n' },
  { name: 'return', sequence: '\r' },
  { name: 'tab', sequence: '\t' },
]);

// space and backspace
addTest('\b\x7f\x1b\b\x1b\x7f \x1b ', [
  { name: 'backspace', sequence: '\b' },
  { name: 'backspace', sequence: '\x7f' },
  { name: 'backspace', sequence: '\x1b\b', meta: true },
  { name: 'backspace', sequence: '\x1b\x7f', meta: true },
  { name: 'space', sequence: ' ' },
  { name: 'space', sequence: '\x1b ', meta: true },
]);

// control keys
addTest('\x01\x0b\x10', [
  { name: 'a', sequence: '\x01', ctrl: true },
  { name: 'k', sequence: '\x0b', ctrl: true },
  { name: 'p', sequence: '\x10', ctrl: true },
]);

// alt keys
addTest('a\x1baA\x1bA', [
  { name: 'a', sequence: 'a' },
  { name: 'a', sequence: '\x1ba', meta: true },
  { name: 'a', sequence: 'A', shift: true },
  { name: 'a', sequence: '\x1bA', meta: true, shift: true },
]);

// xterm/gnome ESC O letter
addTest('\x1bOP\x1bOQ\x1bOR\x1bOS', [
  { name: 'f1', sequence: '\x1bOP', code: 'OP' },
  { name: 'f2', sequence: '\x1bOQ', code: 'OQ' },
  { name: 'f3', sequence: '\x1bOR', code: 'OR' },
  { name: 'f4', sequence: '\x1bOS', code: 'OS' },
]);

// xterm/rxvt ESC [ number ~ */
addTest('\x1b[11~\x1b[12~\x1b[13~\x1b[14~', [
  { name: 'f1', sequence: '\x1b[11~', code: '[11~' },
  { name: 'f2', sequence: '\x1b[12~', code: '[12~' },
  { name: 'f3', sequence: '\x1b[13~', code: '[13~' },
  { name: 'f4', sequence: '\x1b[14~', code: '[14~' },
]);

// from Cygwin and used in libuv
addTest('\x1b[[A\x1b[[B\x1b[[C\x1b[[D\x1b[[E', [
  { name: 'f1', sequence: '\x1b[[A', code: '[[A' },
  { name: 'f2', sequence: '\x1b[[B', code: '[[B' },
  { name: 'f3', sequence: '\x1b[[C', code: '[[C' },
  { name: 'f4', sequence: '\x1b[[D', code: '[[D' },
  { name: 'f5', sequence: '\x1b[[E', code: '[[E' },
]);

// common
addTest('\x1b[15~\x1b[17~\x1b[18~\x1b[19~\x1b[20~\x1b[21~\x1b[23~\x1b[24~', [
  { name: 'f5', sequence: '\x1b[15~', code: '[15~' },
  { name: 'f6', sequence: '\x1b[17~', code: '[17~' },
  { name: 'f7', sequence: '\x1b[18~', code: '[18~' },
  { name: 'f8', sequence: '\x1b[19~', code: '[19~' },
  { name: 'f9', sequence: '\x1b[20~', code: '[20~' },
  { name: 'f10', sequence: '\x1b[21~', code: '[21~' },
  { name: 'f11', sequence: '\x1b[23~', code: '[23~' },
  { name: 'f12', sequence: '\x1b[24~', code: '[24~' },
]);

// xterm ESC [ letter
addTest('\x1b[A\x1b[B\x1b[C\x1b[D\x1b[E\x1b[F\x1b[H', [
  { name: 'up', sequence: '\x1b[A', code: '[A' },
  { name: 'down', sequence: '\x1b[B', code: '[B' },
  { name: 'right', sequence: '\x1b[C', code: '[C' },
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'clear', sequence: '\x1b[E', code: '[E' },
  { name: 'end', sequence: '\x1b[F', code: '[F' },
  { name: 'home', sequence: '\x1b[H', code: '[H' },
]);

// xterm/gnome ESC O letter
addTest('\x1bOA\x1bOB\x1bOC\x1bOD\x1bOE\x1bOF\x1bOH', [
  { name: 'up', sequence: '\x1bOA', code: 'OA' },
  { name: 'down', sequence: '\x1bOB', code: 'OB' },
  { name: 'right', sequence: '\x1bOC', code: 'OC' },
  { name: 'left', sequence: '\x1bOD', code: 'OD' },
  { name: 'clear', sequence: '\x1bOE', code: 'OE' },
  { name: 'end', sequence: '\x1bOF', code: 'OF' },
  { name: 'home', sequence: '\x1bOH', code: 'OH' },
]);

// old xterm shift-arrows
addTest('\x1bO2A\x1bO2B', [
  { name: 'up', sequence: '\x1bO2A', code: 'OA', shift: true },
  { name: 'down', sequence: '\x1bO2B', code: 'OB', shift: true },
]);

// xterm/rxvt ESC [ number ~
addTest('\x1b[1~\x1b[2~\x1b[3~\x1b[4~\x1b[5~\x1b[6~', [
  { name: 'home', sequence: '\x1b[1~', code: '[1~' },
  { name: 'insert', sequence: '\x1b[2~', code: '[2~' },
  { name: 'delete', sequence: '\x1b[3~', code: '[3~' },
  { name: 'end', sequence: '\x1b[4~', code: '[4~' },
  { name: 'pageup', sequence: '\x1b[5~', code: '[5~' },
  { name: 'pagedown', sequence: '\x1b[6~', code: '[6~' },
]);

// putty
addTest('\x1b[[5~\x1b[[6~', [
  { name: 'pageup', sequence: '\x1b[[5~', code: '[[5~' },
  { name: 'pagedown', sequence: '\x1b[[6~', code: '[[6~' },
]);

// rxvt
addTest('\x1b[7~\x1b[8~', [
  { name: 'home', sequence: '\x1b[7~', code: '[7~' },
  { name: 'end', sequence: '\x1b[8~', code: '[8~' },
]);

// gnome terminal
addTest('\x1b[A\x1b[B\x1b[2A\x1b[2B', [
  { name: 'up', sequence: '\x1b[A', code: '[A' },
  { name: 'down', sequence: '\x1b[B', code: '[B' },
  { name: 'up', sequence: '\x1b[2A', code: '[A', shift: true },
  { name: 'down', sequence: '\x1b[2B', code: '[B', shift: true },
]);

// rxvt keys with modifiers
// eslint-disable-next-line max-len
addTest('\x1b[20~\x1b[2$\x1b[2^\x1b[3$\x1b[3^\x1b[5$\x1b[5^\x1b[6$\x1b[6^\x1b[7$\x1b[7^\x1b[8$\x1b[8^', [
  { name: 'f9', sequence: '\x1b[20~', code: '[20~' },
  { name: 'insert', sequence: '\x1b[2$', code: '[2$', shift: true },
  { name: 'insert', sequence: '\x1b[2^', code: '[2^', ctrl: true },
  { name: 'delete', sequence: '\x1b[3$', code: '[3$', shift: true },
  { name: 'delete', sequence: '\x1b[3^', code: '[3^', ctrl: true },
  { name: 'pageup', sequence: '\x1b[5$', code: '[5$', shift: true },
  { name: 'pageup', sequence: '\x1b[5^', code: '[5^', ctrl: true },
  { name: 'pagedown', sequence: '\x1b[6$', code: '[6$', shift: true },
  { name: 'pagedown', sequence: '\x1b[6^', code: '[6^', ctrl: true },
  { name: 'home', sequence: '\x1b[7$', code: '[7$', shift: true },
  { name: 'home', sequence: '\x1b[7^', code: '[7^', ctrl: true },
  { name: 'end', sequence: '\x1b[8$', code: '[8$', shift: true },
  { name: 'end', sequence: '\x1b[8^', code: '[8^', ctrl: true },
]);

// misc
addTest('\x1b[Z', [
  { name: 'tab', sequence: '\x1b[Z', code: '[Z', shift: true },
]);

// xterm + modifiers
addTest('\x1b[20;5~\x1b[6;5^', [
  { name: 'f9', sequence: '\x1b[20;5~', code: '[20~', ctrl: true },
  { name: 'pagedown', sequence: '\x1b[6;5^', code: '[6^', ctrl: true },
]);

addTest('\x1b[H\x1b[5H\x1b[1;5H', [
  { name: 'home', sequence: '\x1b[H', code: '[H' },
  { name: 'home', sequence: '\x1b[5H', code: '[H', ctrl: true },
  { name: 'home', sequence: '\x1b[1;5H', code: '[H', ctrl: true },
]);

// escape sequences broken into multiple data chunks
addTest('\x1b[D\x1b[C\x1b[D\x1b[C'.split(''), [
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'right', sequence: '\x1b[C', code: '[C' },
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'right', sequence: '\x1b[C', code: '[C' },
]);

// escape sequences mixed with regular ones
addTest('\x1b[DD\x1b[2DD\x1b[2^D', [
  { name: 'left', sequence: '\x1b[D', code: '[D' },
  { name: 'd', sequence: 'D', shift: true },
  { name: 'left', sequence: '\x1b[2D', code: '[D', shift: true },
  { name: 'd', sequence: 'D', shift: true },
  { name: 'insert', sequence: '\x1b[2^', code: '[2^', ctrl: true },
  { name: 'd', sequence: 'D', shift: true },
]);

// color sequences
addTest('\x1b[31ma\x1b[39ma', [
  { name: 'undefined', sequence: '\x1b[31m', code: '[31m' },
  { name: 'a', sequence: 'a' },
  { name: 'undefined', sequence: '\x1b[39m', code: '[39m' },
  { name: 'a', sequence: 'a' },
]);

// Reduce array of addKeyIntervalTest(..) right to left
// with () => {} as initial function
const runKeyIntervalTests = [
  // escape character
  addKeyIntervalTest('\x1b', [
    { name: 'escape', sequence: '\x1b', meta: true }
  ]),
  // chain of escape characters
  addKeyIntervalTest('\x1b\x1b\x1b\x1b'.split(''), [
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true },
    { name: 'escape', sequence: '\x1b', meta: true }
  ])
].reverse().reduce((acc, fn) => fn(acc), () => {});

// run key interval tests one after another
runKeyIntervalTests();
