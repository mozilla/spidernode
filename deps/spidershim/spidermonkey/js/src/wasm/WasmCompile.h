/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_compile_h
#define wasm_compile_h

#include "wasm/WasmModule.h"

namespace js {
namespace wasm {

// Describes the JS scripted caller of a request to compile a wasm module.

struct ScriptedCaller
{
    UniqueChars filename;
    unsigned line;
    unsigned column;
};

// Describes all the parameters that control wasm compilation.

struct CompileArgs : ShareableBase<CompileArgs>
{
    Assumptions assumptions;
    ScriptedCaller scriptedCaller;
    bool baselineEnabled;
    bool debugEnabled;
    bool ionEnabled;

    CompileArgs(Assumptions&& assumptions, ScriptedCaller&& scriptedCaller)
      : assumptions(Move(assumptions)),
        scriptedCaller(Move(scriptedCaller)),
        baselineEnabled(false),
        debugEnabled(false),
        ionEnabled(false)
    {}

    // If CompileArgs is constructed without arguments, initFromContext() must
    // be called to complete initialization.
    CompileArgs() = default;
    bool initFromContext(JSContext* cx, ScriptedCaller&& scriptedCaller);
};

typedef RefPtr<CompileArgs> MutableCompileArgs;
typedef RefPtr<const CompileArgs> SharedCompileArgs;

// Compile the given WebAssembly bytecode with the given arguments into a
// wasm::Module. On success, the Module is returned. On failure, the returned
// SharedModule pointer is null and either:
//  - *error points to a string description of the error
//  - *error is null and the caller should report out-of-memory.

SharedModule
CompileInitialTier(const ShareableBytes& bytecode, const CompileArgs& args, UniqueChars* error);

// Attempt to compile the second tier of the given wasm::Module, returning whether
// tier-2 compilation succeeded and Module::finishTier2 was called.

bool
CompileTier2(Module& module, const CompileArgs& args, Atomic<bool>* cancelled);

}  // namespace wasm
}  // namespace js

#endif // namespace wasm_compile_h
