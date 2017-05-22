// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script=New_Tai_Lue`
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
    [0x001980, 0x0019AB],
    [0x0019B0, 0x0019C9],
    [0x0019D0, 0x0019DA],
    [0x0019DE, 0x0019DF]
  ]
});
testPropertyEscapes(
  /^\p{Script=New_Tai_Lue}+$/u,
  matchSymbols,
  "\\p{Script=New_Tai_Lue}"
);
testPropertyEscapes(
  /^\p{Script=Talu}+$/u,
  matchSymbols,
  "\\p{Script=Talu}"
);
testPropertyEscapes(
  /^\p{sc=New_Tai_Lue}+$/u,
  matchSymbols,
  "\\p{sc=New_Tai_Lue}"
);
testPropertyEscapes(
  /^\p{sc=Talu}+$/u,
  matchSymbols,
  "\\p{sc=Talu}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x00197F],
    [0x0019AC, 0x0019AF],
    [0x0019CA, 0x0019CF],
    [0x0019DB, 0x0019DD],
    [0x0019E0, 0x00DBFF],
    [0x00E000, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script=New_Tai_Lue}+$/u,
  nonMatchSymbols,
  "\\P{Script=New_Tai_Lue}"
);
testPropertyEscapes(
  /^\P{Script=Talu}+$/u,
  nonMatchSymbols,
  "\\P{Script=Talu}"
);
testPropertyEscapes(
  /^\P{sc=New_Tai_Lue}+$/u,
  nonMatchSymbols,
  "\\P{sc=New_Tai_Lue}"
);
testPropertyEscapes(
  /^\P{sc=Talu}+$/u,
  nonMatchSymbols,
  "\\P{sc=Talu}"
);

reportCompare(0, 0);
