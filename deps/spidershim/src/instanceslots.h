// Copyright Mozilla Foundation. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#pragma once

#include "jsfriendapi.h"

// Object's internal fields are stored in indices InstanceSlots::NumSlots + i.
enum class InstanceSlots {
  InstanceClassSlot,              // Stores a refcounted pointer to our InstanceClass*.
  ConstructorSlot,                // Stores the JSObject for the constructor of our ObjectTemplate
  CallCallbackSlot,               // Stores our call as function callback function.
  NamedGetterCallbackSlot1,       // Stores our named prop getter callback.
  NamedGetterCallbackSlot2,
  NamedSetterCallbackSlot1,       // Stores our named prop setter callback.
  NamedSetterCallbackSlot2,
  NamedQueryCallbackSlot1,        // Stores our named prop query callback.
  NamedQueryCallbackSlot2,
  NamedDeleterCallbackSlot1,      // Stores our named prop deleter callback.
  NamedDeleterCallbackSlot2,
  NamedEnumeratorCallbackSlot1,   // Stores our named prop enumerator callback.
  NamedEnumeratorCallbackSlot2,
  NamedCallbackDataSlot,          // Stores our named prop callback data
  IndexedGetterCallbackSlot1,     // Stores our indexed prop getter callback.
  IndexedGetterCallbackSlot2,
  IndexedSetterCallbackSlot1,     // Stores our indexed prop setter callback.
  IndexedSetterCallbackSlot2,
  IndexedQueryCallbackSlot1,      // Stores our indexed prop query callback.
  IndexedQueryCallbackSlot2,
  IndexedDeleterCallbackSlot1,    // Stores our indexed prop deleter callback.
  IndexedDeleterCallbackSlot2,
  IndexedEnumeratorCallbackSlot1, // Stores our indexed prop enumerator callback.
  IndexedEnumeratorCallbackSlot2,
  IndexedCallbackDataSlot,        // Stores our indexed prop callback data
  GenericGetterCallbackSlot1,     // Stores our generic prop getter callback.
  GenericGetterCallbackSlot2,
  GenericSetterCallbackSlot1,     // Stores our generic prop setter callback.
  GenericSetterCallbackSlot2,
  GenericQueryCallbackSlot1,      // Stores our generic prop query callback.
  GenericQueryCallbackSlot2,
  GenericDeleterCallbackSlot1,    // Stores our generic prop deleter callback.
  GenericDeleterCallbackSlot2,
  GenericEnumeratorCallbackSlot1, // Stores our generic prop enumerator callback.
  GenericEnumeratorCallbackSlot2,
  GenericCallbackDataSlot,        // Stores our generic prop callback data
  ContextSlot,                    // Stores our v8::Context pointer.  Only used on global objects.
  NumSlots
};

inline const JS::Value& GetInstanceSlot(JSObject* obj, size_t slot) {
  if (JS_IsGlobalObject(obj)) {
    // There are 5 preallocated application slots in global objects.
    slot = (slot < JSCLASS_GLOBAL_APPLICATION_SLOTS) ?
           slot : JSCLASS_GLOBAL_SLOT_COUNT + slot - JSCLASS_GLOBAL_APPLICATION_SLOTS;
  }
  return js::GetReservedSlot(obj, slot);
}

inline void SetInstanceSlot(JSObject* obj, size_t slot,
                            const JS::Value& value) {
  if (JS_IsGlobalObject(obj)) {
    // There are 5 preallocated application slots in global objects.
    slot = (slot < JSCLASS_GLOBAL_APPLICATION_SLOTS) ?
           slot : JSCLASS_GLOBAL_SLOT_COUNT + slot - JSCLASS_GLOBAL_APPLICATION_SLOTS;
  }
  js::SetReservedSlot(obj, slot, value);
}
