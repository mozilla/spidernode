// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: unescapedURISet containing "#"
es5id: 15.1.3.3_A3.3_T1
es6id: 18.2.6.4
esid: sec-encodeuri-uri
description: encodeURI("#") === "#"
---*/

if (encodeURI("#") !== "#") {
  $ERROR('#1: unescapedURISet containing "#"');
}

reportCompare(0, 0);
