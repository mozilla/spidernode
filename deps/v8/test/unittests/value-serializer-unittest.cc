// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/value-serializer.h"

#include <algorithm>
#include <string>

#include "include/v8.h"
#include "src/api.h"
#include "src/base/build_config.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

class ValueSerializerTest : public TestWithIsolate {
 protected:
  ValueSerializerTest()
      : serialization_context_(Context::New(isolate())),
        deserialization_context_(Context::New(isolate())) {}

  const Local<Context>& serialization_context() {
    return serialization_context_;
  }
  const Local<Context>& deserialization_context() {
    return deserialization_context_;
  }

  template <typename InputFunctor, typename OutputFunctor>
  void RoundTripTest(const InputFunctor& input_functor,
                     const OutputFunctor& output_functor) {
    EncodeTest(input_functor,
               [this, &output_functor](const std::vector<uint8_t>& data) {
                 DecodeTest(data, output_functor);
               });
  }

  // Variant for the common case where a script is used to build the original
  // value.
  template <typename OutputFunctor>
  void RoundTripTest(const char* source, const OutputFunctor& output_functor) {
    RoundTripTest([this, source]() { return EvaluateScriptForInput(source); },
                  output_functor);
  }

  Maybe<std::vector<uint8_t>> DoEncode(Local<Value> value) {
    // This approximates what the API implementation would do.
    // TODO(jbroman): Use the public API once it exists.
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate());
    i::HandleScope handle_scope(internal_isolate);
    i::ValueSerializer serializer(internal_isolate);
    serializer.WriteHeader();
    if (serializer.WriteObject(Utils::OpenHandle(*value)).FromMaybe(false)) {
      return Just(serializer.ReleaseBuffer());
    }
    if (internal_isolate->has_pending_exception()) {
      internal_isolate->OptionalRescheduleException(true);
    }
    return Nothing<std::vector<uint8_t>>();
  }

  template <typename InputFunctor, typename EncodedDataFunctor>
  void EncodeTest(const InputFunctor& input_functor,
                  const EncodedDataFunctor& encoded_data_functor) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    Local<Value> input_value = input_functor();
    std::vector<uint8_t> buffer;
    ASSERT_TRUE(DoEncode(input_value).To(&buffer));
    ASSERT_FALSE(try_catch.HasCaught());
    encoded_data_functor(buffer);
  }

  template <typename MessageFunctor>
  void InvalidEncodeTest(const char* source, const MessageFunctor& functor) {
    Context::Scope scope(serialization_context());
    TryCatch try_catch(isolate());
    Local<Value> input_value = EvaluateScriptForInput(source);
    ASSERT_TRUE(DoEncode(input_value).IsNothing());
    functor(try_catch.Message());
  }

  void InvalidEncodeTest(const char* source) {
    InvalidEncodeTest(source, [](Local<Message>) {});
  }

  template <typename OutputFunctor>
  void DecodeTest(const std::vector<uint8_t>& data,
                  const OutputFunctor& output_functor) {
    Context::Scope scope(deserialization_context());
    TryCatch try_catch(isolate());
    // TODO(jbroman): Use the public API once it exists.
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate());
    i::HandleScope handle_scope(internal_isolate);
    i::ValueDeserializer deserializer(
        internal_isolate,
        i::Vector<const uint8_t>(&data[0], static_cast<int>(data.size())));
    ASSERT_TRUE(deserializer.ReadHeader().FromMaybe(false));
    Local<Value> result;
    ASSERT_TRUE(ToLocal<Value>(deserializer.ReadObject(), &result));
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_FALSE(try_catch.HasCaught());
    ASSERT_TRUE(deserialization_context()
                    ->Global()
                    ->CreateDataProperty(deserialization_context_,
                                         StringFromUtf8("result"), result)
                    .FromMaybe(false));
    output_functor(result);
    ASSERT_FALSE(try_catch.HasCaught());
  }

  template <typename OutputFunctor>
  void DecodeTestForVersion0(const std::vector<uint8_t>& data,
                             const OutputFunctor& output_functor) {
    Context::Scope scope(deserialization_context());
    TryCatch try_catch(isolate());
    // TODO(jbroman): Use the public API once it exists.
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate());
    i::HandleScope handle_scope(internal_isolate);
    i::ValueDeserializer deserializer(
        internal_isolate,
        i::Vector<const uint8_t>(&data[0], static_cast<int>(data.size())));
    // TODO(jbroman): Enable legacy support.
    ASSERT_TRUE(deserializer.ReadHeader().FromMaybe(false));
    // TODO(jbroman): Check version 0.
    Local<Value> result;
    ASSERT_TRUE(ToLocal<Value>(
        deserializer.ReadObjectUsingEntireBufferForLegacyFormat(), &result));
    ASSERT_FALSE(result.IsEmpty());
    ASSERT_FALSE(try_catch.HasCaught());
    ASSERT_TRUE(deserialization_context()
                    ->Global()
                    ->CreateDataProperty(deserialization_context_,
                                         StringFromUtf8("result"), result)
                    .FromMaybe(false));
    output_functor(result);
    ASSERT_FALSE(try_catch.HasCaught());
  }

  void InvalidDecodeTest(const std::vector<uint8_t>& data) {
    Context::Scope scope(deserialization_context());
    TryCatch try_catch(isolate());
    i::Isolate* internal_isolate = reinterpret_cast<i::Isolate*>(isolate());
    i::HandleScope handle_scope(internal_isolate);
    i::ValueDeserializer deserializer(
        internal_isolate,
        i::Vector<const uint8_t>(&data[0], static_cast<int>(data.size())));
    Maybe<bool> header_result = deserializer.ReadHeader();
    if (header_result.IsNothing()) return;
    ASSERT_TRUE(header_result.ToChecked());
    ASSERT_TRUE(deserializer.ReadObject().is_null());
  }

  Local<Value> EvaluateScriptForInput(const char* utf8_source) {
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(serialization_context_, source).ToLocalChecked();
    return script->Run(serialization_context_).ToLocalChecked();
  }

  bool EvaluateScriptForResultBool(const char* utf8_source) {
    Local<String> source = StringFromUtf8(utf8_source);
    Local<Script> script =
        Script::Compile(deserialization_context_, source).ToLocalChecked();
    Local<Value> value = script->Run(deserialization_context_).ToLocalChecked();
    return value->BooleanValue(deserialization_context_).FromJust();
  }

  Local<String> StringFromUtf8(const char* source) {
    return String::NewFromUtf8(isolate(), source, NewStringType::kNormal)
        .ToLocalChecked();
  }

  static std::string Utf8Value(Local<Value> value) {
    String::Utf8Value utf8(value);
    return std::string(*utf8, utf8.length());
  }

 private:
  Local<Context> serialization_context_;
  Local<Context> deserialization_context_;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializerTest);
};

TEST_F(ValueSerializerTest, DecodeInvalid) {
  // Version tag but no content.
  InvalidDecodeTest({0xff});
  // Version too large.
  InvalidDecodeTest({0xff, 0x7f, 0x5f});
  // Nonsense tag.
  InvalidDecodeTest({0xff, 0x09, 0xdd});
}

TEST_F(ValueSerializerTest, RoundTripOddball) {
  RoundTripTest([this]() { return Undefined(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  RoundTripTest([this]() { return True(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  RoundTripTest([this]() { return False(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  RoundTripTest([this]() { return Null(isolate()); },
                [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });
}

TEST_F(ValueSerializerTest, DecodeOddball) {
  // What this code is expected to generate.
  DecodeTest({0xff, 0x09, 0x5f},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0xff, 0x09, 0x54},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0xff, 0x09, 0x46},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0xff, 0x09, 0x30},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });

  // What v9 of the Blink code generates.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x5f, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x54, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x46, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x30, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });

  // v0 (with no explicit version).
  DecodeTest({0x5f, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsUndefined()); });
  DecodeTest({0x54, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsTrue()); });
  DecodeTest({0x46, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsFalse()); });
  DecodeTest({0x30, 0x00},
             [](Local<Value> value) { EXPECT_TRUE(value->IsNull()); });
}

TEST_F(ValueSerializerTest, RoundTripNumber) {
  RoundTripTest([this]() { return Integer::New(isolate(), 42); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsInt32());
                  EXPECT_EQ(42, Int32::Cast(*value)->Value());
                });
  RoundTripTest([this]() { return Integer::New(isolate(), -31337); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsInt32());
                  EXPECT_EQ(-31337, Int32::Cast(*value)->Value());
                });
  RoundTripTest(
      [this]() {
        return Integer::New(isolate(), std::numeric_limits<int32_t>::min());
      },
      [](Local<Value> value) {
        ASSERT_TRUE(value->IsInt32());
        EXPECT_EQ(std::numeric_limits<int32_t>::min(),
                  Int32::Cast(*value)->Value());
      });
  RoundTripTest([this]() { return Number::New(isolate(), -0.25); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsNumber());
                  EXPECT_EQ(-0.25, Number::Cast(*value)->Value());
                });
  RoundTripTest(
      [this]() {
        return Number::New(isolate(), std::numeric_limits<double>::quiet_NaN());
      },
      [](Local<Value> value) {
        ASSERT_TRUE(value->IsNumber());
        EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
      });
}

TEST_F(ValueSerializerTest, DecodeNumber) {
  // 42 zig-zag encoded (signed)
  DecodeTest({0xff, 0x09, 0x49, 0x54},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               EXPECT_EQ(42, Int32::Cast(*value)->Value());
             });
  // 42 varint encoded (unsigned)
  DecodeTest({0xff, 0x09, 0x55, 0x2a},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               EXPECT_EQ(42, Int32::Cast(*value)->Value());
             });
  // 160 zig-zag encoded (signed)
  DecodeTest({0xff, 0x09, 0x49, 0xc0, 0x02},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               ASSERT_EQ(160, Int32::Cast(*value)->Value());
             });
  // 160 varint encoded (unsigned)
  DecodeTest({0xff, 0x09, 0x55, 0xa0, 0x01},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsInt32());
               ASSERT_EQ(160, Int32::Cast(*value)->Value());
             });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  // IEEE 754 doubles, little-endian byte order
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_EQ(-0.25, Number::Cast(*value)->Value());
             });
  // quiet NaN
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x7f},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
             });
  // signaling NaN
  DecodeTest({0xff, 0x09, 0x4e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf4, 0x7f},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsNumber());
               EXPECT_TRUE(std::isnan(Number::Cast(*value)->Value()));
             });
#endif
  // TODO(jbroman): Equivalent test for big-endian machines.
}

// String constants (in UTF-8) used for string encoding tests.
static const char kHelloString[] = "Hello";
static const char kQuebecString[] = "\x51\x75\xC3\xA9\x62\x65\x63";
static const char kEmojiString[] = "\xF0\x9F\x91\x8A";

TEST_F(ValueSerializerTest, RoundTripString) {
  RoundTripTest([this]() { return String::Empty(isolate()); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(0, String::Cast(*value)->Length());
                });
  // Inside ASCII.
  RoundTripTest([this]() { return StringFromUtf8(kHelloString); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(5, String::Cast(*value)->Length());
                  EXPECT_EQ(kHelloString, Utf8Value(value));
                });
  // Inside Latin-1 (i.e. one-byte string), but not ASCII.
  RoundTripTest([this]() { return StringFromUtf8(kQuebecString); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(6, String::Cast(*value)->Length());
                  EXPECT_EQ(kQuebecString, Utf8Value(value));
                });
  // An emoji (decodes to two 16-bit chars).
  RoundTripTest([this]() { return StringFromUtf8(kEmojiString); },
                [](Local<Value> value) {
                  ASSERT_TRUE(value->IsString());
                  EXPECT_EQ(2, String::Cast(*value)->Length());
                  EXPECT_EQ(kEmojiString, Utf8Value(value));
                });
}

TEST_F(ValueSerializerTest, DecodeString) {
  // Decoding the strings above from UTF-8.
  DecodeTest({0xff, 0x09, 0x53, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(0, String::Cast(*value)->Length());
             });
  DecodeTest({0xff, 0x09, 0x53, 0x05, 'H', 'e', 'l', 'l', 'o'},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(5, String::Cast(*value)->Length());
               EXPECT_EQ(kHelloString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x53, 0x07, 'Q', 'u', 0xc3, 0xa9, 'b', 'e', 'c'},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(6, String::Cast(*value)->Length());
               EXPECT_EQ(kQuebecString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x53, 0x04, 0xf0, 0x9f, 0x91, 0x8a},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(2, String::Cast(*value)->Length());
               EXPECT_EQ(kEmojiString, Utf8Value(value));
             });

// And from two-byte strings (endianness dependent).
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x63, 0x00},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(0, String::Cast(*value)->Length());
             });
  DecodeTest({0xff, 0x09, 0x63, 0x0a, 'H', '\0', 'e', '\0', 'l', '\0', 'l',
              '\0', 'o', '\0'},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(5, String::Cast(*value)->Length());
               EXPECT_EQ(kHelloString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x63, 0x0c, 'Q', '\0', 'u', '\0', 0xe9, '\0', 'b',
              '\0', 'e', '\0', 'c', '\0'},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(6, String::Cast(*value)->Length());
               EXPECT_EQ(kQuebecString, Utf8Value(value));
             });
  DecodeTest({0xff, 0x09, 0x63, 0x04, 0x3d, 0xd8, 0x4a, 0xdc},
             [](Local<Value> value) {
               ASSERT_TRUE(value->IsString());
               EXPECT_EQ(2, String::Cast(*value)->Length());
               EXPECT_EQ(kEmojiString, Utf8Value(value));
             });
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, DecodeInvalidString) {
  // UTF-8 string with too few bytes available.
  InvalidDecodeTest({0xff, 0x09, 0x53, 0x10, 'v', '8'});
#if defined(V8_TARGET_LITTLE_ENDIAN)
  // Two-byte string with too few bytes available.
  InvalidDecodeTest({0xff, 0x09, 0x63, 0x10, 'v', '\0', '8', '\0'});
  // Two-byte string with an odd byte length.
  InvalidDecodeTest({0xff, 0x09, 0x63, 0x03, 'v', '\0', '8'});
#endif
  // TODO(jbroman): The same for big-endian systems.
}

TEST_F(ValueSerializerTest, EncodeTwoByteStringUsesPadding) {
  // As long as the output has a version that Blink expects to be able to read,
  // we must respect its alignment requirements. It requires that two-byte
  // characters be aligned.
  EncodeTest(
      [this]() {
        // We need a string whose length will take two bytes to encode, so that
        // a padding byte is needed to keep the characters aligned. The string
        // must also have a two-byte character, so that it gets the two-byte
        // encoding.
        std::string string(200, ' ');
        string += kEmojiString;
        return StringFromUtf8(string.c_str());
      },
      [](const std::vector<uint8_t>& data) {
        // This is a sufficient but not necessary condition to be aligned.
        // Note that the third byte (0x00) is padding.
        const uint8_t expected_prefix[] = {0xff, 0x09, 0x00, 0x63, 0x94, 0x03};
        ASSERT_GT(data.size(), sizeof(expected_prefix) / sizeof(uint8_t));
        EXPECT_TRUE(std::equal(std::begin(expected_prefix),
                               std::end(expected_prefix), data.begin()));
      });
}

TEST_F(ValueSerializerTest, RoundTripDictionaryObject) {
  // Empty object.
  RoundTripTest("({})", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Object.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 0"));
  });
  // String key.
  RoundTripTest("({ a: 42 })", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 1"));
  });
  // Integer key (treated as a string, but may be encoded differently).
  RoundTripTest("({ 42: 'a' })", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsObject());
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).length === 1"));
  });
  // Key order must be preserved.
  RoundTripTest("({ x: 1, y: 2, a: 3 })", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
  });
  // A harder case of enumeration order.
  // Indexes first, in order (but not 2^32 - 1, which is not an index), then the
  // remaining (string) keys, in the order they were defined.
  RoundTripTest(
      "({ a: 2, 0xFFFFFFFF: 1, 0xFFFFFFFE: 3, 1: 0 })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === "
            "'1,4294967294,a,4294967295'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 2"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFF] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFE] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 0"));
      });
  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  RoundTripTest(
      "(() => { var y = {}; y.self = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result === result.self"));
      });
}

TEST_F(ValueSerializerTest, DecodeDictionaryObject) {
  // Empty object.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6f, 0x7b, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsObject());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Object.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getOwnPropertyNames(result).length === 0"));
             });
  // String key.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01,
       0x49, 0x54, 0x7b, 0x01},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Integer key (treated as a string, but may be encoded differently).
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x49, 0x54, 0x3f, 0x01, 0x53,
       0x01, 0x61, 0x7b, 0x01},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Key order must be preserved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x78, 0x3f, 0x01,
       0x49, 0x02, 0x3f, 0x01, 0x53, 0x01, 0x79, 0x3f, 0x01, 0x49, 0x04, 0x3f,
       0x01, 0x53, 0x01, 0x61, 0x3f, 0x01, 0x49, 0x06, 0x7b, 0x03},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
      });
  // A harder case of enumeration order.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x49, 0x02, 0x3f, 0x01,
       0x49, 0x00, 0x3f, 0x01, 0x55, 0xfe, 0xff, 0xff, 0xff, 0x0f, 0x3f,
       0x01, 0x49, 0x06, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01, 0x49,
       0x04, 0x3f, 0x01, 0x53, 0x0a, 0x34, 0x32, 0x39, 0x34, 0x39, 0x36,
       0x37, 0x32, 0x39, 0x35, 0x3f, 0x01, 0x49, 0x02, 0x7b, 0x04},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === "
            "'1,4294967294,a,4294967295'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 2"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFF] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0xFFFFFFFE] === 3"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 0"));
      });
  // This detects a fairly subtle case: the object itself must be in the map
  // before its properties are deserialized, so that references to it can be
  // resolved.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x04, 0x73,
       0x65, 0x6c, 0x66, 0x3f, 0x01, 0x5e, 0x00, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result === result.self"));
      });
}

TEST_F(ValueSerializerTest, RoundTripOnlyOwnEnumerableStringKeys) {
  // Only "own" properties should be serialized, not ones on the prototype.
  RoundTripTest("(() => { var x = {}; x.__proto__ = {a: 4}; return x; })()",
                [this](Local<Value> value) {
                  EXPECT_TRUE(EvaluateScriptForResultBool("!('a' in result)"));
                });
  // Only enumerable properties should be serialized.
  RoundTripTest(
      "(() => {"
      "  var x = {};"
      "  Object.defineProperty(x, 'a', {value: 1, enumerable: false});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('a' in result)"));
      });
  // Symbol keys should not be serialized.
  RoundTripTest("({ [Symbol()]: 4 })", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getOwnPropertySymbols(result).length === 0"));
  });
}

TEST_F(ValueSerializerTest, RoundTripTrickyGetters) {
  // Keys are enumerated before any setters are called, but if there is no own
  // property when the value is to be read, then it should not be serialized.
  RoundTripTest("({ get a() { delete this.b; return 1; }, b: 2 })",
                [this](Local<Value> value) {
                  EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
                });
  // Keys added after the property enumeration should not be serialized.
  RoundTripTest("({ get a() { this.b = 3; }})", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
  });
  // But if you remove a key and add it back, that's fine. But it will appear in
  // the original place in enumeration order.
  RoundTripTest(
      "({ get a() { delete this.b; this.b = 4; }, b: 2, c: 3 })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'a,b,c'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.b === 4"));
      });
  // Similarly, it only matters if a property was enumerable when the
  // enumeration happened.
  RoundTripTest(
      "({ get a() {"
      "    Object.defineProperty(this, 'b', {value: 2, enumerable: false});"
      "}, b: 1})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.b === 2"));
      });
  RoundTripTest(
      "(() => {"
      "  var x = {"
      "    get a() {"
      "      Object.defineProperty(this, 'b', {value: 2, enumerable: true});"
      "    }"
      "  };"
      "  Object.defineProperty(x, 'b',"
      "      {value: 1, enumerable: false, configurable: true});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
      });
  // The property also should not be read if it can only be found on the
  // prototype chain (but not as an own property) after enumeration.
  RoundTripTest(
      "(() => {"
      "  var x = { get a() { delete this.b; }, b: 1 };"
      "  x.__proto__ = { b: 0 };"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("!('b' in result)"));
      });
  // If an exception is thrown by script, encoding must fail and the exception
  // must be thrown.
  InvalidEncodeTest("({ get a() { throw new Error('sentinel'); } })",
                    [](Local<Message> message) {
                      ASSERT_FALSE(message.IsEmpty());
                      EXPECT_NE(std::string::npos,
                                Utf8Value(message->Get()).find("sentinel"));
                    });
}

TEST_F(ValueSerializerTest, DecodeDictionaryObjectVersion0) {
  // Empty object.
  DecodeTestForVersion0(
      {0x7b, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Object.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 0"));
      });
  // String key.
  DecodeTestForVersion0(
      {0x53, 0x01, 0x61, 0x49, 0x54, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Object.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('a')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Integer key (treated as a string, but may be encoded differently).
  DecodeTestForVersion0(
      {0x49, 0x54, 0x53, 0x01, 0x61, 0x7b, 0x01, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsObject());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty('42')"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).length === 1"));
      });
  // Key order must be preserved.
  DecodeTestForVersion0(
      {0x53, 0x01, 0x78, 0x49, 0x02, 0x53, 0x01, 0x79, 0x49, 0x04, 0x53, 0x01,
       0x61, 0x49, 0x06, 0x7b, 0x03, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === 'x,y,a'"));
      });
  // A property and an element.
  DecodeTestForVersion0(
      {0x49, 0x54, 0x53, 0x01, 0x61, 0x53, 0x01, 0x61, 0x49, 0x54, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getOwnPropertyNames(result).toString() === '42,a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[42] === 'a'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === 42"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArray) {
  // A simple array of integers.
  RoundTripTest("[1, 2, 3, 4, 5]", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArray());
    EXPECT_EQ(5, Array::Cast(*value)->Length());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Array.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '1,2,3,4,5'"));
  });
  // A long (sparse) array.
  RoundTripTest(
      "(() => { var x = new Array(1000); x[500] = 42; return x; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[500] === 42"));
      });
  // Duplicate reference.
  RoundTripTest(
      "(() => { var y = {}; return [y, y]; })()", [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result[1]"));
      });
  // Duplicate reference in a sparse array.
  RoundTripTest(
      "(() => { var x = new Array(1000); x[1] = x[500] = {}; return x; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'object'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === result[500]"));
      });
  // Self reference.
  RoundTripTest(
      "(() => { var y = []; y[0] = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result"));
      });
  // Self reference in a sparse array.
  RoundTripTest(
      "(() => { var y = new Array(1000); y[519] = y; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[519] === result"));
      });
  // Array with additional properties.
  RoundTripTest(
      "(() => { var y = [1, 2]; y.foo = 'bar'; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '1,2'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // Sparse array with additional properties.
  RoundTripTest(
      "(() => { var y = new Array(1000); y.foo = 'bar'; return y; })()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.toString() === ','.repeat(999)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // The distinction between holes and undefined elements must be maintained.
  RoundTripTest("[,undefined]", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsArray());
    ASSERT_EQ(2, Array::Cast(*value)->Length());
    EXPECT_TRUE(
        EvaluateScriptForResultBool("typeof result[0] === 'undefined'"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(0)"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty(1)"));
  });
}

TEST_F(ValueSerializerTest, DecodeArray) {
  // A simple array of integers.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x41, 0x05, 0x3f, 0x01, 0x49, 0x02,
              0x3f, 0x01, 0x49, 0x04, 0x3f, 0x01, 0x49, 0x06, 0x3f, 0x01,
              0x49, 0x08, 0x3f, 0x01, 0x49, 0x0a, 0x24, 0x00, 0x05, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               EXPECT_EQ(5, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Array.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '1,2,3,4,5'"));
             });
  // A long (sparse) array.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
              0xe8, 0x07, 0x3f, 0x01, 0x49, 0x54, 0x40, 0x01, 0xe8, 0x07},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               EXPECT_EQ(1000, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool("result[500] === 42"));
             });
  // Duplicate reference.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x41, 0x02, 0x3f, 0x01, 0x6f, 0x7b, 0x00, 0x3f,
       0x02, 0x5e, 0x01, 0x24, 0x00, 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result[1]"));
      });
  // Duplicate reference in a sparse array.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
       0x02, 0x3f, 0x01, 0x6f, 0x7b, 0x00, 0x3f, 0x02, 0x49, 0xe8,
       0x07, 0x3f, 0x02, 0x5e, 0x01, 0x40, 0x02, 0xe8, 0x07, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'object'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === result[500]"));
      });
  // Self reference.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x41, 0x01, 0x3f, 0x01, 0x5e, 0x00, 0x24,
              0x00, 0x01, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               ASSERT_EQ(1, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === result"));
             });
  // Self reference in a sparse array.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01, 0x49,
       0x8e, 0x08, 0x3f, 0x01, 0x5e, 0x00, 0x40, 0x01, 0xe8, 0x07},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[519] === result"));
      });
  // Array with additional properties.
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x41, 0x02, 0x3f, 0x01, 0x49, 0x02, 0x3f,
       0x01, 0x49, 0x04, 0x3f, 0x01, 0x53, 0x03, 0x66, 0x6f, 0x6f, 0x3f,
       0x01, 0x53, 0x03, 0x62, 0x61, 0x72, 0x24, 0x01, 0x02, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '1,2'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
      });
  // Sparse array with additional properties.
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x61, 0xe8, 0x07, 0x3f, 0x01,
              0x53, 0x03, 0x66, 0x6f, 0x6f, 0x3f, 0x01, 0x53, 0x03,
              0x62, 0x61, 0x72, 0x40, 0x01, 0xe8, 0x07, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsArray());
               ASSERT_EQ(1000, Array::Cast(*value)->Length());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === ','.repeat(999)"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
             });
  // The distinction between holes and undefined elements must be maintained.
  // Note that since the previous output from Chrome fails this test, an
  // encoding using the sparse format was constructed instead.
  DecodeTest(
      {0xff, 0x09, 0x61, 0x02, 0x49, 0x02, 0x5f, 0x40, 0x01, 0x02},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[0] === 'undefined'"));
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(0)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.hasOwnProperty(1)"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArrayWithNonEnumerableElement) {
  // Even though this array looks like [1,5,3], the 5 should be missing from the
  // perspective of structured clone, which only clones properties that were
  // enumerable.
  RoundTripTest(
      "(() => {"
      "  var x = [1,2,3];"
      "  Object.defineProperty(x, '1', {enumerable:false, value:5});"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(3, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty('1')"));
      });
}

TEST_F(ValueSerializerTest, RoundTripArrayWithTrickyGetters) {
  // If an element is deleted before it is serialized, then it's deleted.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; }}, 42];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(1)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; }}, 42];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("typeof result[1] === 'undefined'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(1)"));
      });
  // If the length is changed, then the resulting array still has the original
  // length, but elements that were not yet serialized are gone.
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 0; }}, 3, 4];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(4, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(2)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [1, { get a() { x.length = 0; }}, 3, 4];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[0] === 1"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!result.hasOwnProperty(2)"));
      });
  // If a getter makes a property non-enumerable, it should still be enumerated
  // as enumeration happens once before getters are invoked.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() {"
      "    Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "  }}, 2];"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 3"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() {"
      "    Object.defineProperty(x, '1', { value: 3, enumerable: false });"
      "  }}, 2];"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 3"));
      });
  // Getters on the array itself must also run.
  RoundTripTest(
      "(() => {"
      "  var x = [1, 2, 3];"
      "  Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(3, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 4"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [1, 2, 3];"
      "  Object.defineProperty(x, '1', { enumerable: true, get: () => 4 });"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] === 4"));
      });
  // Even with a getter that deletes things, we don't read from the prototype.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; } }, 2];"
      "  x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
      });
  // Same for sparse arrays.
  RoundTripTest(
      "(() => {"
      "  var x = [{ get a() { delete x[1]; } }, 2];"
      "  x.__proto__ = Object.create(Array.prototype, { 1: { value: 6 } });"
      "  x.length = 1000;"
      "  return x;"
      "})()",
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        ASSERT_EQ(1000, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
      });
}

TEST_F(ValueSerializerTest, DecodeSparseArrayVersion0) {
  // Empty (sparse) array.
  DecodeTestForVersion0({0x40, 0x00, 0x00, 0x00},
                        [this](Local<Value> value) {
                          ASSERT_TRUE(value->IsArray());
                          ASSERT_EQ(0, Array::Cast(*value)->Length());
                        });
  // Sparse array with a mixture of elements and properties.
  DecodeTestForVersion0(
      {0x55, 0x00, 0x53, 0x01, 'a',  0x55, 0x02, 0x55, 0x05, 0x53,
       0x03, 'f',  'o',  'o',  0x53, 0x03, 'b',  'a',  'r',  0x53,
       0x03, 'b',  'a',  'z',  0x49, 0x0b, 0x40, 0x04, 0x03, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(3, Array::Cast(*value)->Length());
        EXPECT_TRUE(
            EvaluateScriptForResultBool("result.toString() === 'a,,5'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!(1 in result)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.foo === 'bar'"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.baz === -6"));
      });
  // Sparse array in a sparse array (sanity check of nesting).
  DecodeTestForVersion0(
      {0x55, 0x01, 0x55, 0x01, 0x54, 0x40, 0x01, 0x02, 0x40, 0x01, 0x02, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsArray());
        EXPECT_EQ(2, Array::Cast(*value)->Length());
        EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result)"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1] instanceof Array"));
        EXPECT_TRUE(EvaluateScriptForResultBool("!(0 in result[1])"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result[1][1] === true"));
      });
}

TEST_F(ValueSerializerTest, RoundTripDate) {
  RoundTripTest("new Date(1e6)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
    EXPECT_TRUE("Object.getPrototypeOf(result) === Date.prototype");
  });
  RoundTripTest("new Date(Date.UTC(1867, 6, 1))", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_TRUE("result.toISOString() === '1867-07-01T00:00:00.000Z'");
  });
  RoundTripTest("new Date(NaN)", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsDate());
    EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
  });
  RoundTripTest(
      "({ a: new Date(), get b() { return this.a; } })",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Date"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeDate) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x80, 0x84,
              0x2e, 0x41, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
               EXPECT_TRUE("Object.getPrototypeOf(result) === Date.prototype");
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x20, 0x45, 0x27, 0x89, 0x87,
       0xc2, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsDate());
        EXPECT_TRUE("result.toISOString() === '1867-07-01T00:00:00.000Z'");
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf8, 0x7f, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
             });
#else
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x41, 0x2e, 0x84, 0x80, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_EQ(1e6, Date::Cast(*value)->ValueOf());
               EXPECT_TRUE("Object.getPrototypeOf(result) === Date.prototype");
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x44, 0xc2, 0x87, 0x89, 0x27, 0x45, 0x20, 0x00,
       0x00, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsDate());
        EXPECT_TRUE("result.toISOString() === '1867-07-01T00:00:00.000Z'");
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x44, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsDate());
               EXPECT_TRUE(std::isnan(Date::Cast(*value)->ValueOf()));
             });
#endif
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f,
       0x01, 0x44, 0x00, 0x20, 0x39, 0x50, 0x37, 0x6a, 0x75, 0x42, 0x3f,
       0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Date"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, RoundTripValueObjects) {
  RoundTripTest("new Boolean(true)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Boolean.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === true"));
  });
  RoundTripTest("new Boolean(false)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Boolean.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === false"));
  });
  RoundTripTest(
      "({ a: new Boolean(true), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Boolean"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  RoundTripTest("new Number(-42)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Number.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
  });
  RoundTripTest("new Number(NaN)", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === Number.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("Number.isNaN(result.valueOf())"));
  });
  RoundTripTest(
      "({ a: new Number(6), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Number"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  RoundTripTest("new String('Qu\\xe9bec')", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === String.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.valueOf() === 'Qu\\xe9bec'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 6"));
  });
  RoundTripTest("new String('\\ud83d\\udc4a')", [this](Local<Value> value) {
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === String.prototype"));
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.valueOf() === '\\ud83d\\udc4a'"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 2"));
  });
  RoundTripTest(
      "({ a: new String(), get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof String"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, RejectsOtherValueObjects) {
  // This is a roundabout way of getting an instance of Symbol.
  InvalidEncodeTest("Object.valueOf.apply(Symbol())");
}

TEST_F(ValueSerializerTest, DecodeValueObjects) {
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x79, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Boolean.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === true"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x78, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Boolean.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === false"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f, 0x01,
       0x79, 0x3f, 0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Boolean"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
#if defined(V8_TARGET_LITTLE_ENDIAN)
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x45,
       0xc0, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Number.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0xf8, 0x7f, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Number.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Number.isNaN(result.valueOf())"));
             });
#else
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6e, 0xc0, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "Object.getPrototypeOf(result) === Number.prototype"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.valueOf() === -42"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x6e, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === Number.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Number.isNaN(result.valueOf())"));
             });
#endif
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61, 0x3f,
       0x01, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x40, 0x3f,
       0x02, 0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof Number"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x73, 0x07, 0x51, 0x75, 0xc3, 0xa9, 0x62,
              0x65, 0x63, 0x00},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === String.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.valueOf() === 'Qu\\xe9bec'"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 6"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x73, 0x04, 0xf0, 0x9f, 0x91, 0x8a},
             [this](Local<Value> value) {
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === String.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.valueOf() === '\\ud83d\\udc4a'"));
               EXPECT_TRUE(EvaluateScriptForResultBool("result.length === 2"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01,
       0x61, 0x3f, 0x01, 0x73, 0x00, 0x3f, 0x02, 0x53, 0x01,
       0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof String"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, RoundTripRegExp) {
  RoundTripTest("/foo/g", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsRegExp());
    EXPECT_TRUE(EvaluateScriptForResultBool(
        "Object.getPrototypeOf(result) === RegExp.prototype"));
    EXPECT_TRUE(EvaluateScriptForResultBool("result.toString() === '/foo/g'"));
  });
  RoundTripTest("new RegExp('Qu\\xe9bec', 'i')", [this](Local<Value> value) {
    ASSERT_TRUE(value->IsRegExp());
    EXPECT_TRUE(
        EvaluateScriptForResultBool("result.toString() === '/Qu\\xe9bec/i'"));
  });
  RoundTripTest("new RegExp('\\ud83d\\udc4a', 'ug')",
                [this](Local<Value> value) {
                  ASSERT_TRUE(value->IsRegExp());
                  EXPECT_TRUE(EvaluateScriptForResultBool(
                      "result.toString() === '/\\ud83d\\udc4a/gu'"));
                });
  RoundTripTest(
      "({ a: /foo/gi, get b() { return this.a; }})",
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof RegExp"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

TEST_F(ValueSerializerTest, DecodeRegExp) {
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x01},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "Object.getPrototypeOf(result) === RegExp.prototype"));
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/foo/g'"));
             });
  DecodeTest({0xff, 0x09, 0x3f, 0x00, 0x52, 0x07, 0x51, 0x75, 0xc3, 0xa9, 0x62,
              0x65, 0x63, 0x02},
             [this](Local<Value> value) {
               ASSERT_TRUE(value->IsRegExp());
               EXPECT_TRUE(EvaluateScriptForResultBool(
                   "result.toString() === '/Qu\\xe9bec/i'"));
             });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x52, 0x04, 0xf0, 0x9f, 0x91, 0x8a, 0x11, 0x00},
      [this](Local<Value> value) {
        ASSERT_TRUE(value->IsRegExp());
        EXPECT_TRUE(EvaluateScriptForResultBool(
            "result.toString() === '/\\ud83d\\udc4a/gu'"));
      });
  DecodeTest(
      {0xff, 0x09, 0x3f, 0x00, 0x6f, 0x3f, 0x01, 0x53, 0x01, 0x61,
       0x3f, 0x01, 0x52, 0x03, 0x66, 0x6f, 0x6f, 0x03, 0x3f, 0x02,
       0x53, 0x01, 0x62, 0x3f, 0x02, 0x5e, 0x01, 0x7b, 0x02, 0x00},
      [this](Local<Value> value) {
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a instanceof RegExp"));
        EXPECT_TRUE(EvaluateScriptForResultBool("result.a === result.b"));
      });
}

}  // namespace
}  // namespace v8
