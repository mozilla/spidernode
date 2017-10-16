// |reftest| skip error:SyntaxError -- BigInt is not supported
// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Octal BigInt literal containing an invalid digit
esid: prod-NumericLiteral
info: |
  NumericLiteral ::
    NumericLiteralBase NumericLiteralSuffix

  NumericLiteralBase ::
    DecimalLiteral
    BinaryIntegerLiteral
    OctalIntegerLiteral
    HexIntegerLiteral

  NumericLiteralSuffix :: n
negative:
  phase: early
  type: SyntaxError
features: [BigInt]
---*/

throw "Test262: This statement should not be evaluated.";

0o9n;
