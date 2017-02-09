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

#include <assert.h>

#include "v8.h"

namespace v8 {

struct PropertyDescriptor::Impl {
  Impl()
      : enumerable_(false),
        has_enumerable_(false),
        configurable_(false),
        has_configurable_(false),
        writable_(false),
        has_writable_(false) {}

  void SetEnumerable(bool enumerable) {
    enumerable_ = enumerable;
    has_enumerable_ = true;
  }

  bool HasEnumerable() {
    return has_enumerable_;
  }

  bool Enumerable() {
    return enumerable_;
  }

  void SetConfigurable(bool configurable) {
    configurable_ = configurable;
    has_configurable_ = true;
  }

  bool HasConfigurable() {
    return has_configurable_;
  }

  bool Configurable() {
    return configurable_;
  }

  void SetWritable(bool writable) {
    writable_ = writable;
    has_writable_ = true;
  }

  bool HasWritable() {
    return has_writable_;
  }

  bool Writable() {
    return writable_;
  }

  void SetValue(Local<v8::Value> value) {
    value_ = value;
  }

  bool HasValue() {
    return *value_ != nullptr;
  }

  Local<v8::Value> Value() {
    return value_;
  }

  void SetGetter(Local<v8::Value> getter) {
    getter_ = getter;
  }

  bool HasGetter() {
    return *getter_ != nullptr;
  }

  Local<v8::Value> Getter() {
    return getter_;
  }

  void SetSetter(Local<v8::Value> setter) {
    setter_ = setter;
  }

  bool HasSetter() {
    return *setter_ != nullptr;
  }

  Local<v8::Value> Setter() {
    return setter_;
  }

 private:
  bool enumerable_;
  bool has_enumerable_;
  bool configurable_;
  bool has_configurable_;
  bool writable_;
  bool has_writable_;
  Local<v8::Value> value_;
  Local<v8::Value> getter_;
  Local<v8::Value> setter_;

};

PropertyDescriptor::PropertyDescriptor()
    : pimpl_(new Impl()) {}

PropertyDescriptor::PropertyDescriptor(Local<Value> value)
    : pimpl_(new Impl()) {
  pimpl_->SetValue(value);
}

PropertyDescriptor::PropertyDescriptor(Local<Value> value, bool writable)
    : pimpl_(new Impl()) {
  pimpl_->SetValue(value);
  pimpl_->SetWritable(writable);
}

PropertyDescriptor::PropertyDescriptor(Local<Value> get, Local<Value> set)
    : pimpl_(new Impl()) {
  assert(get.IsEmpty() || get->IsUndefined() || get->IsFunction());
  assert(set.IsEmpty() || set->IsUndefined() || set->IsFunction());
  pimpl_->SetGetter(get);
  pimpl_->SetSetter(set);
}

PropertyDescriptor::~PropertyDescriptor() {
  delete pimpl_;
}

Local<Value> PropertyDescriptor::value() const {
  assert(pimpl_->HasValue());
  return pimpl_->Value();
}

Local<Value> PropertyDescriptor::get() const {
  assert(pimpl_->HasGetter());
  return pimpl_->Getter();
}

Local<Value> PropertyDescriptor::set() const {
  assert(pimpl_->HasSetter());
  return pimpl_->Setter();
}

bool PropertyDescriptor::has_value() const {
  return pimpl_->HasValue();
}
bool PropertyDescriptor::has_get() const {
  return pimpl_->HasGetter();
}
bool PropertyDescriptor::has_set() const {
  return pimpl_->HasSetter();
}

void PropertyDescriptor::set_enumerable(bool enumerable) {
  pimpl_->SetEnumerable(enumerable);
}

bool PropertyDescriptor::enumerable() const {
  assert(pimpl_->HasEnumerable());
  return pimpl_->Enumerable();
}

bool PropertyDescriptor::has_enumerable() const {
  return pimpl_->HasEnumerable();
}

void PropertyDescriptor::set_configurable(bool configurable) {
  pimpl_->SetConfigurable(configurable);
}

bool PropertyDescriptor::configurable() const {
  assert(pimpl_->HasConfigurable());
  return pimpl_->Configurable();
}

bool PropertyDescriptor::has_configurable() const {
  return pimpl_->HasConfigurable();
}

bool PropertyDescriptor::writable() const {
  assert(pimpl_->HasWritable());
  return pimpl_->Writable();
}

bool PropertyDescriptor::has_writable() const {
  return pimpl_->HasWritable();
}

} // namespace v8
