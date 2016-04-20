Stack rooting
===
### Introduction
V8 and SpiderMonkey differ in how they keep track of the GC roots living on the stack.

V8 uses the `HandleScope` object for this purpose.  This object is meant to be allocated on the stack before calling into V8 code, and it internally keeps track of all of the GC roots living on the stack.  Several `HandleScope` objects can be nested within each other on the native stack, effectively forming a `HandleScope` stack.  Values that are being tracked in a `HandleScope` are denoted using the `Local` smart pointer class.  `Local` objects are light weight rooted objects that can be passed around efficiently.  Under the hoods, `Local<T>` holds a `T*` inside it, and nothing else.  The type system ensures that you can't construct a `Local` out of thin air.  These objects are meant to be obtained from other APIs (for example, by calling `Foo::New()`.)

In SpiderMonkey, each context or runtime holds a list of stack roots.  The `Rooted` smart pointer registers a value with the respective stack roots list, and the `Rooted` objects on the native stack form a linked list.  The GC traverses this linked list in order to compute the full list of stack roots.  `Handle` smart pointers can be efficiently obtained from a `Rooted` object and they can be passed around.  The `Handle` objects are quite similar to V8's `Local` objects.  The type system ensures that you can only obtain a `Handle` from `Rooted`.

Another thing to note about the SpiderMonkey side is that there are different kinds of roots (see the [RootKind enum](https://dxr.mozilla.org/mozilla-central/source/js/public/TraceKind.h#98) for a full list).  For example, `JS::Value`s live in a different list than `JSScript`s.

### SpiderShim solution
These two models are incompatible with each other.  In SpiderShim, we connect these two worlds in the `HandleScope` implementation.  Each `HandleScope` currently contains two linked list containing the roots registered with it, one for `JS::Value`s and one for `JSScript`s.  We may need to extend this to contain more root types in the future.  Each one of these lists is registered as a root by the virtue of it living inside a `Rooted`.  The linked list class, `GCList` is an `std::list` which provides a `trace()` method used by SpiderMonkey when tracing the GC roots.  The `HandleScope::AddToScope()` overloads are expected to deal with the different types of classes that can be used to instantiate `Local` with.  In some cases, such as `Context`, no GC root needs to be registered with SpiderMonkey, so the `AddToScope()` overload can just return its argument.  There are a couple of overloads that haven't been fully implemented yet.  If you try to create a `Local` with a class hat this setup does not know how to handle, you should get a compile time error complaining about a missing `AddToScope()` overload.

### How to use this system inside SpiderShim?
In code which is meant to create a `Local` and return it to the outside world, you can use a pattern like this:

```cpp
#include "v8local.h"

Local<Object> CreateNew() {
  JS::Value val;
  // Populate the value with something.  For example an object.
  val.setObject(someObj);
  // Object below should be the type of the Local to be created.
  return internal::Local<Object>::New(isolate, val);
}
```
