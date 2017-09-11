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

#include "wasm/WasmCompile.h"

#include "mozilla/Maybe.h"
#include "mozilla/Unused.h"

#include "jsprf.h"

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmBinaryIterator.h"
#include "wasm/WasmGenerator.h"
#include "wasm/WasmSignalHandlers.h"
#include "wasm/WasmValidate.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

static bool
DecodeFunctionBody(Decoder& d, ModuleGenerator& mg, uint32_t funcIndex)
{
    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return d.fail("expected number of function body bytes");

    const size_t offsetInModule = d.currentOffset();

    // Skip over the function body; it will be validated by the compilation thread.
    const uint8_t* bodyBegin;
    if (!d.readBytes(bodySize, &bodyBegin))
        return d.fail("function body length too big");

    return mg.compileFuncDef(funcIndex, offsetInModule, bodyBegin, bodyBegin + bodySize);
}

static bool
DecodeCodeSection(Decoder& d, ModuleGenerator& mg, ModuleEnvironment* env)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(SectionId::Code, env, &sectionStart, &sectionSize, "code"))
        return false;

    if (!mg.startFuncDefs())
        return false;

    if (sectionStart == Decoder::NotStarted) {
        if (env->numFuncDefs() != 0)
            return d.fail("expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return d.fail("expected function body count");

    if (numFuncDefs != env->numFuncDefs())
        return d.fail("function body count does not match function signature count");

    for (uint32_t funcDefIndex = 0; funcDefIndex < numFuncDefs; funcDefIndex++) {
        if (!DecodeFunctionBody(d, mg, env->numFuncImports() + funcDefIndex))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize, "code"))
        return false;

    return mg.finishFuncDefs();
}

bool
CompileArgs::initFromContext(JSContext* cx, ScriptedCaller&& scriptedCaller)
{
    baselineEnabled = cx->options().wasmBaseline();

    // For sanity's sake, just use Ion if both compilers are disabled.
    ionEnabled = cx->options().wasmIon() || !cx->options().wasmBaseline();

    // Debug information such as source view or debug traps will require
    // additional memory and permanently stay in baseline code, so we try to
    // only enable it when a developer actually cares: when the debugger tab
    // is open.
    debugEnabled = cx->compartment()->debuggerObservesAsmJS();

    this->scriptedCaller = Move(scriptedCaller);
    return assumptions.initBuildIdFromContext(cx);
}

static bool
BackgroundWorkPossible()
{
    return CanUseExtraThreads() && HelperThreadState().cpuCount > 1;
}

SharedModule
wasm::CompileInitialTier(const ShareableBytes& bytecode, const CompileArgs& args, UniqueChars* error)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    bool baselineEnabled = BaselineCanCompile() && args.baselineEnabled;
    bool debugEnabled = BaselineCanCompile() && args.debugEnabled;
    bool ionEnabled = args.ionEnabled || !baselineEnabled;

    CompileMode mode;
    Tier tier;
    DebugEnabled debug;
    if (BackgroundWorkPossible() && baselineEnabled && ionEnabled && !debugEnabled) {
        mode = CompileMode::Tier1;
        tier = Tier::Baseline;
        debug = DebugEnabled::False;
    } else {
        mode = CompileMode::Once;
        tier = debugEnabled || !ionEnabled ? Tier::Baseline : Tier::Ion;
        debug = debugEnabled ? DebugEnabled::True : DebugEnabled::False;
    }

    ModuleEnvironment env(mode, tier, debug);

    Decoder d(bytecode.bytes, error);
    if (!DecodeModuleEnvironment(d, &env))
        return nullptr;

    ModuleGenerator mg(args, &env, nullptr, error);
    if (!mg.init())
        return nullptr;

    if (!DecodeCodeSection(d, mg, &env))
        return nullptr;

    if (!DecodeModuleTail(d, &env))
        return nullptr;

    return mg.finishModule(bytecode);
}

bool
wasm::CompileTier2(Module& module, const CompileArgs& args, Atomic<bool>* cancelled)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    UniqueChars error;
    Decoder d(module.bytecode().bytes, &error);

    ModuleEnvironment env(CompileMode::Tier2, Tier::Ion, DebugEnabled::False);
    if (!DecodeModuleEnvironment(d, &env))
        return false;

    ModuleGenerator mg(args, &env, cancelled, &error);
    if (!mg.init())
        return false;

    if (!DecodeCodeSection(d, mg, &env))
        return false;

    if (!DecodeModuleTail(d, &env))
        return false;

    return mg.finishTier2(module);
}
