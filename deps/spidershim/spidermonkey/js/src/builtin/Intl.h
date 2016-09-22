/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Intl_h
#define builtin_Intl_h

#include "NamespaceImports.h"
#if ENABLE_INTL_API
#include "unicode/utypes.h"
#endif

/*
 * The Intl module specified by standard ECMA-402,
 * ECMAScript Internationalization API Specification.
 */

namespace js {

/**
 * Initializes the Intl Object and its standard built-in properties.
 * Spec: ECMAScript Internationalization API Specification, 8.0, 8.1
 */
extern JSObject*
InitIntlClass(JSContext* cx, HandleObject obj);

/*
 * The following functions are for use by self-hosted code.
 */


/******************** Collator ********************/

/**
 * Returns a new instance of the standard built-in Collator constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: collator = intl_Collator(locales, options)
 */
extern MOZ_MUST_USE bool
intl_Collator(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for collation
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_Collator_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_Collator_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an array with the collation type identifiers per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * collations supported for the given locale. "standard" and "search" are
 * excluded.
 *
 * Usage: collations = intl_availableCollations(locale)
 */
extern MOZ_MUST_USE bool
intl_availableCollations(JSContext* cx, unsigned argc, Value* vp);

/**
 * Compares x and y (which must be String values), and returns a number less
 * than 0 if x < y, 0 if x = y, or a number greater than 0 if x > y according
 * to the sort order for the locale and collation options of the given
 * Collator.
 *
 * Spec: ECMAScript Internationalization API Specification, 10.3.2.
 *
 * Usage: result = intl_CompareStrings(collator, x, y)
 */
extern MOZ_MUST_USE bool
intl_CompareStrings(JSContext* cx, unsigned argc, Value* vp);


/******************** NumberFormat ********************/

/**
 * Returns a new instance of the standard built-in NumberFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: numberFormat = intl_NumberFormat(locales, options)
 */
extern MOZ_MUST_USE bool
intl_NumberFormat(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for number formatting
 * by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_NumberFormat_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_NumberFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns the numbering system type identifier per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * default numbering system for the given locale.
 *
 * Usage: defaultNumberingSystem = intl_numberingSystem(locale)
 */
extern MOZ_MUST_USE bool
intl_numberingSystem(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a string representing the number x according to the effective
 * locale and the formatting options of the given NumberFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 11.3.2.
 *
 * Usage: formatted = intl_FormatNumber(numberFormat, x)
 */
extern MOZ_MUST_USE bool
intl_FormatNumber(JSContext* cx, unsigned argc, Value* vp);


/******************** DateTimeFormat ********************/

/**
 * Returns a new instance of the standard built-in DateTimeFormat constructor.
 * Self-hosted code cannot cache this constructor (as it does for others in
 * Utilities.js) because it is initialized after self-hosted code is compiled.
 *
 * Usage: dateTimeFormat = intl_DateTimeFormat(locales, options)
 */
extern MOZ_MUST_USE bool
intl_DateTimeFormat(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an object indicating the supported locales for date and time
 * formatting by having a true-valued property for each such locale with the
 * canonicalized language tag as the property name. The object has no
 * prototype.
 *
 * Usage: availableLocales = intl_DateTimeFormat_availableLocales()
 */
extern MOZ_MUST_USE bool
intl_DateTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns an array with the calendar type identifiers per Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * supported calendars for the given locale. The default calendar is
 * element 0.
 *
 * Usage: calendars = intl_availableCalendars(locale)
 */
extern MOZ_MUST_USE bool
intl_availableCalendars(JSContext* cx, unsigned argc, Value* vp);

/**
 * Return a pattern in the date-time format pattern language of Unicode
 * Technical Standard 35, Unicode Locale Data Markup Language, for the
 * best-fit date-time format pattern corresponding to skeleton for the
 * given locale.
 *
 * Usage: pattern = intl_patternForSkeleton(locale, skeleton)
 */
extern MOZ_MUST_USE bool
intl_patternForSkeleton(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a String value representing x (which must be a Number value)
 * according to the effective locale and the formatting options of the
 * given DateTimeFormat.
 *
 * Spec: ECMAScript Internationalization API Specification, 12.3.2.
 *
 * Usage: formatted = intl_FormatDateTime(dateTimeFormat, x)
 */
extern MOZ_MUST_USE bool
intl_FormatDateTime(JSContext* cx, unsigned argc, Value* vp);

/**
 * Returns a plain object with calendar information for a single valid locale
 * (callers must perform this validation).  The object will have these
 * properties:
 *
 *   firstDayOfWeek
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the first day of the week in calendars, e.g. 1 for en-US,
 *     2 for en-GB, 1 for bn-IN
 *   minDays
 *     an integer in the range of 1 to 7 indicating the minimum number
 *     of days required in the first week of the year, e.g. 1 for en-US, 4 for de
 *   weekendStart
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the beginning of a weekend, e.g. 7 for en-US, 7 for en-GB,
 *     1 for bn-IN
 *   weekendEnd
 *     an integer in the range 1=Sunday to 7=Saturday indicating the day
 *     considered the end of a weekend, e.g. 1 for en-US, 1 for en-GB,
 *     1 for bn-IN (note that "weekend" is *not* necessarily two days)
 *
 * NOTE: "calendar" and "locale" properties are *not* added to the object.
 */
extern MOZ_MUST_USE bool
intl_GetCalendarInfo(JSContext* cx, unsigned argc, Value* vp);

#if ENABLE_INTL_API
/**
 * Cast char16_t* strings to UChar* strings used by ICU.
 */
inline const UChar*
Char16ToUChar(const char16_t* chars)
{
  return reinterpret_cast<const UChar*>(chars);
}

inline UChar*
Char16ToUChar(char16_t* chars)
{
  return reinterpret_cast<UChar*>(chars);
}
#endif // ENABLE_INTL_API

} // namespace js

#endif /* builtin_Intl_h */
