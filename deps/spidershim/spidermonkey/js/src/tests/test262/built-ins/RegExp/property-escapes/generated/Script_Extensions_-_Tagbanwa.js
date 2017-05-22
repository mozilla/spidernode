// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script_Extensions=Tagbanwa`
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
    [0x001735, 0x001736],
    [0x001760, 0x00176C],
    [0x00176E, 0x001770],
    [0x001772, 0x001773]
  ]
});
testPropertyEscapes(
  /^\p{Script_Extensions=Tagbanwa}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Tagbanwa}"
);
testPropertyEscapes(
  /^\p{Script_Extensions=Tagb}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Tagb}"
);
testPropertyEscapes(
  /^\p{scx=Tagbanwa}+$/u,
  matchSymbols,
  "\\p{scx=Tagbanwa}"
);
testPropertyEscapes(
  /^\p{scx=Tagb}+$/u,
  matchSymbols,
  "\\p{scx=Tagb}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [
    0x00176D,
    0x001771
  ],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x001734],
    [0x001737, 0x00175F],
    [0x001774, 0x00DBFF],
    [0x00E000, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script_Extensions=Tagbanwa}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Tagbanwa}"
);
testPropertyEscapes(
  /^\P{Script_Extensions=Tagb}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Tagb}"
);
testPropertyEscapes(
  /^\P{scx=Tagbanwa}+$/u,
  nonMatchSymbols,
  "\\P{scx=Tagbanwa}"
);
testPropertyEscapes(
  /^\P{scx=Tagb}+$/u,
  nonMatchSymbols,
  "\\P{scx=Tagb}"
);

reportCompare(0, 0);
