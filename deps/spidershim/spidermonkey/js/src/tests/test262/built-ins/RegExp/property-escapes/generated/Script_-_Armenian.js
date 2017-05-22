// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script=Armenian`
info: |
  Generated by https://github.com/mathiasbynens/unicode-property-escapes-tests
  Unicode v9.0.0
  Emoji v5.0 (UTR51)
esid: sec-static-semantics-unicodematchproperty-p
features: [regexp-unicode-property-escapes]
includes: [regExpUtils.js]
---*/

const matchSymbols = buildString({
  loneCodePoints: [
    0x00058A
  ],
  ranges: [
    [0x000531, 0x000556],
    [0x000559, 0x00055F],
    [0x000561, 0x000587],
    [0x00058D, 0x00058F],
    [0x00FB13, 0x00FB17]
  ]
});
testPropertyEscapes(
  /^\p{Script=Armenian}+$/u,
  matchSymbols,
  "\\p{Script=Armenian}"
);
testPropertyEscapes(
  /^\p{Script=Armn}+$/u,
  matchSymbols,
  "\\p{Script=Armn}"
);
testPropertyEscapes(
  /^\p{sc=Armenian}+$/u,
  matchSymbols,
  "\\p{sc=Armenian}"
);
testPropertyEscapes(
  /^\p{sc=Armn}+$/u,
  matchSymbols,
  "\\p{sc=Armn}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [
    0x000560
  ],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x000530],
    [0x000557, 0x000558],
    [0x000588, 0x000589],
    [0x00058B, 0x00058C],
    [0x000590, 0x00DBFF],
    [0x00E000, 0x00FB12],
    [0x00FB18, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script=Armenian}+$/u,
  nonMatchSymbols,
  "\\P{Script=Armenian}"
);
testPropertyEscapes(
  /^\P{Script=Armn}+$/u,
  nonMatchSymbols,
  "\\P{Script=Armn}"
);
testPropertyEscapes(
  /^\P{sc=Armenian}+$/u,
  nonMatchSymbols,
  "\\P{sc=Armenian}"
);
testPropertyEscapes(
  /^\P{sc=Armn}+$/u,
  nonMatchSymbols,
  "\\P{sc=Armn}"
);

reportCompare(0, 0);
