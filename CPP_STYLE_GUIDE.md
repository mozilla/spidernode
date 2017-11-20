# C++ Style Guide

## Table of Contents

* [Left-leaning (C++ style) asterisks for pointer declarations](#left-leaning-c-style-asterisks-for-pointer-declarations)
* [2 spaces of indentation for blocks or bodies of conditionals](#2-spaces-of-indentation-for-blocks-or-bodies-of-conditionals)
* [4 spaces of indentation for statement continuations](#4-spaces-of-indentation-for-statement-continuations)
* [Align function arguments vertically](#align-function-arguments-vertically)
* [Initialization lists](#initialization-lists)
* [CamelCase for methods, functions and classes](#camelcase-for-methods-functions-and-classes)
* [snake\_case for local variables and parameters](#snake_case-for-local-variables-and-parameters)
* [snake\_case\_ for private class fields](#snake_case_-for-private-class-fields)
* [Space after `template`](#space-after-template)
* [Type casting](#type-casting)
* [Memory allocation](#memory-allocation)
* [`nullptr` instead of `NULL` or `0`](#nullptr-instead-of-null-or-0)
* [Do not include `*.h` if `*-inl.h` has already been included](#do-not-include-h-if--inlh-has-already-been-included)
* [Avoid throwing JavaScript errors in nested C++ methods](#avoid-throwing-javascript-errors-in-nested-c-methods)
* [Ownership and Smart Pointers](#ownership-and-smart-pointers)

Unfortunately, the C++ linter (based on
[Google’s `cpplint`](https://github.com/google/styleguide)), which can be run
explicitly via `make lint-cpp`, does not currently catch a lot of rules that are
specific to the Node.js C++ code base. This document explains the most common of
these rules:

## Left-leaning (C++ style) asterisks for pointer declarations

`char* buffer;` instead of `char *buffer;`

## 2 spaces of indentation for blocks or bodies of conditionals

```c++
if (foo)
  bar();
```

or

```c++
if (foo) {
  bar();
  baz();
}
```

Braces are optional if the statement body only has one line.

`namespace`s receive no indentation on their own.

## 4 spaces of indentation for statement continuations

```c++
VeryLongTypeName very_long_result = SomeValueWithAVeryLongName +
    SomeOtherValueWithAVeryLongName;
```

Operators are before the line break in these cases.

## Align function arguments vertically

```c++
void FunctionWithAVeryLongName(int parameter_with_a_very_long_name,
                               double other_parameter_with_a_very_long_name,
                               ...);
```

If that doesn’t work, break after the `(` and use 4 spaces of indentation:

```c++
void FunctionWithAReallyReallyReallyLongNameSeriouslyStopIt(
    int okay_there_is_no_space_left_in_the_previous_line,
    ...);
```

## Initialization lists

Long initialization lists are formatted like this:

```c++
HandleWrap::HandleWrap(Environment* env,
                       Local<Object> object,
                       uv_handle_t* handle,
                       AsyncWrap::ProviderType provider)
    : AsyncWrap(env, object, provider),
      state_(kInitialized),
      handle_(handle) {
```

## CamelCase for methods, functions and classes

Exceptions are simple getters/setters, which are named `property_name()` and
`set_property_name()`, respectively.

```c++
class FooBar {
 public:
  void DoSomething();
  static void DoSomethingButItsStaticInstead();

  void set_foo_flag(int flag_value);
  int foo_flag() const;  // Use const-correctness whenever possible.
};
```

## snake\_case for local variables and parameters

```c++
int FunctionThatDoesSomething(const char* important_string) {
  const char* pointer_into_string = important_string;
}
```

## snake\_case\_ for private class fields

```c++
class Foo {
 private:
  int counter_ = 0;
};
```

## Space after `template`

```c++
template <typename T>
class FancyContainer {
 ...
}
```

## Type casting

- Always avoid C-style casts (`(type)value`)
- `dynamic_cast` does not work because RTTI is not enabled
- Use `static_cast` for casting whenever it works
- `reinterpret_cast` is okay if `static_cast` is not appropriate

## Memory allocation

- `Malloc()`, `Calloc()`, etc. from `util.h` abort in Out-of-Memory situations
- `UncheckedMalloc()`, etc. return `nullptr` in OOM situations

## `nullptr` instead of `NULL` or `0`

What it says in the title.

## Do not include `*.h` if `*-inl.h` has already been included

Do

```cpp
#include "util-inl.h"  // already includes util.h
```

instead of

```cpp
#include "util.h"
#include "util-inl.h"
```

## Avoid throwing JavaScript errors in nested C++ methods

If you need to throw JavaScript errors from a C++ binding method, try to do it
at the top level and not inside of nested calls.

A lot of code inside Node.js is written so that typechecking etc. is performed
in JavaScript.

Using C++ `throw` is not allowed.

## Ownership and Smart Pointers

"Smart" pointers are classes that act like pointers, e.g.
by overloading the `*` and `->` operators. Some smart pointer types can be
used to automate ownership bookkeeping, to ensure these responsibilities are
met. `std::unique_ptr` is a smart pointer type introduced in C++11, which
expresses exclusive ownership of a dynamically allocated object; the object
is deleted when the `std::unique_ptr` goes out of scope. It cannot be
copied, but can be moved to represent ownership transfer.
`std::shared_ptr` is a smart pointer type that expresses shared ownership of a
dynamically allocated object. `std::shared_ptr`s can be copied; ownership
of the object is shared among all copies, and the object
is deleted when the last `std::shared_ptr` is destroyed.

Prefer to use `std::unique_ptr` to make ownership
transfer explicit. For example:

```cpp
std::unique_ptr<Foo> FooFactory();
void FooConsumer(std::unique_ptr<Foo> ptr);
```

Never use `std::auto_ptr`. Instead, use `std::unique_ptr`.
