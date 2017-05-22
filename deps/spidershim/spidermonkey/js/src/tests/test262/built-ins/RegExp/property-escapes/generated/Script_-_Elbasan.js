// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script=Elbasan`
info: |
  Generated by https://github.com/mathiasbynens/unicode-property-escapes-tests
  Unicode v9.0.0
  Emoji v5.0 (UTR51)
esid: sec-static-semantics-unicodematchproperty-p
features: [regexp-unicode-property-escapes]
includes: [regExpUtils.js]
---*/

const matchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x010500, 0x010527]
  ]
});
testPropertyEscapes(
  /^\p{Script=Elbasan}+$/u,
  matchSymbols,
  "\\p{Script=Elbasan}"
);
testPropertyEscapes(
  /^\p{Script=Elba}+$/u,
  matchSymbols,
  "\\p{Script=Elba}"
);
testPropertyEscapes(
  /^\p{sc=Elbasan}+$/u,
  matchSymbols,
  "\\p{sc=Elbasan}"
);
testPropertyEscapes(
  /^\p{sc=Elba}+$/u,
  matchSymbols,
  "\\p{sc=Elba}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x00DBFF],
    [0x00E000, 0x0104FF],
    [0x010528, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script=Elbasan}+$/u,
  nonMatchSymbols,
  "\\P{Script=Elbasan}"
);
testPropertyEscapes(
  /^\P{Script=Elba}+$/u,
  nonMatchSymbols,
  "\\P{Script=Elba}"
);
testPropertyEscapes(
  /^\P{sc=Elbasan}+$/u,
  nonMatchSymbols,
  "\\P{sc=Elbasan}"
);
testPropertyEscapes(
  /^\P{sc=Elba}+$/u,
  nonMatchSymbols,
  "\\P{sc=Elba}"
);

reportCompare(0, 0);
