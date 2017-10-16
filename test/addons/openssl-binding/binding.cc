#include <node.h>
#include <assert.h>
#include <openssl/rand.h>

namespace {

inline void RandomBytes(const v8::FunctionCallbackInfo<v8::Value>& info) {
  assert(info[0]->IsArrayBufferView());
  auto view = info[0].As<v8::ArrayBufferView>();
  auto byte_offset = view->ByteOffset();
  auto byte_length = view->ByteLength();
  assert(view->HasBuffer());
  auto buffer = view->Buffer();
  auto contents = buffer->GetContents();
  auto data = static_cast<unsigned char*>(contents.Data()) + byte_offset;
  assert(RAND_poll());
  auto rval = RAND_bytes(data, static_cast<int>(byte_length));
  info.GetReturnValue().Set(rval > 0);
}

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  auto isolate = context->GetIsolate();
  auto key = v8::String::NewFromUtf8(isolate, "randomBytes");
  auto value = v8::FunctionTemplate::New(isolate, RandomBytes)->GetFunction();
  assert(exports->Set(context, key, value).IsJust());
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
