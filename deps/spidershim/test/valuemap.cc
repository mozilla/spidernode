// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "v8engine.h"
#include "v8-util.h"

#include "gtest/gtest.h"

namespace {

void* IntKeyToVoidPointer(int key) { return reinterpret_cast<void*>(key << 1); }


Local<v8::Object> NewObjectForIntKey(
    v8::Isolate* isolate, const v8::Global<v8::ObjectTemplate>& templ,
    int key) {
  auto local = Local<v8::ObjectTemplate>::New(isolate, templ);
  auto obj = local->NewInstance(isolate->GetCurrentContext()).ToLocalChecked();
  obj->SetAlignedPointerInInternalField(0, IntKeyToVoidPointer(key));
  return obj;
}


template <typename K, typename V>
class PhantomStdMapTraits : public v8::StdMapTraits<K, V> {
 public:
  typedef typename v8::GlobalValueMap<K, V, PhantomStdMapTraits<K, V>> MapType;
  static const v8::PersistentContainerCallbackType kCallbackType =
      v8::kWeakWithInternalFields;
  struct WeakCallbackDataType {
    MapType* map;
    K key;
  };
  static WeakCallbackDataType* WeakCallbackParameter(MapType* map, const K& key,
                                                     Local<V> value) {
    WeakCallbackDataType* data = new WeakCallbackDataType;
    data->map = map;
    data->key = key;
    return data;
  }
  static MapType* MapFromWeakCallbackInfo(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
    return data.GetParameter()->map;
  }
  static K KeyFromWeakCallbackInfo(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& data) {
    return data.GetParameter()->key;
  }
  static void DisposeCallbackData(WeakCallbackDataType* data) { delete data; }
  static void Dispose(v8::Isolate* isolate, v8::Global<V> value, K key) {
    EXPECT_EQ(IntKeyToVoidPointer(key),
             v8::Object::GetAlignedPointerFromInternalField(value, 0));
  }
  static void OnWeakCallback(
      const v8::WeakCallbackInfo<WeakCallbackDataType>&) {}
  static void DisposeWeak(
      const v8::WeakCallbackInfo<WeakCallbackDataType>& info) {
    K key = KeyFromWeakCallbackInfo(info);
    EXPECT_EQ(IntKeyToVoidPointer(key), info.GetInternalField(0));
    DisposeCallbackData(info.GetParameter());
  }
};


template <typename Map>
void TestGlobalValueMap() {
  // Based on the V8 test-api.cc TestGlobalValueMap test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);

  v8::Global<ObjectTemplate> templ;
  {
    HandleScope scope(isolate);
    auto t = ObjectTemplate::New(isolate);
    t->SetInternalFieldCount(1);
    templ.Reset(isolate, t);
  }
  Map map(isolate);
  auto initial_handle_count = engine.GlobalHandleCount();
  EXPECT_EQ(0, static_cast<int>(map.Size()));
  {
    HandleScope scope(isolate);
    Local<v8::Object> obj = map.Get(7);
    EXPECT_TRUE(obj.IsEmpty());
    Local<v8::Object> expected = v8::Object::New(isolate);
    map.Set(7, expected);
    EXPECT_EQ(1, static_cast<int>(map.Size()));
    obj = map.Get(7);
    EXPECT_TRUE(expected->Equals(context, obj).FromJust());
    {
      typename Map::PersistentValueReference ref = map.GetReference(7);
      EXPECT_TRUE(expected->Equals(context, ref.NewLocal(isolate)).FromJust());
    }
    v8::Global<v8::Object> removed = map.Remove(7);
    EXPECT_EQ(0, static_cast<int>(map.Size()));
    EXPECT_TRUE(expected == removed);
    removed = map.Remove(7);
    EXPECT_TRUE(removed.IsEmpty());
    map.Set(8, expected);
    EXPECT_EQ(1, static_cast<int>(map.Size()));
    map.Set(8, expected);
    EXPECT_EQ(1, static_cast<int>(map.Size()));
    {
      typename Map::PersistentValueReference ref;
      Local<v8::Object> expected2 = NewObjectForIntKey(isolate, templ, 8);
      removed = map.Set(8, v8::Global<v8::Object>(isolate, expected2), &ref);
      EXPECT_EQ(1, static_cast<int>(map.Size()));
      EXPECT_TRUE(expected == removed);
      EXPECT_TRUE(expected2->Equals(context, ref.NewLocal(isolate)).FromJust());
    }
  }
  EXPECT_EQ(initial_handle_count + 1, engine.GlobalHandleCount());
  if (map.IsWeak()) {
    isolate->RequestGarbageCollectionForTesting(kFullGarbageCollection);
  } else {
    map.Clear();
  }
  EXPECT_EQ(0, static_cast<int>(map.Size()));
  EXPECT_EQ(initial_handle_count, engine.GlobalHandleCount());
  {
    HandleScope scope(isolate);
    Local<v8::Object> value = NewObjectForIntKey(isolate, templ, 9);
    map.Set(9, value);
    map.Clear();
  }
  EXPECT_EQ(0, static_cast<int>(map.Size()));
  EXPECT_EQ(initial_handle_count, engine.GlobalHandleCount());
}

}  // namespace


TEST(SpiderShim, GlobalValueMap) {
  // Default case, w/o weak callbacks:
  TestGlobalValueMap<v8::StdGlobalValueMap<int, v8::Object>>();

#if 0
  // Custom traits with weak callbacks:
  typedef v8::GlobalValueMap<int, v8::Object,
                             PhantomStdMapTraits<int, v8::Object>> WeakMap;
  TestGlobalValueMap<WeakMap>();
#endif
}


TEST(SpiderShim, PersistentValueVector) {
  // Based on the V8 test-api.cc ReplaceConstantFunction test.
  V8Engine engine;
  Isolate* isolate = engine.isolate();
  Isolate::Scope isolate_scope(isolate);

  HandleScope scope(isolate);

  Local<Context> context = Context::New(isolate);
  Context::Scope context_scope(context);
  auto handle_count = engine.GlobalHandleCount();

  v8::PersistentValueVector<v8::Object> vector(isolate);

  Local<v8::Object> obj1 = v8::Object::New(isolate);
  Local<v8::Object> obj2 = v8::Object::New(isolate);
  v8::Global<v8::Object> obj3(isolate, v8::Object::New(isolate));

  EXPECT_TRUE(vector.IsEmpty());
  EXPECT_EQ(0, static_cast<int>(vector.Size()));

  vector.ReserveCapacity(3);
  EXPECT_TRUE(vector.IsEmpty());

  vector.Append(obj1);
  vector.Append(obj2);
  vector.Append(obj1);
  vector.Append(obj3.Pass());
  vector.Append(obj1);

  EXPECT_TRUE(!vector.IsEmpty());
  EXPECT_EQ(5, static_cast<int>(vector.Size()));
  EXPECT_TRUE(obj3.IsEmpty());
  EXPECT_TRUE(obj1->Equals(context, vector.Get(0)).FromJust());
  EXPECT_TRUE(obj1->Equals(context, vector.Get(2)).FromJust());
  EXPECT_TRUE(obj1->Equals(context, vector.Get(4)).FromJust());
  EXPECT_TRUE(obj2->Equals(context, vector.Get(1)).FromJust());

  EXPECT_EQ(5 + handle_count, engine.GlobalHandleCount());

  vector.Clear();
  EXPECT_TRUE(vector.IsEmpty());
  EXPECT_EQ(0, static_cast<int>(vector.Size()));
  EXPECT_EQ(handle_count, engine.GlobalHandleCount());
}
