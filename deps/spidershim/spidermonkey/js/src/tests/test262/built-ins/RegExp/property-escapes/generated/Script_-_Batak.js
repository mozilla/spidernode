// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script=Batak`
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
    [0x001BC0, 0x001BF3],
    [0x001BFC, 0x001BFF]
  ]
});
testPropertyEscapes(
  /^\p{Script=Batak}+$/u,
  matchSymbols,
  "\\p{Script=Batak}"
);
testPropertyEscapes(
  /^\p{Script=Batk}+$/u,
  matchSymbols,
  "\\p{Script=Batk}"
);
testPropertyEscapes(
  /^\p{sc=Batak}+$/u,
  matchSymbols,
  "\\p{sc=Batak}"
);
testPropertyEscapes(
  /^\p{sc=Batk}+$/u,
  matchSymbols,
  "\\p{sc=Batk}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x001BBF],
    [0x001BF4, 0x001BFB],
    [0x001C00, 0x00DBFF],
    [0x00E000, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script=Batak}+$/u,
  nonMatchSymbols,
  "\\P{Script=Batak}"
);
testPropertyEscapes(
  /^\P{Script=Batk}+$/u,
  nonMatchSymbols,
  "\\P{Script=Batk}"
);
testPropertyEscapes(
  /^\P{sc=Batak}+$/u,
  nonMatchSymbols,
  "\\P{sc=Batak}"
);
testPropertyEscapes(
  /^\P{sc=Batk}+$/u,
  nonMatchSymbols,
  "\\P{sc=Batk}"
);

reportCompare(0, 0);
