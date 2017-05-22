// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: >
    If B = 1110xxxx (n = 3) and C != 10xxxxxx (C - first of octets after B),
    throw URIError
es5id: 15.1.3.1_A1.14_T2
description: Complex tests. B = [0xE0 - 0xEF], C = [0x00, 0x7F]
includes: [decimalToHexString.js]
---*/

var errorCount = 0;
var count = 0;
var indexP;
var indexO = 0;

for (var indexB = 0xE0; indexB <= 0xEF; indexB++) {
  count++; 
  var hexB = decimalToPercentHexString(indexB);
  var result = true;
  for (var indexC = 0x00; indexC <= 0x7F; indexC++) {
    var hexC = decimalToPercentHexString(indexC);
    try {
      decodeURI(hexB + "%A0" + hexC);
    } catch (e) { 
      if ((e instanceof URIError) === true) continue;                
    }
    result = false;
  }
  if (result !== true) {
    if (indexO === 0) { 
      indexO = indexB;
    } else {
      if ((indexB - indexP) !== 1) {             
        if ((indexP - indexO) !== 0) {
          var hexP = decimalToHexString(indexP);
          var hexO = decimalToHexString(indexO);
          $ERROR('#' + hexO + '-' + hexP + ' ');
        } 
        else {
          var hexP = decimalToHexString(indexP);
          $ERROR('#' + hexP + ' ');
        }  
        indexO = indexB;
      }         
    }
    indexP = indexB;
    errorCount++;       
  }
}

if (errorCount > 0) {
  if ((indexP - indexO) !== 0) {
    var hexP = decimalToHexString(indexP);
    var hexO = decimalToHexString(indexO);
    $ERROR('#' + hexO + '-' + hexP + ' ');
  } else {
    var hexP = decimalToHexString(indexP);
    $ERROR('#' + hexP + ' ');
  }     
  $ERROR('Total error: ' + errorCount + ' bad Unicode character in ' + count + ' ');
}

reportCompare(0, 0);
