// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/messages.h"

#include <memory>

#include "src/api.h"
#include "src/execution.h"
#include "src/isolate-inl.h"
#include "src/keys.h"
#include "src/string-builder.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {

MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos)
    : script_(script), start_pos_(start_pos), end_pos_(end_pos) {}
MessageLocation::MessageLocation(Handle<Script> script, int start_pos,
                                 int end_pos, Handle<JSFunction> function)
    : script_(script),
      start_pos_(start_pos),
      end_pos_(end_pos),
      function_(function) {}
MessageLocation::MessageLocation() : start_pos_(-1), end_pos_(-1) {}

// If no message listeners have been registered this one is called
// by default.
void MessageHandler::DefaultMessageReport(Isolate* isolate,
                                          const MessageLocation* loc,
                                          Handle<Object> message_obj) {
  std::unique_ptr<char[]> str = GetLocalizedMessage(isolate, message_obj);
  if (loc == NULL) {
    PrintF("%s\n", str.get());
  } else {
    HandleScope scope(isolate);
    Handle<Object> data(loc->script()->name(), isolate);
    std::unique_ptr<char[]> data_str;
    if (data->IsString())
      data_str = Handle<String>::cast(data)->ToCString(DISALLOW_NULLS);
    PrintF("%s:%i: %s\n", data_str.get() ? data_str.get() : "<unknown>",
           loc->start_pos(), str.get());
  }
}


Handle<JSMessageObject> MessageHandler::MakeMessageObject(
    Isolate* isolate, MessageTemplate::Template message,
    MessageLocation* location, Handle<Object> argument,
    Handle<JSArray> stack_frames) {
  Factory* factory = isolate->factory();

  int start = -1;
  int end = -1;
  Handle<Object> script_handle = factory->undefined_value();
  if (location != NULL) {
    start = location->start_pos();
    end = location->end_pos();
    script_handle = Script::GetWrapper(location->script());
  } else {
    script_handle = Script::GetWrapper(isolate->factory()->empty_script());
  }

  Handle<Object> stack_frames_handle = stack_frames.is_null()
      ? Handle<Object>::cast(factory->undefined_value())
      : Handle<Object>::cast(stack_frames);

  Handle<JSMessageObject> message_obj = factory->NewJSMessageObject(
      message, argument, start, end, script_handle, stack_frames_handle);

  return message_obj;
}


void MessageHandler::ReportMessage(Isolate* isolate, MessageLocation* loc,
                                   Handle<JSMessageObject> message) {
  // We are calling into embedder's code which can throw exceptions.
  // Thus we need to save current exception state, reset it to the clean one
  // and ignore scheduled exceptions callbacks can throw.

  // We pass the exception object into the message handler callback though.
  Object* exception_object = isolate->heap()->undefined_value();
  if (isolate->has_pending_exception()) {
    exception_object = isolate->pending_exception();
  }
  Handle<Object> exception(exception_object, isolate);

  Isolate::ExceptionScope exception_scope(isolate);
  isolate->clear_pending_exception();
  isolate->set_external_caught_exception(false);

  // Turn the exception on the message into a string if it is an object.
  if (message->argument()->IsJSObject()) {
    HandleScope scope(isolate);
    Handle<Object> argument(message->argument(), isolate);

    MaybeHandle<Object> maybe_stringified;
    Handle<Object> stringified;
    // Make sure we don't leak uncaught internally generated Error objects.
    if (argument->IsJSError()) {
      maybe_stringified = Object::NoSideEffectsToString(isolate, argument);
    } else {
      v8::TryCatch catcher(reinterpret_cast<v8::Isolate*>(isolate));
      catcher.SetVerbose(false);
      catcher.SetCaptureMessage(false);

      maybe_stringified = Object::ToString(isolate, argument);
    }

    if (!maybe_stringified.ToHandle(&stringified)) {
      stringified = isolate->factory()->NewStringFromAsciiChecked("exception");
    }
    message->set_argument(*stringified);
  }

  v8::Local<v8::Message> api_message_obj = v8::Utils::MessageToLocal(message);
  v8::Local<v8::Value> api_exception_obj = v8::Utils::ToLocal(exception);

  Handle<TemplateList> global_listeners =
      isolate->factory()->message_listeners();
  int global_length = global_listeners->length();
  if (global_length == 0) {
    DefaultMessageReport(isolate, loc, message);
    if (isolate->has_scheduled_exception()) {
      isolate->clear_scheduled_exception();
    }
  } else {
    for (int i = 0; i < global_length; i++) {
      HandleScope scope(isolate);
      if (global_listeners->get(i)->IsUndefined(isolate)) continue;
      FixedArray* listener = FixedArray::cast(global_listeners->get(i));
      Foreign* callback_obj = Foreign::cast(listener->get(0));
      v8::MessageCallback callback =
          FUNCTION_CAST<v8::MessageCallback>(callback_obj->foreign_address());
      Handle<Object> callback_data(listener->get(1), isolate);
      {
        // Do not allow exceptions to propagate.
        v8::TryCatch try_catch(reinterpret_cast<v8::Isolate*>(isolate));
        callback(api_message_obj, callback_data->IsUndefined(isolate)
                                      ? api_exception_obj
                                      : v8::Utils::ToLocal(callback_data));
      }
      if (isolate->has_scheduled_exception()) {
        isolate->clear_scheduled_exception();
      }
    }
  }
}


Handle<String> MessageHandler::GetMessage(Isolate* isolate,
                                          Handle<Object> data) {
  Handle<JSMessageObject> message = Handle<JSMessageObject>::cast(data);
  Handle<Object> arg = Handle<Object>(message->argument(), isolate);
  return MessageTemplate::FormatMessage(isolate, message->type(), arg);
}

std::unique_ptr<char[]> MessageHandler::GetLocalizedMessage(
    Isolate* isolate, Handle<Object> data) {
  HandleScope scope(isolate);
  return GetMessage(isolate, data)->ToCString(DISALLOW_NULLS);
}


CallSite::CallSite(Isolate* isolate, Handle<JSObject> call_site_obj)
    : isolate_(isolate) {
  Handle<Object> maybe_function = JSObject::GetDataProperty(
      call_site_obj, isolate->factory()->call_site_function_symbol());
  if (maybe_function->IsJSFunction()) {
    // javascript
    fun_ = Handle<JSFunction>::cast(maybe_function);
    receiver_ = JSObject::GetDataProperty(
        call_site_obj, isolate->factory()->call_site_receiver_symbol());
  } else {
    Handle<Object> maybe_wasm_func_index = JSObject::GetDataProperty(
        call_site_obj, isolate->factory()->call_site_wasm_func_index_symbol());
    if (!maybe_wasm_func_index->IsSmi()) {
      // invalid: neither javascript nor wasm
      return;
    }
    // wasm
    wasm_obj_ = Handle<JSObject>::cast(JSObject::GetDataProperty(
        call_site_obj, isolate->factory()->call_site_wasm_obj_symbol()));
    wasm_func_index_ = Smi::cast(*maybe_wasm_func_index)->value();
    DCHECK(static_cast<int>(wasm_func_index_) >= 0);
  }

  CHECK(JSObject::GetDataProperty(
            call_site_obj, isolate->factory()->call_site_position_symbol())
            ->ToInt32(&pos_));
}


Handle<Object> CallSite::GetFileName() {
  if (!IsJavaScript()) return isolate_->factory()->null_value();
  Object* script = fun_->shared()->script();
  if (!script->IsScript()) return isolate_->factory()->null_value();
  return Handle<Object>(Script::cast(script)->name(), isolate_);
}


Handle<Object> CallSite::GetFunctionName() {
  if (IsWasm()) {
    return wasm::GetWasmFunctionNameOrNull(isolate_, wasm_obj_,
                                           wasm_func_index_);
  }
  Handle<String> result = JSFunction::GetName(fun_);
  if (result->length() != 0) return result;

  Handle<Object> script(fun_->shared()->script(), isolate_);
  if (script->IsScript() &&
      Handle<Script>::cast(script)->compilation_type() ==
          Script::COMPILATION_TYPE_EVAL) {
    return isolate_->factory()->eval_string();
  }
  return isolate_->factory()->null_value();
}

Handle<Object> CallSite::GetScriptNameOrSourceUrl() {
  if (!IsJavaScript()) return isolate_->factory()->null_value();
  Object* script_obj = fun_->shared()->script();
  if (!script_obj->IsScript()) return isolate_->factory()->null_value();
  Handle<Script> script(Script::cast(script_obj), isolate_);
  Object* source_url = script->source_url();
  if (source_url->IsString()) return Handle<Object>(source_url, isolate_);
  return Handle<Object>(script->name(), isolate_);
}

bool CheckMethodName(Isolate* isolate, Handle<JSObject> obj, Handle<Name> name,
                     Handle<JSFunction> fun,
                     LookupIterator::Configuration config) {
  LookupIterator iter =
      LookupIterator::PropertyOrElement(isolate, obj, name, config);
  if (iter.state() == LookupIterator::DATA) {
    return iter.GetDataValue().is_identical_to(fun);
  } else if (iter.state() == LookupIterator::ACCESSOR) {
    Handle<Object> accessors = iter.GetAccessors();
    if (accessors->IsAccessorPair()) {
      Handle<AccessorPair> pair = Handle<AccessorPair>::cast(accessors);
      return pair->getter() == *fun || pair->setter() == *fun;
    }
  }
  return false;
}


Handle<Object> CallSite::GetMethodName() {
  if (!IsJavaScript() || receiver_->IsNull(isolate_) ||
      receiver_->IsUndefined(isolate_)) {
    return isolate_->factory()->null_value();
  }
  Handle<JSReceiver> receiver =
      Object::ToObject(isolate_, receiver_).ToHandleChecked();
  if (!receiver->IsJSObject()) {
    return isolate_->factory()->null_value();
  }

  Handle<JSObject> obj = Handle<JSObject>::cast(receiver);
  Handle<Object> function_name(fun_->shared()->name(), isolate_);
  if (function_name->IsString()) {
    Handle<String> name = Handle<String>::cast(function_name);
    // ES2015 gives getters and setters name prefixes which must
    // be stripped to find the property name.
    if (name->IsUtf8EqualTo(CStrVector("get "), true) ||
        name->IsUtf8EqualTo(CStrVector("set "), true)) {
      name = isolate_->factory()->NewProperSubString(name, 4, name->length());
    }
    if (CheckMethodName(isolate_, obj, name, fun_,
                        LookupIterator::PROTOTYPE_CHAIN_SKIP_INTERCEPTOR)) {
      return name;
    }
  }

  HandleScope outer_scope(isolate_);
  Handle<Object> result;
  for (PrototypeIterator iter(isolate_, obj, kStartAtReceiver); !iter.IsAtEnd();
       iter.Advance()) {
    Handle<Object> current = PrototypeIterator::GetCurrent(iter);
    if (!current->IsJSObject()) break;
    Handle<JSObject> current_obj = Handle<JSObject>::cast(current);
    if (current_obj->IsAccessCheckNeeded()) break;
    Handle<FixedArray> keys =
        KeyAccumulator::GetOwnEnumPropertyKeys(isolate_, current_obj);
    for (int i = 0; i < keys->length(); i++) {
      HandleScope inner_scope(isolate_);
      if (!keys->get(i)->IsName()) continue;
      Handle<Name> name_key(Name::cast(keys->get(i)), isolate_);
      if (!CheckMethodName(isolate_, current_obj, name_key, fun_,
                           LookupIterator::OWN_SKIP_INTERCEPTOR))
        continue;
      // Return null in case of duplicates to avoid confusion.
      if (!result.is_null()) return isolate_->factory()->null_value();
      result = inner_scope.CloseAndEscape(name_key);
    }
  }

  if (!result.is_null()) return outer_scope.CloseAndEscape(result);
  return isolate_->factory()->null_value();
}

Handle<Object> CallSite::GetTypeName() {
  // TODO(jgruber): Check for strict/constructor here as in
  // CallSitePrototypeGetThis.

  if (receiver_->IsNull(isolate_) || receiver_->IsUndefined(isolate_))
    return isolate_->factory()->null_value();

  if (receiver_->IsJSProxy()) return isolate_->factory()->Proxy_string();

  Handle<JSReceiver> receiver_object =
      Object::ToObject(isolate_, receiver_).ToHandleChecked();
  return JSReceiver::GetConstructorName(receiver_object);
}

namespace {

Object* EvalFromFunctionName(Isolate* isolate, Handle<Script> script) {
  if (script->eval_from_shared()->IsUndefined(isolate))
    return *isolate->factory()->undefined_value();

  Handle<SharedFunctionInfo> shared(
      SharedFunctionInfo::cast(script->eval_from_shared()));
  // Find the name of the function calling eval.
  if (shared->name()->BooleanValue()) {
    return shared->name();
  }

  return shared->inferred_name();
}

Object* EvalFromScript(Isolate* isolate, Handle<Script> script) {
  if (script->eval_from_shared()->IsUndefined(isolate))
    return *isolate->factory()->undefined_value();

  Handle<SharedFunctionInfo> eval_from_shared(
      SharedFunctionInfo::cast(script->eval_from_shared()));
  return eval_from_shared->script()->IsScript()
             ? eval_from_shared->script()
             : *isolate->factory()->undefined_value();
}

MaybeHandle<String> FormatEvalOrigin(Isolate* isolate, Handle<Script> script) {
  Handle<Object> sourceURL = Script::GetNameOrSourceURL(script);
  if (!sourceURL->IsUndefined(isolate)) {
    DCHECK(sourceURL->IsString());
    return Handle<String>::cast(sourceURL);
  }

  IncrementalStringBuilder builder(isolate);
  builder.AppendCString("eval at ");

  Handle<Object> eval_from_function_name =
      handle(EvalFromFunctionName(isolate, script), isolate);
  if (eval_from_function_name->BooleanValue()) {
    Handle<String> str;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, str, Object::ToString(isolate, eval_from_function_name),
        String);
    builder.AppendString(str);
  } else {
    builder.AppendCString("<anonymous>");
  }

  Handle<Object> eval_from_script_obj =
      handle(EvalFromScript(isolate, script), isolate);
  if (eval_from_script_obj->IsScript()) {
    Handle<Script> eval_from_script =
        Handle<Script>::cast(eval_from_script_obj);
    builder.AppendCString(" (");
    if (eval_from_script->compilation_type() == Script::COMPILATION_TYPE_EVAL) {
      // Eval script originated from another eval.
      Handle<String> str;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate, str, FormatEvalOrigin(isolate, eval_from_script), String);
      builder.AppendString(str);
    } else {
      DCHECK(eval_from_script->compilation_type() !=
             Script::COMPILATION_TYPE_EVAL);
      // eval script originated from "real" source.
      Handle<Object> name_obj = handle(eval_from_script->name(), isolate);
      if (eval_from_script->name()->IsString()) {
        builder.AppendString(Handle<String>::cast(name_obj));

        Script::PositionInfo info;
        if (eval_from_script->GetPositionInfo(script->GetEvalPosition(), &info,
                                              Script::NO_OFFSET)) {
          builder.AppendCString(":");

          Handle<String> str = isolate->factory()->NumberToString(
              handle(Smi::FromInt(info.line + 1), isolate));
          builder.AppendString(str);

          builder.AppendCString(":");

          str = isolate->factory()->NumberToString(
              handle(Smi::FromInt(info.column + 1), isolate));
          builder.AppendString(str);
        }
      } else {
        DCHECK(!eval_from_script->name()->IsString());
        builder.AppendCString("unknown source");
      }
    }
    builder.AppendCString(")");
  }

  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, builder.Finish(), String);
  return result;
}

}  // namespace

Handle<Object> CallSite::GetEvalOrigin() {
  if (IsWasm()) return isolate_->factory()->undefined_value();
  DCHECK(IsJavaScript());

  Handle<Object> script = handle(fun_->shared()->script(), isolate_);
  if (!script->IsScript()) return isolate_->factory()->undefined_value();

  return FormatEvalOrigin(isolate_, Handle<Script>::cast(script))
      .ToHandleChecked();
}

int CallSite::GetLineNumber() {
  if (pos_ >= 0 && IsJavaScript()) {
    Handle<Object> script_obj(fun_->shared()->script(), isolate_);
    if (script_obj->IsScript()) {
      Handle<Script> script = Handle<Script>::cast(script_obj);
      return Script::GetLineNumber(script, pos_) + 1;
    }
  }
  return -1;
}


int CallSite::GetColumnNumber() {
  if (pos_ >= 0 && IsJavaScript()) {
    Handle<Object> script_obj(fun_->shared()->script(), isolate_);
    if (script_obj->IsScript()) {
      Handle<Script> script = Handle<Script>::cast(script_obj);
      return Script::GetColumnNumber(script, pos_) + 1;
    }
  }
  return -1;
}


bool CallSite::IsNative() {
  if (!IsJavaScript()) return false;
  Handle<Object> script(fun_->shared()->script(), isolate_);
  return script->IsScript() &&
         Handle<Script>::cast(script)->type() == Script::TYPE_NATIVE;
}


bool CallSite::IsToplevel() {
  if (IsWasm()) return false;
  return receiver_->IsJSGlobalProxy() || receiver_->IsNull(isolate_) ||
         receiver_->IsUndefined(isolate_);
}


bool CallSite::IsEval() {
  if (!IsJavaScript()) return false;
  Handle<Object> script(fun_->shared()->script(), isolate_);
  return script->IsScript() &&
         Handle<Script>::cast(script)->compilation_type() ==
             Script::COMPILATION_TYPE_EVAL;
}


bool CallSite::IsConstructor() {
  // Builtin exit frames mark constructors by passing a special symbol as the
  // receiver.
  Object* ctor_symbol = isolate_->heap()->call_site_constructor_symbol();
  if (*receiver_ == ctor_symbol) return true;
  if (!IsJavaScript() || !receiver_->IsJSObject()) return false;
  Handle<Object> constructor =
      JSReceiver::GetDataProperty(Handle<JSObject>::cast(receiver_),
                                  isolate_->factory()->constructor_string());
  return constructor.is_identical_to(fun_);
}

namespace {

// Convert the raw frames as written by Isolate::CaptureSimpleStackTrace into
// a vector of JS CallSite objects.
MaybeHandle<FixedArray> GetStackFrames(Isolate* isolate,
                                       Handle<Object> raw_stack) {
  DCHECK(raw_stack->IsJSArray());
  Handle<JSArray> raw_stack_array = Handle<JSArray>::cast(raw_stack);

  DCHECK(raw_stack_array->elements()->IsFixedArray());
  Handle<FixedArray> raw_stack_elements =
      handle(FixedArray::cast(raw_stack_array->elements()), isolate);

  const int raw_stack_len = raw_stack_elements->length();
  DCHECK(raw_stack_len % 4 == 1);  // Multiples of 4 plus sloppy frames count.
  const int frame_count = (raw_stack_len - 1) / 4;

  Handle<Object> sloppy_frames_obj =
      FixedArray::get(*raw_stack_elements, 0, isolate);
  int sloppy_frames = Handle<Smi>::cast(sloppy_frames_obj)->value();

  int dst_ix = 0;
  Handle<FixedArray> frames = isolate->factory()->NewFixedArray(frame_count);
  for (int i = 1; i < raw_stack_len; i += 4) {
    Handle<Object> recv = FixedArray::get(*raw_stack_elements, i, isolate);
    Handle<Object> fun = FixedArray::get(*raw_stack_elements, i + 1, isolate);
    Handle<AbstractCode> code = Handle<AbstractCode>::cast(
        FixedArray::get(*raw_stack_elements, i + 2, isolate));
    Handle<Smi> pc =
        Handle<Smi>::cast(FixedArray::get(*raw_stack_elements, i + 3, isolate));

    Handle<Object> pos =
        (fun->IsSmi() && pc->value() < 0)
            ? handle(Smi::FromInt(-1 - pc->value()), isolate)
            : handle(Smi::FromInt(code->SourcePosition(pc->value())), isolate);

    sloppy_frames--;
    Handle<Object> strict = isolate->factory()->ToBoolean(sloppy_frames < 0);

    Handle<Object> callsite;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, callsite,
        CallSiteUtils::Construct(isolate, recv, fun, pos, strict), FixedArray);

    frames->set(dst_ix++, *callsite);
  }

  DCHECK_EQ(frame_count, dst_ix);
  return frames;
}

MaybeHandle<Object> AppendErrorString(Isolate* isolate, Handle<Object> error,
                                      IncrementalStringBuilder* builder) {
  MaybeHandle<String> err_str =
      ErrorUtils::ToString(isolate, Handle<Object>::cast(error));
  if (err_str.is_null()) {
    // Error.toString threw. Try to return a string representation of the thrown
    // exception instead.

    DCHECK(isolate->has_pending_exception());
    Handle<Object> pending_exception =
        handle(isolate->pending_exception(), isolate);
    isolate->clear_pending_exception();

    err_str = ErrorUtils::ToString(isolate, pending_exception);
    if (err_str.is_null()) {
      // Formatting the thrown exception threw again, give up.
      DCHECK(isolate->has_pending_exception());
      isolate->clear_pending_exception();

      builder->AppendCString("<error>");
    } else {
      // Formatted thrown exception successfully, append it.
      builder->AppendCString("<error: ");
      builder->AppendString(err_str.ToHandleChecked());
      builder->AppendCharacter('>');
    }
  } else {
    builder->AppendString(err_str.ToHandleChecked());
  }

  return error;
}

class PrepareStackTraceScope {
 public:
  explicit PrepareStackTraceScope(Isolate* isolate) : isolate_(isolate) {
    DCHECK(!isolate_->formatting_stack_trace());
    isolate_->set_formatting_stack_trace(true);
  }

  ~PrepareStackTraceScope() { isolate_->set_formatting_stack_trace(false); }

 private:
  Isolate* isolate_;

  DISALLOW_COPY_AND_ASSIGN(PrepareStackTraceScope);
};

}  // namespace

// static
MaybeHandle<Object> ErrorUtils::FormatStackTrace(Isolate* isolate,
                                                 Handle<JSObject> error,
                                                 Handle<Object> raw_stack) {
  // Create JS CallSite objects from the raw stack frame array.

  Handle<FixedArray> frames;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, frames,
                             GetStackFrames(isolate, raw_stack), Object);

  // If there's a user-specified "prepareStackFrames" function, call it on the
  // frames and use its result.

  Handle<JSFunction> global_error = isolate->error_function();
  Handle<Object> prepare_stack_trace;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, prepare_stack_trace,
      JSFunction::GetProperty(isolate, global_error, "prepareStackTrace"),
      Object);

  const bool in_recursion = isolate->formatting_stack_trace();
  if (prepare_stack_trace->IsJSFunction() && !in_recursion) {
    PrepareStackTraceScope scope(isolate);
    Handle<JSArray> array = isolate->factory()->NewJSArrayWithElements(frames);

    const int argc = 2;
    ScopedVector<Handle<Object>> argv(argc);
    argv[0] = error;
    argv[1] = array;

    Handle<Object> result;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, Execution::Call(isolate, prepare_stack_trace,
                                         global_error, argc, argv.start()),
        Object);

    return result;
  }

  IncrementalStringBuilder builder(isolate);

  RETURN_ON_EXCEPTION(isolate, AppendErrorString(isolate, error, &builder),
                      Object);

  for (int i = 0; i < frames->length(); i++) {
    builder.AppendCString("\n    at ");

    Handle<Object> frame = FixedArray::get(*frames, i, isolate);
    MaybeHandle<String> maybe_frame_string =
        CallSiteUtils::ToString(isolate, frame);
    if (maybe_frame_string.is_null()) {
      // CallSite.toString threw. Try to return a string representation of the
      // thrown exception instead.

      DCHECK(isolate->has_pending_exception());
      Handle<Object> pending_exception =
          handle(isolate->pending_exception(), isolate);
      isolate->clear_pending_exception();

      maybe_frame_string = ErrorUtils::ToString(isolate, pending_exception);
      if (maybe_frame_string.is_null()) {
        // Formatting the thrown exception threw again, give up.

        builder.AppendCString("<error>");
      } else {
        // Formatted thrown exception successfully, append it.
        builder.AppendCString("<error: ");
        builder.AppendString(maybe_frame_string.ToHandleChecked());
        builder.AppendCString("<error>");
      }
    } else {
      // CallSite.toString completed without throwing.
      builder.AppendString(maybe_frame_string.ToHandleChecked());
    }
  }

  RETURN_RESULT(isolate, builder.Finish(), Object);
}

Handle<String> MessageTemplate::FormatMessage(Isolate* isolate,
                                              int template_index,
                                              Handle<Object> arg) {
  Factory* factory = isolate->factory();
  Handle<String> result_string = Object::NoSideEffectsToString(isolate, arg);
  MaybeHandle<String> maybe_result_string = MessageTemplate::FormatMessage(
      template_index, result_string, factory->empty_string(),
      factory->empty_string());
  if (!maybe_result_string.ToHandle(&result_string)) {
    return factory->InternalizeOneByteString(STATIC_CHAR_VECTOR("<error>"));
  }
  // A string that has been obtained from JS code in this way is
  // likely to be a complicated ConsString of some sort.  We flatten it
  // here to improve the efficiency of converting it to a C string and
  // other operations that are likely to take place (see GetLocalizedMessage
  // for example).
  return String::Flatten(result_string);
}


const char* MessageTemplate::TemplateString(int template_index) {
  switch (template_index) {
#define CASE(NAME, STRING) \
  case k##NAME:            \
    return STRING;
    MESSAGE_TEMPLATES(CASE)
#undef CASE
    case kLastMessage:
    default:
      return NULL;
  }
}


MaybeHandle<String> MessageTemplate::FormatMessage(int template_index,
                                                   Handle<String> arg0,
                                                   Handle<String> arg1,
                                                   Handle<String> arg2) {
  Isolate* isolate = arg0->GetIsolate();
  const char* template_string = TemplateString(template_index);
  if (template_string == NULL) {
    isolate->ThrowIllegalOperation();
    return MaybeHandle<String>();
  }

  IncrementalStringBuilder builder(isolate);

  unsigned int i = 0;
  Handle<String> args[] = {arg0, arg1, arg2};
  for (const char* c = template_string; *c != '\0'; c++) {
    if (*c == '%') {
      // %% results in verbatim %.
      if (*(c + 1) == '%') {
        c++;
        builder.AppendCharacter('%');
      } else {
        DCHECK(i < arraysize(args));
        Handle<String> arg = args[i++];
        builder.AppendString(arg);
      }
    } else {
      builder.AppendCharacter(*c);
    }
  }

  return builder.Finish();
}

MaybeHandle<Object> ErrorUtils::Construct(
    Isolate* isolate, Handle<JSFunction> target, Handle<Object> new_target,
    Handle<Object> message, FrameSkipMode mode, Handle<Object> caller,
    bool suppress_detailed_trace) {
  // 1. If NewTarget is undefined, let newTarget be the active function object,
  // else let newTarget be NewTarget.

  Handle<JSReceiver> new_target_recv =
      new_target->IsJSReceiver() ? Handle<JSReceiver>::cast(new_target)
                                 : Handle<JSReceiver>::cast(target);

  // 2. Let O be ? OrdinaryCreateFromConstructor(newTarget, "%ErrorPrototype%",
  //    « [[ErrorData]] »).
  Handle<JSObject> err;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, err,
                             JSObject::New(target, new_target_recv), Object);

  // 3. If message is not undefined, then
  //  a. Let msg be ? ToString(message).
  //  b. Let msgDesc be the PropertyDescriptor{[[Value]]: msg, [[Writable]]:
  //     true, [[Enumerable]]: false, [[Configurable]]: true}.
  //  c. Perform ! DefinePropertyOrThrow(O, "message", msgDesc).
  // 4. Return O.

  if (!message->IsUndefined(isolate)) {
    Handle<String> msg_string;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, msg_string,
                               Object::ToString(isolate, message), Object);
    RETURN_ON_EXCEPTION(isolate, JSObject::SetOwnPropertyIgnoreAttributes(
                                     err, isolate->factory()->message_string(),
                                     msg_string, DONT_ENUM),
                        Object);
  }

  // Optionally capture a more detailed stack trace for the message.
  if (!suppress_detailed_trace) {
    RETURN_ON_EXCEPTION(isolate, isolate->CaptureAndSetDetailedStackTrace(err),
                        Object);
  }

  // Capture a simple stack trace for the stack property.
  RETURN_ON_EXCEPTION(isolate,
                      isolate->CaptureAndSetSimpleStackTrace(err, mode, caller),
                      Object);

  return err;
}

namespace {

MaybeHandle<String> GetStringPropertyOrDefault(Isolate* isolate,
                                               Handle<JSReceiver> recv,
                                               Handle<String> key,
                                               Handle<String> default_str) {
  Handle<Object> obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj, JSObject::GetProperty(recv, key),
                             String);

  Handle<String> str;
  if (obj->IsUndefined(isolate)) {
    str = default_str;
  } else {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, str, Object::ToString(isolate, obj),
                               String);
  }

  return str;
}

}  // namespace

// ES6 section 19.5.3.4 Error.prototype.toString ( )
MaybeHandle<String> ErrorUtils::ToString(Isolate* isolate,
                                         Handle<Object> receiver) {
  // 1. Let O be the this value.
  // 2. If Type(O) is not Object, throw a TypeError exception.
  if (!receiver->IsJSReceiver()) {
    return isolate->Throw<String>(isolate->factory()->NewTypeError(
        MessageTemplate::kIncompatibleMethodReceiver,
        isolate->factory()->NewStringFromAsciiChecked(
            "Error.prototype.toString"),
        receiver));
  }
  Handle<JSReceiver> recv = Handle<JSReceiver>::cast(receiver);

  // 3. Let name be ? Get(O, "name").
  // 4. If name is undefined, let name be "Error"; otherwise let name be
  // ? ToString(name).
  Handle<String> name_key = isolate->factory()->name_string();
  Handle<String> name_default = isolate->factory()->Error_string();
  Handle<String> name;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, name,
      GetStringPropertyOrDefault(isolate, recv, name_key, name_default),
      String);

  // 5. Let msg be ? Get(O, "message").
  // 6. If msg is undefined, let msg be the empty String; otherwise let msg be
  // ? ToString(msg).
  Handle<String> msg_key = isolate->factory()->message_string();
  Handle<String> msg_default = isolate->factory()->empty_string();
  Handle<String> msg;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, msg,
      GetStringPropertyOrDefault(isolate, recv, msg_key, msg_default), String);

  // 7. If name is the empty String, return msg.
  // 8. If msg is the empty String, return name.
  if (name->length() == 0) return msg;
  if (msg->length() == 0) return name;

  // 9. Return the result of concatenating name, the code unit 0x003A (COLON),
  // the code unit 0x0020 (SPACE), and msg.
  IncrementalStringBuilder builder(isolate);
  builder.AppendString(name);
  builder.AppendCString(": ");
  builder.AppendString(msg);

  Handle<String> result;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, result, builder.Finish(), String);
  return result;
}

namespace {

Handle<String> FormatMessage(Isolate* isolate, int template_index,
                             Handle<Object> arg0, Handle<Object> arg1,
                             Handle<Object> arg2) {
  Handle<String> arg0_str = Object::NoSideEffectsToString(isolate, arg0);
  Handle<String> arg1_str = Object::NoSideEffectsToString(isolate, arg1);
  Handle<String> arg2_str = Object::NoSideEffectsToString(isolate, arg2);

  isolate->native_context()->IncrementErrorsThrown();

  Handle<String> msg;
  if (!MessageTemplate::FormatMessage(template_index, arg0_str, arg1_str,
                                      arg2_str)
           .ToHandle(&msg)) {
    DCHECK(isolate->has_pending_exception());
    isolate->clear_pending_exception();
    return isolate->factory()->NewStringFromAsciiChecked("<error>");
  }

  return msg;
}

}  // namespace

// static
MaybeHandle<Object> ErrorUtils::MakeGenericError(
    Isolate* isolate, Handle<JSFunction> constructor, int template_index,
    Handle<Object> arg0, Handle<Object> arg1, Handle<Object> arg2,
    FrameSkipMode mode) {
  if (FLAG_clear_exceptions_on_js_entry) {
    // This function used to be implemented in JavaScript, and JSEntryStub
    // clears
    // any pending exceptions - so whenever we'd call this from C++, pending
    // exceptions would be cleared. Preserve this behavior.
    isolate->clear_pending_exception();
  }

  DCHECK(mode != SKIP_UNTIL_SEEN);

  Handle<Object> no_caller;
  Handle<String> msg = FormatMessage(isolate, template_index, arg0, arg1, arg2);
  return ErrorUtils::Construct(isolate, constructor, constructor, msg, mode,
                               no_caller, false);
}

#define SET_CALLSITE_PROPERTY(target, key, value)                        \
  RETURN_ON_EXCEPTION(                                                   \
      isolate, JSObject::SetOwnPropertyIgnoreAttributes(                 \
                   target, isolate->factory()->key(), value, DONT_ENUM), \
      Object)

MaybeHandle<Object> CallSiteUtils::Construct(Isolate* isolate,
                                             Handle<Object> receiver,
                                             Handle<Object> fun,
                                             Handle<Object> pos,
                                             Handle<Object> strict_mode) {
  // Create the JS object.

  Handle<JSFunction> target =
      handle(isolate->native_context()->callsite_function(), isolate);

  Handle<JSObject> obj;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, obj, JSObject::New(target, target),
                             Object);

  // For wasm frames, receiver is the wasm object and fun is the function index
  // instead of an actual function.
  const bool is_wasm_object =
      receiver->IsJSObject() && wasm::IsWasmObject(JSObject::cast(*receiver));
  if (!fun->IsJSFunction() && !is_wasm_object) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kCallSiteExpectsFunction,
                                 Object::TypeOf(isolate, receiver),
                                 Object::TypeOf(isolate, fun)),
                    Object);
  }

  if (is_wasm_object) {
    DCHECK(fun->IsSmi());
    DCHECK(wasm::GetNumberOfFunctions(JSObject::cast(*receiver)) >
           Smi::cast(*fun)->value());

    SET_CALLSITE_PROPERTY(obj, call_site_wasm_obj_symbol, receiver);
    SET_CALLSITE_PROPERTY(obj, call_site_wasm_func_index_symbol, fun);
  } else {
    DCHECK(fun->IsJSFunction());
    SET_CALLSITE_PROPERTY(obj, call_site_receiver_symbol, receiver);
    SET_CALLSITE_PROPERTY(obj, call_site_function_symbol, fun);
  }

  DCHECK(pos->IsSmi());
  SET_CALLSITE_PROPERTY(obj, call_site_position_symbol, pos);
  SET_CALLSITE_PROPERTY(
      obj, call_site_strict_symbol,
      isolate->factory()->ToBoolean(strict_mode->BooleanValue()));

  return obj;
}

#undef SET_CALLSITE_PROPERTY

namespace {

bool IsNonEmptyString(Handle<Object> object) {
  return (object->IsString() && String::cast(*object)->length() > 0);
}

MaybeHandle<JSObject> AppendWasmToString(Isolate* isolate,
                                         Handle<JSObject> recv,
                                         CallSite* call_site,
                                         IncrementalStringBuilder* builder) {
  Handle<Object> name = call_site->GetFunctionName();
  if (name->IsNull(isolate)) {
    builder->AppendCString("<WASM UNNAMED>");
  } else {
    DCHECK(name->IsString());
    builder->AppendString(Handle<String>::cast(name));
  }

  builder->AppendCString(" (<WASM>[");

  Handle<String> ix = isolate->factory()->NumberToString(
      handle(Smi::FromInt(call_site->wasm_func_index()), isolate));
  builder->AppendString(ix);

  builder->AppendCString("]+");

  Handle<Object> pos;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, pos, JSObject::GetProperty(
                        recv, isolate->factory()->call_site_position_symbol()),
      JSObject);
  DCHECK(pos->IsNumber());
  builder->AppendString(isolate->factory()->NumberToString(pos));
  builder->AppendCString(")");

  return recv;
}

MaybeHandle<JSObject> AppendFileLocation(Isolate* isolate,
                                         Handle<JSObject> recv,
                                         CallSite* call_site,
                                         IncrementalStringBuilder* builder) {
  if (call_site->IsNative()) {
    builder->AppendCString("native");
    return recv;
  }

  Handle<Object> file_name = call_site->GetScriptNameOrSourceUrl();
  if (!file_name->IsString() && call_site->IsEval()) {
    Handle<Object> eval_origin = call_site->GetEvalOrigin();
    DCHECK(eval_origin->IsString());
    builder->AppendString(Handle<String>::cast(eval_origin));
    builder->AppendCString(", ");  // Expecting source position to follow.
  }

  if (IsNonEmptyString(file_name)) {
    builder->AppendString(Handle<String>::cast(file_name));
  } else {
    // Source code does not originate from a file and is not native, but we
    // can still get the source position inside the source string, e.g. in
    // an eval string.
    builder->AppendCString("<anonymous>");
  }

  int line_number = call_site->GetLineNumber();
  if (line_number != -1) {
    builder->AppendCharacter(':');
    Handle<String> line_string = isolate->factory()->NumberToString(
        handle(Smi::FromInt(line_number), isolate), isolate);
    builder->AppendString(line_string);

    int column_number = call_site->GetColumnNumber();
    if (column_number != -1) {
      builder->AppendCharacter(':');
      Handle<String> column_string = isolate->factory()->NumberToString(
          handle(Smi::FromInt(column_number), isolate), isolate);
      builder->AppendString(column_string);
    }
  }

  return recv;
}

int StringIndexOf(Isolate* isolate, Handle<String> subject,
                  Handle<String> pattern) {
  if (pattern->length() > subject->length()) return -1;
  return String::IndexOf(isolate, subject, pattern, 0);
}

// Returns true iff
// 1. the subject ends with '.' + pattern, or
// 2. subject == pattern.
bool StringEndsWithMethodName(Isolate* isolate, Handle<String> subject,
                              Handle<String> pattern) {
  if (String::Equals(subject, pattern)) return true;

  FlatStringReader subject_reader(isolate, String::Flatten(subject));
  FlatStringReader pattern_reader(isolate, String::Flatten(pattern));

  int pattern_index = pattern_reader.length() - 1;
  int subject_index = subject_reader.length() - 1;
  for (int i = 0; i <= pattern_reader.length(); i++) {  // Iterate over len + 1.
    if (subject_index < 0) {
      return false;
    }

    const uc32 subject_char = subject_reader.Get(subject_index);
    if (i == pattern_reader.length()) {
      if (subject_char != '.') return false;
    } else if (subject_char != pattern_reader.Get(pattern_index)) {
      return false;
    }

    pattern_index--;
    subject_index--;
  }

  return true;
}

MaybeHandle<JSObject> AppendMethodCall(Isolate* isolate, Handle<JSObject> recv,
                                       CallSite* call_site,
                                       IncrementalStringBuilder* builder) {
  Handle<Object> type_name = call_site->GetTypeName();
  Handle<Object> method_name = call_site->GetMethodName();
  Handle<Object> function_name = call_site->GetFunctionName();

  if (IsNonEmptyString(function_name)) {
    Handle<String> function_string = Handle<String>::cast(function_name);
    if (IsNonEmptyString(type_name)) {
      Handle<String> type_string = Handle<String>::cast(type_name);
      bool starts_with_type_name =
          (StringIndexOf(isolate, function_string, type_string) == 0);
      if (!starts_with_type_name) {
        builder->AppendString(type_string);
        builder->AppendCharacter('.');
      }
    }
    builder->AppendString(function_string);

    if (IsNonEmptyString(method_name)) {
      Handle<String> method_string = Handle<String>::cast(method_name);
      if (!StringEndsWithMethodName(isolate, function_string, method_string)) {
        builder->AppendCString(" [as ");
        builder->AppendString(method_string);
        builder->AppendCharacter(']');
      }
    }
  } else {
    builder->AppendString(Handle<String>::cast(type_name));
    builder->AppendCharacter('.');
    if (IsNonEmptyString(method_name)) {
      builder->AppendString(Handle<String>::cast(method_name));
    } else {
      builder->AppendCString("<anonymous>");
    }
  }

  return recv;
}

}  // namespace

MaybeHandle<String> CallSiteUtils::ToString(Isolate* isolate,
                                            Handle<Object> receiver) {
  if (!receiver->IsJSObject()) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                     isolate->factory()->NewStringFromAsciiChecked("toString"),
                     receiver),
        String);
  }
  Handle<JSObject> recv = Handle<JSObject>::cast(receiver);

  if (!JSReceiver::HasOwnProperty(
           recv, isolate->factory()->call_site_position_symbol())
           .FromMaybe(false)) {
    THROW_NEW_ERROR(
        isolate,
        NewTypeError(MessageTemplate::kCallSiteMethod,
                     isolate->factory()->NewStringFromAsciiChecked("toString")),
        String);
  }

  IncrementalStringBuilder builder(isolate);

  CallSite call_site(isolate, recv);
  if (call_site.IsWasm()) {
    RETURN_ON_EXCEPTION(isolate,
                        AppendWasmToString(isolate, recv, &call_site, &builder),
                        String);
    RETURN_RESULT(isolate, builder.Finish(), String);
  }

  DCHECK(!call_site.IsWasm());
  Handle<Object> function_name = call_site.GetFunctionName();

  const bool is_toplevel = call_site.IsToplevel();
  const bool is_constructor = call_site.IsConstructor();
  const bool is_method_call = !(is_toplevel || is_constructor);

  if (is_method_call) {
    RETURN_ON_EXCEPTION(
        isolate, AppendMethodCall(isolate, recv, &call_site, &builder), String);
  } else if (is_constructor) {
    builder.AppendCString("new ");
    if (IsNonEmptyString(function_name)) {
      builder.AppendString(Handle<String>::cast(function_name));
    } else {
      builder.AppendCString("<anonymous>");
    }
  } else if (IsNonEmptyString(function_name)) {
    builder.AppendString(Handle<String>::cast(function_name));
  } else {
    RETURN_ON_EXCEPTION(isolate,
                        AppendFileLocation(isolate, recv, &call_site, &builder),
                        String);
    RETURN_RESULT(isolate, builder.Finish(), String);
  }

  builder.AppendCString(" (");
  RETURN_ON_EXCEPTION(
      isolate, AppendFileLocation(isolate, recv, &call_site, &builder), String);
  builder.AppendCString(")");

  RETURN_RESULT(isolate, builder.Finish(), String);
}

}  // namespace internal
}  // namespace v8
