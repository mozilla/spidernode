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

#include <list>

#include "v8.h"
#include "autojsapi.h"
#include "js/GCPolicyAPI.h"

namespace v8 {
namespace internal {

template <class T>
class GCList : public std::list<T> {
 public:
  static void trace(GCList* list, JSTracer* trc) { list->trace(trc); }

  void trace(JSTracer* trc) {
    for (auto& item : *this) {
      JS::GCPolicy<T>::trace(trc, &item, "list element");
    }
  }
};

template <class Outer, class T>
class GCListOperations {
 protected:
  using List = GCList<T>;
  const List& list() const { return static_cast<const Outer*>(this)->get(); }

 public:
  size_t size() const { return list().size(); }
  typename List::const_reference back() const { return list().back(); }
};

template <class Outer, class T>
class MutableGCListOperations : public GCListOperations<Outer, T> {
  using List = typename GCListOperations<Outer, T>::List;
  List& list() { return static_cast<Outer*>(this)->get(); }

 public:
  typename List::reference back() { return list().back(); }
  void push_back(const T& elem) { return list().push_back(elem); }
  void erase(const typename List::iterator position) { list().erase(position); }
};
}
}

namespace js {

template <class T, typename Container>
class RootedBase<v8::internal::GCList<T>, Container>
    : public v8::internal::MutableGCListOperations<Container, T> {};
}
