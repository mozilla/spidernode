/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   File Name:          15.9.3.8.js
   ECMA Section:       15.9.3.8 The Date Constructor
   new Date( value )
   Description:        The [[Prototype]] property of the newly constructed
   object is set to the original Date prototype object,
   the one that is the initial valiue of Date.prototype.

   The [[Class]] property of the newly constructed object is
   set to "Date".

   The [[Value]] property of the newly constructed object is
   set as follows:

   1. Call ToPrimitive(value)
   2. If Type( Result(1) ) is String, then go to step 5.
   3. Let V be  ToNumber( Result(1) ).
   4. Set the [[Value]] property of the newly constructed
   object to TimeClip(V) and return.
   5. Parse Result(1) as a date, in exactly the same manner
   as for the parse method.  Let V be the time value for
   this date.
   6. Go to step 4.

   Author:             christine@netscape.com
   Date:               28 october 1997
   Version:            9706

*/

var SECTION = "15.9.3.8";
var TYPEOF  = "object";

var TIME        = 0;
var UTC_YEAR    = 1;
var UTC_MONTH   = 2;
var UTC_DATE    = 3;
var UTC_DAY     = 4;
var UTC_HOURS   = 5;
var UTC_MINUTES = 6;
var UTC_SECONDS = 7;
var UTC_MS      = 8;

var YEAR        = 9;
var MONTH       = 10;
var DATE        = 11;
var DAY         = 12;
var HOURS       = 13;
var MINUTES     = 14;
var SECONDS     = 15;
var MS          = 16;


var TITLE = "Date constructor:  new Date( value )";
var SECTION = "15.9.3.8";

writeHeaderToLog( SECTION +" " + TITLE );

// all the "ResultArrays" below are hard-coded to Pacific Standard Time values -
var TZ_ADJUST = -TZ_PST * msPerHour;


// Dates around 1970
addNewTestCase( new Date(0),
		"new Date(0)",
		[0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

addNewTestCase( new Date(1),
		"new Date(1)",
		[1,1970,0,1,4,0,0,0,1,1969,11,31,3,16,0,0,1] );

addNewTestCase( new Date(true),
		"new Date(true)",
		[1,1970,0,1,4,0,0,0,1,1969,11,31,3,16,0,0,1] );

addNewTestCase( new Date(false),
		"new Date(false)",
		[0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

addNewTestCase( new Date( (new Date(0)).toString() ),
		"new Date(\""+ (new Date(0)).toString()+"\" )",
		[0,1970,0,1,4,0,0,0,0,1969,11,31,3,16,0,0,0] );

test();

function addNewTestCase( DateCase, DateString, ResultArray ) {
  //adjust hard-coded ResultArray for tester's timezone instead of PST
  adjustResultArray(ResultArray, 'msMode');


  new TestCase( DateString+".getTime()", ResultArray[TIME],       DateCase.getTime() );
  new TestCase( DateString+".valueOf()", ResultArray[TIME],       DateCase.valueOf() );
  new TestCase( DateString+".getUTCFullYear()",      ResultArray[UTC_YEAR], DateCase.getUTCFullYear() );
  new TestCase( DateString+".getUTCMonth()",         ResultArray[UTC_MONTH],  DateCase.getUTCMonth() );
  new TestCase( DateString+".getUTCDate()",          ResultArray[UTC_DATE],   DateCase.getUTCDate() );
  new TestCase( DateString+".getUTCDay()",           ResultArray[UTC_DAY],    DateCase.getUTCDay() );
  new TestCase( DateString+".getUTCHours()",         ResultArray[UTC_HOURS],  DateCase.getUTCHours() );
  new TestCase( DateString+".getUTCMinutes()",       ResultArray[UTC_MINUTES],DateCase.getUTCMinutes() );
  new TestCase( DateString+".getUTCSeconds()",       ResultArray[UTC_SECONDS],DateCase.getUTCSeconds() );
  new TestCase( DateString+".getUTCMilliseconds()",  ResultArray[UTC_MS],     DateCase.getUTCMilliseconds() );
  new TestCase( DateString+".getFullYear()",         ResultArray[YEAR],       DateCase.getFullYear() );
  new TestCase( DateString+".getMonth()",            ResultArray[MONTH],      DateCase.getMonth() );
  new TestCase( DateString+".getDate()",             ResultArray[DATE],       DateCase.getDate() );
  new TestCase( DateString+".getDay()",              ResultArray[DAY],        DateCase.getDay() );
  new TestCase( DateString+".getHours()",            ResultArray[HOURS],      DateCase.getHours() );
  new TestCase( DateString+".getMinutes()",          ResultArray[MINUTES],    DateCase.getMinutes() );
  new TestCase( DateString+".getSeconds()",          ResultArray[SECONDS],    DateCase.getSeconds() );
  new TestCase( DateString+".getMilliseconds()",     ResultArray[MS],         DateCase.getMilliseconds() );
}
