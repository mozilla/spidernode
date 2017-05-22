// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script_Extensions=Coptic`
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
    [0x0003E2, 0x0003EF],
    [0x002C80, 0x002CF3],
    [0x002CF9, 0x002CFF],
    [0x0102E0, 0x0102FB]
  ]
});
testPropertyEscapes(
  /^\p{Script_Extensions=Coptic}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Coptic}"
);
testPropertyEscapes(
  /^\p{Script_Extensions=Copt}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Copt}"
);
testPropertyEscapes(
  /^\p{Script_Extensions=Qaac}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Qaac}"
);
testPropertyEscapes(
  /^\p{scx=Coptic}+$/u,
  matchSymbols,
  "\\p{scx=Coptic}"
);
testPropertyEscapes(
  /^\p{scx=Copt}+$/u,
  matchSymbols,
  "\\p{scx=Copt}"
);
testPropertyEscapes(
  /^\p{scx=Qaac}+$/u,
  matchSymbols,
  "\\p{scx=Qaac}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x0003E1],
    [0x0003F0, 0x002C7F],
    [0x002CF4, 0x002CF8],
    [0x002D00, 0x00DBFF],
    [0x00E000, 0x0102DF],
    [0x0102FC, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script_Extensions=Coptic}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Coptic}"
);
testPropertyEscapes(
  /^\P{Script_Extensions=Copt}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Copt}"
);
testPropertyEscapes(
  /^\P{Script_Extensions=Qaac}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Qaac}"
);
testPropertyEscapes(
  /^\P{scx=Coptic}+$/u,
  nonMatchSymbols,
  "\\P{scx=Coptic}"
);
testPropertyEscapes(
  /^\P{scx=Copt}+$/u,
  nonMatchSymbols,
  "\\P{scx=Copt}"
);
testPropertyEscapes(
  /^\P{scx=Qaac}+$/u,
  nonMatchSymbols,
  "\\P{scx=Qaac}"
);

reportCompare(0, 0);
