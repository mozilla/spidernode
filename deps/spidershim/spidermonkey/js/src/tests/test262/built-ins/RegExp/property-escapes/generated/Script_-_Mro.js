// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script=Mro`
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
    [0x016A40, 0x016A5E],
    [0x016A60, 0x016A69],
    [0x016A6E, 0x016A6F]
  ]
});
testPropertyEscapes(
  /^\p{Script=Mro}+$/u,
  matchSymbols,
  "\\p{Script=Mro}"
);
testPropertyEscapes(
  /^\p{Script=Mroo}+$/u,
  matchSymbols,
  "\\p{Script=Mroo}"
);
testPropertyEscapes(
  /^\p{sc=Mro}+$/u,
  matchSymbols,
  "\\p{sc=Mro}"
);
testPropertyEscapes(
  /^\p{sc=Mroo}+$/u,
  matchSymbols,
  "\\p{sc=Mroo}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [
    0x016A5F
  ],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x00DBFF],
    [0x00E000, 0x016A3F],
    [0x016A6A, 0x016A6D],
    [0x016A70, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script=Mro}+$/u,
  nonMatchSymbols,
  "\\P{Script=Mro}"
);
testPropertyEscapes(
  /^\P{Script=Mroo}+$/u,
  nonMatchSymbols,
  "\\P{Script=Mroo}"
);
testPropertyEscapes(
  /^\P{sc=Mro}+$/u,
  nonMatchSymbols,
  "\\P{sc=Mro}"
);
testPropertyEscapes(
  /^\P{sc=Mroo}+$/u,
  nonMatchSymbols,
  "\\P{sc=Mroo}"
);

reportCompare(0, 0);
