// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/debug/debug-interface.h"
#include "src/frames-inl.h"
#include "src/property-descriptor.h"
#include "src/utils.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-objects.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/value-helper.h"
#include "test/cctest/wasm/wasm-run-utils.h"
#include "test/common/wasm/test-signatures.h"

using namespace v8::internal;
using namespace v8::internal::wasm;
namespace debug = v8::debug;

namespace {

void CheckLocations(
    WasmCompiledModule *compiled_module, debug::Location start,
    debug::Location end,
    std::initializer_list<debug::Location> expected_locations_init) {
  std::vector<debug::BreakLocation> locations;
  bool success =
      compiled_module->GetPossibleBreakpoints(start, end, &locations);
  CHECK(success);

  printf("got %d locations: ", static_cast<int>(locations.size()));
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    printf("%s<%d,%d>", i == 0 ? "" : ", ", locations[i].GetLineNumber(),
           locations[i].GetColumnNumber());
  }
  printf("\n");

  std::vector<debug::Location> expected_locations(expected_locations_init);
  CHECK_EQ(expected_locations.size(), locations.size());
  for (size_t i = 0, e = locations.size(); i != e; ++i) {
    CHECK_EQ(expected_locations[i].GetLineNumber(),
             locations[i].GetLineNumber());
    CHECK_EQ(expected_locations[i].GetColumnNumber(),
             locations[i].GetColumnNumber());
  }
}
void CheckLocationsFail(WasmCompiledModule *compiled_module,
                        debug::Location start, debug::Location end) {
  std::vector<debug::BreakLocation> locations;
  bool success =
      compiled_module->GetPossibleBreakpoints(start, end, &locations);
  CHECK(!success);
}

class BreakHandler : public debug::DebugDelegate {
 public:
  enum Action {
    Continue = StepAction::LastStepAction + 1,
    StepNext = StepAction::StepNext,
    StepIn = StepAction::StepIn,
    StepOut = StepAction::StepOut
  };
  struct BreakPoint {
    int position;
    Action action;
    BreakPoint(int position, Action action)
        : position(position), action(action) {}
  };

  explicit BreakHandler(Isolate* isolate,
                        std::initializer_list<BreakPoint> expected_breaks)
      : isolate_(isolate), expected_breaks_(expected_breaks) {
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_), this);
  }
  ~BreakHandler() {
    // Check that all expected breakpoints have been hit.
    CHECK_EQ(count_, expected_breaks_.size());
    v8::debug::SetDebugDelegate(reinterpret_cast<v8::Isolate*>(isolate_),
                                nullptr);
  }

  int count() const { return count_; }

 private:
  Isolate* isolate_;
  int count_ = 0;
  std::vector<BreakPoint> expected_breaks_;

  void BreakProgramRequested(v8::Local<v8::Context> paused_context,
                             v8::Local<v8::Object> exec_state,
                             v8::Local<v8::Value> break_points_hit) override {
    printf("Break #%d\n", count_);
    CHECK_GT(expected_breaks_.size(), count_);

    // Check the current position.
    StackTraceFrameIterator frame_it(isolate_);
    auto summ = FrameSummary::GetTop(frame_it.frame()).AsWasmInterpreted();
    CHECK_EQ(expected_breaks_[count_].position, summ.byte_offset());

    Action next_action = expected_breaks_[count_].action;
    switch (next_action) {
      case Continue:
        break;
      case StepNext:
      case StepIn:
      case StepOut:
        isolate_->debug()->PrepareStep(static_cast<StepAction>(next_action));
        break;
      default:
        UNREACHABLE();
    }
    ++count_;
  }
};

Handle<JSObject> MakeFakeBreakpoint(Isolate* isolate, int position) {
  Handle<JSObject> obj =
      isolate->factory()->NewJSObject(isolate->object_function());
  // Generate an "isTriggered" method that always returns true.
  // This can/must be refactored once we remove remaining JS parts from the
  // debugger (bug 5530).
  Handle<String> source = isolate->factory()->NewStringFromStaticChars("true");
  Handle<Context> context(isolate->context(), isolate);
  Handle<JSFunction> triggered_fun =
      Compiler::GetFunctionFromString(context, source, NO_PARSE_RESTRICTION,
                                      kNoSourcePosition)
          .ToHandleChecked();
  PropertyDescriptor desc;
  desc.set_value(triggered_fun);
  Handle<String> name =
      isolate->factory()->InternalizeUtf8String(CStrVector("isTriggered"));
  CHECK(
      JSObject::DefineOwnProperty(isolate, obj, name, &desc, Object::DONT_THROW)
          .FromMaybe(false));
  return obj;
}

void SetBreakpoint(WasmRunnerBase& runner, int function_index, int byte_offset,
                   int expected_set_byte_offset = -1) {
  int func_offset =
      runner.module().module->functions[function_index].code_start_offset;
  int code_offset = func_offset + byte_offset;
  if (expected_set_byte_offset == -1) expected_set_byte_offset = byte_offset;
  Handle<WasmInstanceObject> instance = runner.module().instance_object();
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module());
  Handle<JSObject> fake_breakpoint_object =
      MakeFakeBreakpoint(runner.main_isolate(), code_offset);
  CHECK(WasmCompiledModule::SetBreakPoint(compiled_module, &code_offset,
                                          fake_breakpoint_object));
  int set_byte_offset = code_offset - func_offset;
  CHECK_EQ(expected_set_byte_offset, set_byte_offset);
  // Also set breakpoint on the debug info of the instance directly, since the
  // instance chain is not setup properly in tests.
  Handle<WasmDebugInfo> debug_info =
      WasmInstanceObject::GetOrCreateDebugInfo(instance);
  WasmDebugInfo::SetBreakpoint(debug_info, function_index, set_byte_offset);
}

}  // namespace

TEST(WasmCollectPossibleBreakpoints) {
  WasmRunner<int> runner(kExecuteCompiled);

  BUILD(runner, WASM_NOP, WASM_I32_ADD(WASM_ZERO, WASM_ONE));

  Handle<WasmInstanceObject> instance = runner.module().instance_object();
  std::vector<debug::Location> locations;
  // Check all locations for function 0.
  CheckLocations(instance->compiled_module(), {0, 0}, {1, 0},
                 {{0, 1}, {0, 2}, {0, 4}, {0, 6}, {0, 7}});
  // Check a range ending at an instruction.
  CheckLocations(instance->compiled_module(), {0, 2}, {0, 4}, {{0, 2}});
  // Check a range ending one behind an instruction.
  CheckLocations(instance->compiled_module(), {0, 2}, {0, 5}, {{0, 2}, {0, 4}});
  // Check a range starting at an instruction.
  CheckLocations(instance->compiled_module(), {0, 7}, {0, 8}, {{0, 7}});
  // Check from an instruction to beginning of next function.
  CheckLocations(instance->compiled_module(), {0, 7}, {1, 0}, {{0, 7}});
  // Check from end of one function (no valid instruction position) to beginning
  // of next function. Must be empty, but not fail.
  CheckLocations(instance->compiled_module(), {0, 8}, {1, 0}, {});
  // Check from one after the end of the function. Must fail.
  CheckLocationsFail(instance->compiled_module(), {0, 9}, {1, 0});
}

TEST(WasmSimpleBreak) {
  WasmRunner<int> runner(kExecuteCompiled);
  Isolate* isolate = runner.main_isolate();

  BUILD(runner, WASM_NOP, WASM_I32_ADD(WASM_I32V_1(11), WASM_I32V_1(3)));

  Handle<JSFunction> main_fun_wrapper =
      runner.module().WrapCode(runner.function_index());
  SetBreakpoint(runner, runner.function_index(), 4, 4);

  BreakHandler count_breaks(isolate, {{4, BreakHandler::Continue}});

  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK(!retval.is_null());
  int result;
  CHECK(retval.ToHandleChecked()->ToInt32(&result));
  CHECK_EQ(14, result);
}

TEST(WasmSimpleStepping) {
  WasmRunner<int> runner(kExecuteCompiled);
  BUILD(runner, WASM_I32_ADD(WASM_I32V_1(11), WASM_I32V_1(3)));

  Isolate* isolate = runner.main_isolate();
  Handle<JSFunction> main_fun_wrapper =
      runner.module().WrapCode(runner.function_index());

  // Set breakpoint at the first I32Const.
  SetBreakpoint(runner, runner.function_index(), 1, 1);

  BreakHandler count_breaks(isolate,
                            {
                                {1, BreakHandler::StepNext},  // I32Const
                                {3, BreakHandler::StepNext},  // I32Const
                                {5, BreakHandler::Continue}   // I32Add
                            });

  Handle<Object> global(isolate->context()->global_object(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr);
  CHECK(!retval.is_null());
  int result;
  CHECK(retval.ToHandleChecked()->ToInt32(&result));
  CHECK_EQ(14, result);
}

TEST(WasmStepInAndOut) {
  WasmRunner<int, int> runner(kExecuteCompiled);
  WasmFunctionCompiler& f2 = runner.NewFunction<void>();
  f2.AllocateLocal(ValueType::kWord32);

  // Call f2 via indirect call, because a direct call requires f2 to exist when
  // we compile main, but we need to compile main first so that the order of
  // functions in the code section matches the function indexes.

  // return arg0
  BUILD(runner, WASM_RETURN1(WASM_GET_LOCAL(0)));
  // for (int i = 0; i < 10; ++i) { f2(i); }
  BUILD(f2, WASM_LOOP(
                WASM_BR_IF(0, WASM_BINOP(kExprI32GeU, WASM_GET_LOCAL(0),
                                         WASM_I32V_1(10))),
                WASM_SET_LOCAL(
                    0, WASM_BINOP(kExprI32Sub, WASM_GET_LOCAL(0), WASM_ONE)),
                WASM_CALL_FUNCTION(runner.function_index(), WASM_GET_LOCAL(0)),
                WASM_DROP, WASM_BR(1)));

  Isolate* isolate = runner.main_isolate();
  Handle<JSFunction> main_fun_wrapper =
      runner.module().WrapCode(f2.function_index());

  // Set first breakpoint on the GetLocal (offset 19) before the Call.
  SetBreakpoint(runner, f2.function_index(), 19, 19);

  BreakHandler count_breaks(isolate,
                            {
                                {19, BreakHandler::StepIn},   // GetLocal
                                {21, BreakHandler::StepIn},   // Call
                                {1, BreakHandler::StepOut},   // in f2
                                {23, BreakHandler::Continue}  // After Call
                            });

  Handle<Object> global(isolate->context()->global_object(), isolate);
  CHECK(!Execution::Call(isolate, main_fun_wrapper, global, 0, nullptr)
             .is_null());
}
