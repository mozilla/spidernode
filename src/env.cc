#include "node_internals.h"
#include "async-wrap.h"
#include "v8-profiler.h"
#include "node_buffer.h"
#include "node_platform.h"

#if defined(_MSC_VER)
#define getpid GetCurrentProcessId
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <algorithm>

namespace node {

using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Private;
using v8::StackFrame;
using v8::StackTrace;
using v8::String;

IsolateData::IsolateData(Isolate* isolate,
                         uv_loop_t* event_loop,
                         MultiIsolatePlatform* platform,
                         uint32_t* zero_fill_field) :

// Create string and private symbol properties as internalized one byte strings.
//
// Internalized because it makes property lookups a little faster and because
// the string is created in the old space straight away.  It's going to end up
// in the old space sooner or later anyway but now it doesn't go through
// v8::Eternal's new space handling first.
//
// One byte because our strings are ASCII and we can safely skip V8's UTF-8
// decoding step.  It's a one-time cost, but why pay it when you don't have to?
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        Private::New(                                                         \
            isolate,                                                          \
            String::NewFromOneByte(                                           \
                isolate,                                                      \
                reinterpret_cast<const uint8_t*>(StringValue),                \
                v8::NewStringType::kInternalized,                             \
                sizeof(StringValue) - 1).ToLocalChecked())),
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                          \
    PropertyName ## _(                                                        \
        isolate,                                                              \
        String::NewFromOneByte(                                               \
            isolate,                                                          \
            reinterpret_cast<const uint8_t*>(StringValue),                    \
            v8::NewStringType::kInternalized,                                 \
            sizeof(StringValue) - 1).ToLocalChecked()),
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
    isolate_(isolate),
    event_loop_(event_loop),
    zero_fill_field_(zero_fill_field),
    platform_(platform) {
  if (platform_ != nullptr)
    platform_->RegisterIsolate(this, event_loop);
}

IsolateData::~IsolateData() {
  if (platform_ != nullptr)
    platform_->UnregisterIsolate(this);
}

void Environment::Start(int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv,
                        bool start_profiler_idle_notifier) {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());

  uv_check_init(event_loop(), immediate_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(immediate_check_handle()));

  uv_idle_init(event_loop(), immediate_idle_handle());

  // Inform V8's CPU profiler when we're idle.  The profiler is sampling-based
  // but not all samples are created equal; mark the wall clock time spent in
  // epoll_wait() and friends so profiling tools can filter it out.  The samples
  // still end up in v8.log but with state=IDLE rather than state=EXTERNAL.
  // TODO(bnoordhuis) Depends on a libuv implementation detail that we should
  // probably fortify in the API contract, namely that the last started prepare
  // or check watcher runs first.  It's not 100% foolproof; if an add-on starts
  // a prepare or check watcher after us, any samples attributed to its callback
  // will be recorded with state=IDLE.
  uv_prepare_init(event_loop(), &idle_prepare_handle_);
  uv_check_init(event_loop(), &idle_check_handle_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_check_handle_));

  uv_timer_init(event_loop(), destroy_async_ids_timer_handle());

  auto close_and_finish = [](Environment* env, uv_handle_t* handle, void* arg) {
    handle->data = env;

    uv_close(handle, [](uv_handle_t* handle) {
      static_cast<Environment*>(handle->data)->FinishHandleCleanup(handle);
    });
  };

  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_check_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_idle_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_check_handle_),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&destroy_async_ids_timer_handle_),
      close_and_finish,
      nullptr);

  if (start_profiler_idle_notifier) {
    StartProfilerIdleNotifier();
  }

  auto process_template = FunctionTemplate::New(isolate());
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate(), "process"));

  auto process_object =
      process_template->GetFunction()->NewInstance(context()).ToLocalChecked();
  set_process_object(process_object);

  SetupProcessObject(this, argc, argv, exec_argc, exec_argv);
  LoadAsyncWrapperInfo(this);
}

void Environment::CleanupHandles() {
  while (HandleCleanup* hc = handle_cleanup_queue_.PopFront()) {
    handle_cleanup_waiting_++;
    hc->cb_(this, hc->handle_, hc->arg_);
    delete hc;
  }

  while (handle_cleanup_waiting_ != 0)
    uv_run(event_loop(), UV_RUN_ONCE);
}

void Environment::StartProfilerIdleNotifier() {
  uv_prepare_start(&idle_prepare_handle_, [](uv_prepare_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_prepare_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(true);
  });

  uv_check_start(&idle_check_handle_, [](uv_check_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_check_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(false);
  });
}

void Environment::StopProfilerIdleNotifier() {
  uv_prepare_stop(&idle_prepare_handle_);
  uv_check_stop(&idle_check_handle_);
}

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_)
    return;

  HandleScope handle_scope(isolate());
  Local<v8::StackTrace> stack =
      StackTrace::CurrentStackTrace(isolate(), 10, StackTrace::kDetailed);

  fprintf(stderr, "(node:%d) WARNING: Detected use of sync API\n", getpid());

  for (int i = 0; i < stack->GetFrameCount() - 1; i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(i);
    node::Utf8Value fn_name_s(isolate(), stack_frame->GetFunctionName());
    node::Utf8Value script_name(isolate(), stack_frame->GetScriptName());
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();

    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr,
                "    at [eval] (%s:%i:%i)\n",
                *script_name,
                line_number,
                column);
      }
      break;
    }

    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr,
              "    at %s (%s:%i:%i)\n",
              *fn_name_s,
              *script_name,
              line_number,
              column);
    }
  }
  fflush(stderr);
}

void Environment::RunAtExitCallbacks() {
  for (AtExitCallback at_exit : at_exit_functions_) {
    at_exit.cb_(at_exit.arg_);
  }
  at_exit_functions_.clear();
}

void Environment::AtExit(void (*cb)(void* arg), void* arg) {
  at_exit_functions_.push_back(AtExitCallback{cb, arg});
}

void Environment::AddPromiseHook(promise_hook_func fn, void* arg) {
  auto it = std::find_if(
      promise_hooks_.begin(), promise_hooks_.end(),
      [&](const PromiseHookCallback& hook) {
        return hook.cb_ == fn && hook.arg_ == arg;
      });
  if (it != promise_hooks_.end()) {
    it->enable_count_++;
    return;
  }
  promise_hooks_.push_back(PromiseHookCallback{fn, arg, 1});

  if (promise_hooks_.size() == 1) {
    isolate_->SetPromiseHook(EnvPromiseHook);
  }
}

bool Environment::RemovePromiseHook(promise_hook_func fn, void* arg) {
  auto it = std::find_if(
      promise_hooks_.begin(), promise_hooks_.end(),
      [&](const PromiseHookCallback& hook) {
        return hook.cb_ == fn && hook.arg_ == arg;
      });

  if (it == promise_hooks_.end()) return false;

  if (--it->enable_count_ > 0) return true;

  promise_hooks_.erase(it);
  if (promise_hooks_.empty()) {
    isolate_->SetPromiseHook(nullptr);
  }

  return true;
}

bool Environment::EmitNapiWarning() {
  bool current_value = emit_napi_warning_;
  emit_napi_warning_ = false;
  return current_value;
}

void Environment::EnvPromiseHook(v8::PromiseHookType type,
                                 v8::Local<v8::Promise> promise,
                                 v8::Local<v8::Value> parent) {
  Environment* env = Environment::GetCurrent(promise->CreationContext());
  for (const PromiseHookCallback& hook : env->promise_hooks_) {
    hook.cb_(type, promise, parent, hook.arg_);
  }
}

void CollectExceptionInfo(Environment* env,
                          v8::Local<v8::Object> obj,
                          int errorno,
                          const char* err_string,
                          const char* syscall,
                          const char* message,
                          const char* path,
                          const char* dest) {
  obj->Set(env->errno_string(), v8::Integer::New(env->isolate(), errorno));

  obj->Set(env->context(), env->code_string(),
           OneByteString(env->isolate(), err_string)).FromJust();

  if (message != nullptr) {
    obj->Set(env->context(), env->message_string(),
             OneByteString(env->isolate(), message)).FromJust();
  }

  v8::Local<v8::Value> path_buffer;
  if (path != nullptr) {
    path_buffer =
      Buffer::Copy(env->isolate(), path, strlen(path)).ToLocalChecked();
    obj->Set(env->context(), env->path_string(), path_buffer).FromJust();
  }

  v8::Local<v8::Value> dest_buffer;
  if (dest != nullptr) {
    dest_buffer =
      Buffer::Copy(env->isolate(), dest, strlen(dest)).ToLocalChecked();
    obj->Set(env->context(), env->dest_string(), dest_buffer).FromJust();
  }

  if (syscall != nullptr) {
    obj->Set(env->context(), env->syscall_string(),
             OneByteString(env->isolate(), syscall)).FromJust();
  }
}

void Environment::CollectExceptionInfo(v8::Local<v8::Value> object,
                                       int errorno,
                                       const char* syscall,
                                       const char* message,
                                       const char* path) {
  if (!object->IsObject() || errorno == 0)
    return;

  v8::Local<v8::Object> obj = object.As<v8::Object>();
  const char* err_string = node::errno_string(errorno);

  if (message == nullptr || message[0] == '\0') {
    message = strerror(errorno);
  }

  node::CollectExceptionInfo(this, obj, errorno, err_string,
                             syscall, message, path, nullptr);
}

void Environment::CollectUVExceptionInfo(v8::Local<v8::Value> object,
                                         int errorno,
                                         const char* syscall,
                                         const char* message,
                                         const char* path,
                                         const char* dest) {
  if (!object->IsObject() || errorno == 0)
    return;

  v8::Local<v8::Object> obj = object.As<v8::Object>();
  const char* err_string = uv_err_name(errorno);

  if (message == nullptr || message[0] == '\0') {
    message = uv_strerror(errorno);
  }

  node::CollectExceptionInfo(this, obj, errorno, err_string,
                             syscall, message, path, dest);
}

}  // namespace node
