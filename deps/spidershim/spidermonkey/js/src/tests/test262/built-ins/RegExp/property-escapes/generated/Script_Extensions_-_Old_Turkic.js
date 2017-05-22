// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script_Extensions=Old_Turkic`
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
    [0x010C00, 0x010C48]
  ]
});
testPropertyEscapes(
  /^\p{Script_Extensions=Old_Turkic}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Old_Turkic}"
);
testPropertyEscapes(
  /^\p{Script_Extensions=Orkh}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Orkh}"
);
testPropertyEscapes(
  /^\p{scx=Old_Turkic}+$/u,
  matchSymbols,
  "\\p{scx=Old_Turkic}"
);
testPropertyEscapes(
  /^\p{scx=Orkh}+$/u,
  matchSymbols,
  "\\p{scx=Orkh}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x00DBFF],
    [0x00E000, 0x010BFF],
    [0x010C49, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script_Extensions=Old_Turkic}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Old_Turkic}"
);
testPropertyEscapes(
  /^\P{Script_Extensions=Orkh}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Orkh}"
);
testPropertyEscapes(
  /^\P{scx=Old_Turkic}+$/u,
  nonMatchSymbols,
  "\\P{scx=Old_Turkic}"
);
testPropertyEscapes(
  /^\P{scx=Orkh}+$/u,
  nonMatchSymbols,
  "\\P{scx=Orkh}"
);

reportCompare(0, 0);
