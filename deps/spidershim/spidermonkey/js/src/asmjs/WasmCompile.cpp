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

#include "asmjs/WasmCompile.h"

#include "mozilla/CheckedInt.h"

#include "jsprf.h"

#include "asmjs/WasmBinaryIterator.h"
#include "asmjs/WasmGenerator.h"
#include "asmjs/WasmSignalHandlers.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::IsNaN;

static bool
Fail(Decoder& d, const char* str)
{
    uint32_t offset = d.currentOffset();
    d.fail(UniqueChars(JS_smprintf("compile error at offset %" PRIu32 ": %s", offset, str)));
    return false;
}

namespace {

struct ValidatingPolicy : ExprIterPolicy
{
    // Validation is what we're all about here.
    static const bool Validate = true;
};

typedef ExprIter<ValidatingPolicy> ValidatingExprIter;

class FunctionDecoder
{
    const ModuleGenerator& mg_;
    const ValTypeVector& locals_;
    ValidatingExprIter iter_;
    bool newFormat_;

  public:
    FunctionDecoder(const ModuleGenerator& mg, const ValTypeVector& locals, Decoder& d, bool newFormat)
      : mg_(mg), locals_(locals), iter_(d), newFormat_(newFormat)
    {}
    const ModuleGenerator& mg() const { return mg_; }
    ValidatingExprIter& iter() { return iter_; }
    const ValTypeVector& locals() const { return locals_; }
    bool newFormat() const { return newFormat_; }

    bool checkHasMemory() {
        if (!mg().usesMemory())
            return iter().fail("can't touch memory without memory");
        return true;
    }

    bool checkIsOldFormat() {
        if (newFormat_)
            return iter().fail("opcode no longer in new format");
        return true;
    }
};

} // end anonymous namespace

static bool
CheckValType(Decoder& d, ValType type)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
      case ValType::I64:
        return true;
      default:
        // Note: it's important not to remove this default since readValType()
        // can return ValType values for which there is no enumerator.
        break;
    }

    return Fail(d, "bad type");
}

static bool
DecodeCallArgs(FunctionDecoder& f, uint32_t arity, const Sig& sig)
{
    if (arity != sig.args().length())
        return f.iter().fail("call arity out of range");

    const ValTypeVector& args = sig.args();
    uint32_t numArgs = args.length();
    for (size_t i = 0; i < numArgs; ++i) {
        ValType argType = args[i];
        if (!f.iter().readCallArg(argType, numArgs, i, nullptr))
            return false;
    }

    return f.iter().readCallArgsEnd(numArgs);
}

static bool
DecodeCallReturn(FunctionDecoder& f, const Sig& sig)
{
    return f.iter().readCallReturn(sig.ret());
}

static bool
DecodeCall(FunctionDecoder& f)
{
    uint32_t calleeIndex;
    uint32_t arity;
    if (!f.iter().readCall(&calleeIndex, &arity))
        return false;

    const Sig* sig;
    if (f.newFormat()) {
        if (calleeIndex >= f.mg().numFuncs())
            return f.iter().fail("callee index out of range");

        sig = &f.mg().funcSig(calleeIndex);
    } else {
        if (calleeIndex >= f.mg().numFuncDefs())
            return f.iter().fail("callee index out of range");

        sig = &f.mg().funcDefSig(calleeIndex);
    }

    return DecodeCallArgs(f, arity, *sig) &&
           DecodeCallReturn(f, *sig);
}

static bool
DecodeCallIndirect(FunctionDecoder& f)
{
    if (!f.mg().numTables())
        return f.iter().fail("can't call_indirect without a table");

    uint32_t sigIndex;
    uint32_t arity;
    if (!f.iter().readCallIndirect(&sigIndex, &arity))
        return false;

    if (sigIndex >= f.mg().numSigs())
        return f.iter().fail("signature index out of range");

    const Sig& sig = f.mg().sig(sigIndex);
    if (!DecodeCallArgs(f, arity, sig))
        return false;

    if (!f.iter().readCallIndirectCallee(nullptr))
        return false;

    return DecodeCallReturn(f, sig);
}

static bool
DecodeCallImport(FunctionDecoder& f)
{
    uint32_t funcImportIndex;
    uint32_t arity;
    if (!f.iter().readCallImport(&funcImportIndex, &arity))
        return false;

    if (funcImportIndex >= f.mg().numFuncImports())
        return f.iter().fail("import index out of range");

    const Sig& sig = *f.mg().funcImport(funcImportIndex).sig;
    return DecodeCallArgs(f, arity, sig) &&
           DecodeCallReturn(f, sig);
}

static bool
DecodeBrTable(FunctionDecoder& f)
{
    uint32_t tableLength;
    ExprType type = ExprType::Limit;
    if (!f.iter().readBrTable(&tableLength, &type, nullptr, nullptr))
        return false;

    uint32_t depth;
    for (size_t i = 0, e = tableLength; i < e; ++i) {
        if (!f.iter().readBrTableEntry(type, &depth))
            return false;
    }

    // Read the default label.
    return f.iter().readBrTableEntry(type, &depth);
}

static bool
DecodeExpr(FunctionDecoder& f)
{
    Expr expr;
    if (!f.iter().readExpr(&expr))
        return false;

    switch (expr) {
      case Expr::Nop:
        return f.iter().readNullary(ExprType::Void);
      case Expr::Call:
        return DecodeCall(f);
      case Expr::CallIndirect:
        return DecodeCallIndirect(f);
      case Expr::CallImport:
        return f.checkIsOldFormat() &&
               DecodeCallImport(f);
      case Expr::I32Const:
        return f.iter().readI32Const(nullptr);
      case Expr::I64Const:
        return f.iter().readI64Const(nullptr);
      case Expr::F32Const:
        return f.iter().readF32Const(nullptr);
      case Expr::F64Const:
        return f.iter().readF64Const(nullptr);
      case Expr::GetLocal:
        return f.iter().readGetLocal(f.locals(), nullptr);
      case Expr::SetLocal:
        return f.iter().readSetLocal(f.locals(), nullptr, nullptr);
      case Expr::GetGlobal:
        return f.iter().readGetGlobal(f.mg().globals(), nullptr);
      case Expr::SetGlobal:
        return f.iter().readSetGlobal(f.mg().globals(), nullptr, nullptr);
      case Expr::Select:
        return f.iter().readSelect(nullptr, nullptr, nullptr, nullptr);
      case Expr::Block:
        return f.iter().readBlock();
      case Expr::Loop:
        return f.iter().readLoop();
      case Expr::If:
        return f.iter().readIf(nullptr);
      case Expr::Else:
        return f.iter().readElse(nullptr, nullptr);
      case Expr::End:
        return f.iter().readEnd(nullptr, nullptr, nullptr);
      case Expr::I32Clz:
      case Expr::I32Ctz:
      case Expr::I32Popcnt:
        return f.iter().readUnary(ValType::I32, nullptr);
      case Expr::I64Clz:
      case Expr::I64Ctz:
      case Expr::I64Popcnt:
        return f.iter().readUnary(ValType::I64, nullptr);
      case Expr::F32Abs:
      case Expr::F32Neg:
      case Expr::F32Ceil:
      case Expr::F32Floor:
      case Expr::F32Sqrt:
      case Expr::F32Trunc:
      case Expr::F32Nearest:
        return f.iter().readUnary(ValType::F32, nullptr);
      case Expr::F64Abs:
      case Expr::F64Neg:
      case Expr::F64Ceil:
      case Expr::F64Floor:
      case Expr::F64Sqrt:
      case Expr::F64Trunc:
      case Expr::F64Nearest:
        return f.iter().readUnary(ValType::F64, nullptr);
      case Expr::I32Add:
      case Expr::I32Sub:
      case Expr::I32Mul:
      case Expr::I32DivS:
      case Expr::I32DivU:
      case Expr::I32RemS:
      case Expr::I32RemU:
      case Expr::I32And:
      case Expr::I32Or:
      case Expr::I32Xor:
      case Expr::I32Shl:
      case Expr::I32ShrS:
      case Expr::I32ShrU:
      case Expr::I32Rotl:
      case Expr::I32Rotr:
        return f.iter().readBinary(ValType::I32, nullptr, nullptr);
      case Expr::I64Add:
      case Expr::I64Sub:
      case Expr::I64Mul:
      case Expr::I64DivS:
      case Expr::I64DivU:
      case Expr::I64RemS:
      case Expr::I64RemU:
      case Expr::I64And:
      case Expr::I64Or:
      case Expr::I64Xor:
      case Expr::I64Shl:
      case Expr::I64ShrS:
      case Expr::I64ShrU:
      case Expr::I64Rotl:
      case Expr::I64Rotr:
        return f.iter().readBinary(ValType::I64, nullptr, nullptr);
      case Expr::F32Add:
      case Expr::F32Sub:
      case Expr::F32Mul:
      case Expr::F32Div:
      case Expr::F32Min:
      case Expr::F32Max:
      case Expr::F32CopySign:
        return f.iter().readBinary(ValType::F32, nullptr, nullptr);
      case Expr::F64Add:
      case Expr::F64Sub:
      case Expr::F64Mul:
      case Expr::F64Div:
      case Expr::F64Min:
      case Expr::F64Max:
      case Expr::F64CopySign:
        return f.iter().readBinary(ValType::F64, nullptr, nullptr);
      case Expr::I32Eq:
      case Expr::I32Ne:
      case Expr::I32LtS:
      case Expr::I32LtU:
      case Expr::I32LeS:
      case Expr::I32LeU:
      case Expr::I32GtS:
      case Expr::I32GtU:
      case Expr::I32GeS:
      case Expr::I32GeU:
        return f.iter().readComparison(ValType::I32, nullptr, nullptr);
      case Expr::I64Eq:
      case Expr::I64Ne:
      case Expr::I64LtS:
      case Expr::I64LtU:
      case Expr::I64LeS:
      case Expr::I64LeU:
      case Expr::I64GtS:
      case Expr::I64GtU:
      case Expr::I64GeS:
      case Expr::I64GeU:
        return f.iter().readComparison(ValType::I64, nullptr, nullptr);
      case Expr::F32Eq:
      case Expr::F32Ne:
      case Expr::F32Lt:
      case Expr::F32Le:
      case Expr::F32Gt:
      case Expr::F32Ge:
        return f.iter().readComparison(ValType::F32, nullptr, nullptr);
      case Expr::F64Eq:
      case Expr::F64Ne:
      case Expr::F64Lt:
      case Expr::F64Le:
      case Expr::F64Gt:
      case Expr::F64Ge:
        return f.iter().readComparison(ValType::F64, nullptr, nullptr);
      case Expr::I32Eqz:
        return f.iter().readConversion(ValType::I32, ValType::I32, nullptr);
      case Expr::I64Eqz:
      case Expr::I32WrapI64:
        return f.iter().readConversion(ValType::I64, ValType::I32, nullptr);
      case Expr::I32TruncSF32:
      case Expr::I32TruncUF32:
      case Expr::I32ReinterpretF32:
        return f.iter().readConversion(ValType::F32, ValType::I32, nullptr);
      case Expr::I32TruncSF64:
      case Expr::I32TruncUF64:
        return f.iter().readConversion(ValType::F64, ValType::I32, nullptr);
      case Expr::I64ExtendSI32:
      case Expr::I64ExtendUI32:
        return f.iter().readConversion(ValType::I32, ValType::I64, nullptr);
      case Expr::I64TruncSF32:
      case Expr::I64TruncUF32:
        return f.iter().readConversion(ValType::F32, ValType::I64, nullptr);
      case Expr::I64TruncSF64:
      case Expr::I64TruncUF64:
      case Expr::I64ReinterpretF64:
        return f.iter().readConversion(ValType::F64, ValType::I64, nullptr);
      case Expr::F32ConvertSI32:
      case Expr::F32ConvertUI32:
      case Expr::F32ReinterpretI32:
        return f.iter().readConversion(ValType::I32, ValType::F32, nullptr);
      case Expr::F32ConvertSI64:
      case Expr::F32ConvertUI64:
        return f.iter().readConversion(ValType::I64, ValType::F32, nullptr);
      case Expr::F32DemoteF64:
        return f.iter().readConversion(ValType::F64, ValType::F32, nullptr);
      case Expr::F64ConvertSI32:
      case Expr::F64ConvertUI32:
        return f.iter().readConversion(ValType::I32, ValType::F64, nullptr);
      case Expr::F64ConvertSI64:
      case Expr::F64ConvertUI64:
      case Expr::F64ReinterpretI64:
        return f.iter().readConversion(ValType::I64, ValType::F64, nullptr);
      case Expr::F64PromoteF32:
        return f.iter().readConversion(ValType::F32, ValType::F64, nullptr);
      case Expr::I32Load8S:
      case Expr::I32Load8U:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I32, 1, nullptr);
      case Expr::I32Load16S:
      case Expr::I32Load16U:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I32, 2, nullptr);
      case Expr::I32Load:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I32, 4, nullptr);
      case Expr::I64Load8S:
      case Expr::I64Load8U:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I64, 1, nullptr);
      case Expr::I64Load16S:
      case Expr::I64Load16U:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I64, 2, nullptr);
      case Expr::I64Load32S:
      case Expr::I64Load32U:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I64, 4, nullptr);
      case Expr::I64Load:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::I64, 8, nullptr);
      case Expr::F32Load:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::F32, 4, nullptr);
      case Expr::F64Load:
        return f.checkHasMemory() &&
               f.iter().readLoad(ValType::F64, 8, nullptr);
      case Expr::I32Store8:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I32, 1, nullptr, nullptr);
      case Expr::I32Store16:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I32, 2, nullptr, nullptr);
      case Expr::I32Store:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I32, 4, nullptr, nullptr);
      case Expr::I64Store8:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I64, 1, nullptr, nullptr);
      case Expr::I64Store16:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I64, 2, nullptr, nullptr);
      case Expr::I64Store32:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I64, 4, nullptr, nullptr);
      case Expr::I64Store:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::I64, 8, nullptr, nullptr);
      case Expr::F32Store:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::F32, 4, nullptr, nullptr);
      case Expr::F64Store:
        return f.checkHasMemory() &&
               f.iter().readStore(ValType::F64, 8, nullptr, nullptr);
      case Expr::GrowMemory:
        return f.checkHasMemory() &&
               f.iter().readUnary(ValType::I32, nullptr);
      case Expr::CurrentMemory:
        return f.checkHasMemory() &&
               f.iter().readNullary(ExprType::I32);
      case Expr::Br:
        return f.iter().readBr(nullptr, nullptr, nullptr);
      case Expr::BrIf:
        return f.iter().readBrIf(nullptr, nullptr, nullptr, nullptr);
      case Expr::BrTable:
        return DecodeBrTable(f);
      case Expr::Return:
        return f.iter().readReturn(nullptr);
      case Expr::Unreachable:
        return f.iter().readUnreachable();
      default:
        // Note: it's important not to remove this default since readExpr()
        // can return Expr values for which there is no enumerator.
        break;
    }

    return f.iter().unrecognizedOpcode(expr);
}

static bool
DecodePreamble(Decoder& d)
{
    uint32_t u32;
    if (!d.readFixedU32(&u32) || u32 != MagicNumber)
        return Fail(d, "failed to match magic number");

    if (!d.readFixedU32(&u32) || u32 != EncodingVersion)
        return Fail(d, "failed to match binary version");

    return true;
}

static bool
DecodeTypeSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(TypeSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSigs;
    if (!d.readVarU32(&numSigs))
        return Fail(d, "expected number of signatures");

    if (numSigs > MaxSigs)
        return Fail(d, "too many signatures");

    if (!init->sigs.resize(numSigs))
        return false;

    for (uint32_t sigIndex = 0; sigIndex < numSigs; sigIndex++) {
        uint32_t form;
        if (!d.readVarU32(&form) || form != uint32_t(TypeConstructor::Function))
            return Fail(d, "expected function form");

        uint32_t numArgs;
        if (!d.readVarU32(&numArgs))
            return Fail(d, "bad number of function args");

        if (numArgs > MaxArgsPerFunc)
            return Fail(d, "too many arguments in signature");

        ValTypeVector args;
        if (!args.resize(numArgs))
            return false;

        for (uint32_t i = 0; i < numArgs; i++) {
            if (!d.readValType(&args[i]))
                return Fail(d, "bad value type");

            if (!CheckValType(d, args[i]))
                return false;
        }

        uint32_t numRets;
        if (!d.readVarU32(&numRets))
            return Fail(d, "bad number of function returns");

        if (numRets > 1)
            return Fail(d, "too many returns in signature");

        ExprType result = ExprType::Void;

        if (numRets == 1) {
            ValType type;
            if (!d.readValType(&type))
                return Fail(d, "bad expression type");

            if (!CheckValType(d, type))
                return false;

            result = ToExprType(type);
        }

        init->sigs[sigIndex] = Sig(Move(args), result);
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "decls section byte size mismatch");

    return true;
}

static bool
DecodeSignatureIndex(Decoder& d, const ModuleGeneratorData& init, const SigWithId** sig)
{
    uint32_t sigIndex;
    if (!d.readVarU32(&sigIndex))
        return Fail(d, "expected signature index");

    if (sigIndex >= init.sigs.length())
        return Fail(d, "signature index out of range");

    *sig = &init.sigs[sigIndex];
    return true;
}

static bool
DecodeFunctionSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(FunctionSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numDefs;
    if (!d.readVarU32(&numDefs))
        return Fail(d, "expected number of function definitions");

    if (numDefs > MaxFuncs)
        return Fail(d, "too many functions");

    if (!init->funcDefSigs.resize(numDefs))
        return false;

    for (uint32_t i = 0; i < numDefs; i++) {
        if (!DecodeSignatureIndex(d, *init, &init->funcDefSigs[i]))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "decls section byte size mismatch");

    return true;
}

static UniqueChars
DecodeName(Decoder& d)
{
    uint32_t numBytes;
    if (!d.readVarU32(&numBytes))
        return nullptr;

    const uint8_t* bytes;
    if (!d.readBytes(numBytes, &bytes))
        return nullptr;

    UniqueChars name(js_pod_malloc<char>(numBytes + 1));
    if (!name)
        return nullptr;

    memcpy(name.get(), bytes, numBytes);
    name[numBytes] = '\0';

    return name;
}

static bool
DecodeResizable(Decoder& d, ResizableLimits* limits)
{
    uint32_t flags;
    if (!d.readVarU32(&flags))
        return Fail(d, "expected flags");

    if (flags & ~uint32_t(ResizableFlags::AllowedMask))
        return Fail(d, "unexpected bits set in flags");

    if (!(flags & uint32_t(ResizableFlags::Default)))
        return Fail(d, "currently, every memory/table must be declared default");

    if (!d.readVarU32(&limits->initial))
        return Fail(d, "expected initial length");

    if (flags & uint32_t(ResizableFlags::HasMaximum)) {
        uint32_t maximum;
        if (!d.readVarU32(&maximum))
            return Fail(d, "expected maximum length");

        if (limits->initial > maximum)
            return Fail(d, "maximum length less than initial length");

        limits->maximum.emplace(maximum);
    }

    return true;
}

static bool
DecodeResizableMemory(Decoder& d, ModuleGeneratorData* init)
{
    if (UsesMemory(init->memoryUsage))
        return Fail(d, "already have default memory");

    ResizableLimits limits;
    if (!DecodeResizable(d, &limits))
        return false;

    init->memoryUsage = MemoryUsage::Unshared;

    CheckedInt<uint32_t> initialBytes = limits.initial;
    initialBytes *= PageSize;
    if (!initialBytes.isValid() || initialBytes.value() > uint32_t(INT32_MAX))
        return Fail(d, "initial memory size too big");

    init->minMemoryLength = initialBytes.value();

    if (limits.maximum) {
        CheckedInt<uint32_t> maximumBytes = *limits.maximum;
        maximumBytes *= PageSize;
        if (!maximumBytes.isValid())
            return Fail(d, "maximum memory size too big");

        init->maxMemoryLength = Some(maximumBytes.value());
    }

    return true;
}

static bool
DecodeResizableTable(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t elementType;
    if (!d.readVarU32(&elementType))
        return Fail(d, "expected table element type");

    if (elementType != uint32_t(TypeConstructor::AnyFunc))
        return Fail(d, "expected 'anyfunc' element type");

    ResizableLimits limits;
    if (!DecodeResizable(d, &limits))
        return false;

    if (!init->tables.empty())
        return Fail(d, "already have default table");

    return init->tables.emplaceBack(TableKind::AnyFunction, limits);
}

static bool
DecodeGlobalType(Decoder& d, ValType* type, bool* isMutable)
{
    if (!d.readValType(type))
        return Fail(d, "bad global type");

    uint32_t flags;
    if (!d.readVarU32(&flags))
        return Fail(d, "expected flags");

    if (flags & ~uint32_t(GlobalFlags::AllowedMask))
        return Fail(d, "unexpected bits set in flags");

    *isMutable = flags & uint32_t(GlobalFlags::IsMutable);
    return true;
}

static bool
GlobalIsJSCompatible(Decoder& d, ValType type, bool isMutable)
{
    switch (type) {
      case ValType::I32:
      case ValType::F32:
      case ValType::F64:
        break;
      case ValType::I64:
        if (!JitOptions.wasmTestMode)
            return Fail(d, "can't import/export an Int64 global to JS");
        break;
      default:
        return Fail(d, "unexpected variable type in global import/export");
    }

    if (isMutable)
        return Fail(d, "can't import/export mutable globals in the MVP");

    return true;
}

static bool
DecodeImport(Decoder& d, bool newFormat, ModuleGeneratorData* init, ImportVector* imports)
{
    if (!newFormat) {
        const SigWithId* sig = nullptr;
        if (!DecodeSignatureIndex(d, *init, &sig))
            return false;

        if (!init->funcImports.emplaceBack(sig))
            return false;

        UniqueChars moduleName = DecodeName(d);
        if (!moduleName)
            return Fail(d, "expected valid import module name");

        UniqueChars funcName = DecodeName(d);
        if (!funcName)
            return Fail(d, "expected valid import func name");

        return imports->emplaceBack(Move(moduleName), Move(funcName), DefinitionKind::Function);
    }

    UniqueChars moduleName = DecodeName(d);
    if (!moduleName)
        return Fail(d, "expected valid import module name");

    UniqueChars funcName = DecodeName(d);
    if (!funcName)
        return Fail(d, "expected valid import func name");

    uint32_t importKind;
    if (!d.readVarU32(&importKind))
        return Fail(d, "failed to read import kind");

    switch (DefinitionKind(importKind)) {
      case DefinitionKind::Function: {
        const SigWithId* sig = nullptr;
        if (!DecodeSignatureIndex(d, *init, &sig))
            return false;
        if (!init->funcImports.emplaceBack(sig))
            return false;
        break;
      }
      case DefinitionKind::Table: {
        if (!DecodeResizableTable(d, init))
            return false;
        break;
      }
      case DefinitionKind::Memory: {
        if (!DecodeResizableMemory(d, init))
            return false;
        break;
      }
      case DefinitionKind::Global: {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;
        if (!GlobalIsJSCompatible(d, type, isMutable))
            return false;
        if (!init->globals.append(GlobalDesc(type, isMutable, init->globals.length())))
            return false;
        break;
      }
      default:
        return Fail(d, "unsupported import kind");
    }

    return imports->emplaceBack(Move(moduleName), Move(funcName), DefinitionKind(importKind));
}

static bool
DecodeImportSection(Decoder& d, bool newFormat, ModuleGeneratorData* init, ImportVector* imports)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(ImportSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numImports;
    if (!d.readVarU32(&numImports))
        return Fail(d, "failed to read number of imports");

    if (numImports > MaxImports)
        return Fail(d, "too many imports");

    for (uint32_t i = 0; i < numImports; i++) {
        if (!DecodeImport(d, newFormat, init, imports))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "import section byte size mismatch");

    return true;
}

static bool
DecodeTableSection(Decoder& d, bool newFormat, ModuleGeneratorData* init, Uint32Vector* oldElems)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(TableSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (newFormat) {
        if (!DecodeResizableTable(d, init))
            return false;
    } else {
        ResizableLimits limits;
        if (!d.readVarU32(&limits.initial))
            return Fail(d, "expected number of table elems");

        if (limits.initial > MaxTableElems)
            return Fail(d, "too many table elements");

        limits.maximum = Some(limits.initial);

        if (!oldElems->resize(limits.initial))
            return false;

        for (uint32_t i = 0; i < limits.initial; i++) {
            uint32_t funcDefIndex;
            if (!d.readVarU32(&funcDefIndex))
                return Fail(d, "expected table element");

            if (funcDefIndex >= init->funcDefSigs.length())
                return Fail(d, "table element out of range");

            (*oldElems)[i] = init->funcImports.length() + funcDefIndex;
        }

        MOZ_ASSERT(init->tables.empty());
        if (!init->tables.emplaceBack(TableKind::AnyFunction, limits))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "table section byte size mismatch");

    return true;
}

static bool
DecodeMemorySection(Decoder& d, bool newFormat, ModuleGeneratorData* init, bool* exported)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(MemorySectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (newFormat) {
        if (!DecodeResizableMemory(d, init))
            return false;
    } else {
        uint32_t initialSizePages;
        if (!d.readVarU32(&initialSizePages))
            return Fail(d, "expected initial memory size");

        CheckedInt<uint32_t> initialSize = initialSizePages;
        initialSize *= PageSize;
        if (!initialSize.isValid())
            return Fail(d, "initial memory size too big");

        // ArrayBufferObject can't currently allocate more than INT32_MAX bytes.
        if (initialSize.value() > uint32_t(INT32_MAX))
            return false;

        uint32_t maxSizePages;
        if (!d.readVarU32(&maxSizePages))
            return Fail(d, "expected initial memory size");

        CheckedInt<uint32_t> maxSize = maxSizePages;
        maxSize *= PageSize;
        if (!maxSize.isValid())
            return Fail(d, "maximum memory size too big");

        if (maxSize.value() < initialSize.value())
            return Fail(d, "maximum memory size less than initial memory size");

        uint8_t u8;
        if (!d.readFixedU8(&u8))
            return Fail(d, "expected exported byte");

        *exported = u8;
        MOZ_ASSERT(init->memoryUsage == MemoryUsage::None);
        init->memoryUsage = MemoryUsage::Unshared;
        init->minMemoryLength = initialSize.value();
        init->maxMemoryLength = Some(maxSize.value());
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "memory section byte size mismatch");

    return true;
}

static bool
DecodeInitializerExpression(Decoder& d, const GlobalDescVector& globals, ValType expected,
                            InitExpr* init)
{
    Expr expr;
    if (!d.readExpr(&expr))
        return Fail(d, "failed to read initializer type");

    switch (expr) {
      case Expr::I32Const: {
        int32_t i32;
        if (!d.readVarS32(&i32))
            return Fail(d, "failed to read initializer i32 expression");
        *init = InitExpr(Val(uint32_t(i32)));
        break;
      }
      case Expr::I64Const: {
        int64_t i64;
        if (!d.readVarS64(&i64))
            return Fail(d, "failed to read initializer i64 expression");
        *init = InitExpr(Val(uint64_t(i64)));
        break;
      }
      case Expr::F32Const: {
        float f32;
        if (!d.readFixedF32(&f32))
            return Fail(d, "failed to read initializer f32 expression");
        *init = InitExpr(Val(f32));
        break;
      }
      case Expr::F64Const: {
        double f64;
        if (!d.readFixedF64(&f64))
            return Fail(d, "failed to read initializer f64 expression");
        *init = InitExpr(Val(f64));
        break;
      }
      case Expr::GetGlobal: {
        uint32_t i;
        if (!d.readVarU32(&i))
            return Fail(d, "failed to read get_global index in initializer expression");
        if (i >= globals.length())
            return Fail(d, "global index out of range in initializer expression");
        if (!globals[i].isImport() || globals[i].isMutable())
            return Fail(d, "initializer expression must reference a global immutable import");
        *init = InitExpr(i, globals[i].type());
        break;
      }
      default: {
        return Fail(d, "unexpected initializer expression");
      }
    }

    if (expected != init->type())
        return Fail(d, "type mismatch: initializer type and expected type don't match");

    Expr end;
    if (!d.readExpr(&end) || end != Expr::End)
        return Fail(d, "failed to read end of initializer expression");

    return true;
}

static bool
DecodeGlobalSection(Decoder& d, ModuleGeneratorData* init)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(GlobalSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numGlobals;
    if (!d.readVarU32(&numGlobals))
        return Fail(d, "expected number of globals");

    if (numGlobals > MaxGlobals)
        return Fail(d, "too many globals");

    for (uint32_t i = 0; i < numGlobals; i++) {
        ValType type;
        bool isMutable;
        if (!DecodeGlobalType(d, &type, &isMutable))
            return false;

        InitExpr initializer;
        if (!DecodeInitializerExpression(d, init->globals, type, &initializer))
            return false;

        if (!init->globals.append(GlobalDesc(initializer, isMutable)))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "globals section byte size mismatch");

    return true;
}

typedef HashSet<const char*, CStringHasher, SystemAllocPolicy> CStringSet;

static UniqueChars
DecodeExportName(Decoder& d, CStringSet* dupSet)
{
    UniqueChars exportName = DecodeName(d);
    if (!exportName) {
        Fail(d, "expected valid export name");
        return nullptr;
    }

    CStringSet::AddPtr p = dupSet->lookupForAdd(exportName.get());
    if (p) {
        Fail(d, "duplicate export");
        return nullptr;
    }

    if (!dupSet->add(p, exportName.get()))
        return nullptr;

    return Move(exportName);
}

static bool
DecodeExport(Decoder& d, bool newFormat, ModuleGenerator& mg, CStringSet* dupSet)
{
    if (!newFormat) {
        uint32_t funcDefIndex;
        if (!d.readVarU32(&funcDefIndex))
            return Fail(d, "expected export internal index");

        if (funcDefIndex >= mg.numFuncDefs())
            return Fail(d, "exported function index out of bounds");

        UniqueChars fieldName = DecodeExportName(d, dupSet);
        if (!fieldName)
            return false;

        return mg.addFuncDefExport(Move(fieldName), mg.numFuncImports() + funcDefIndex);
    }

    UniqueChars fieldName = DecodeExportName(d, dupSet);
    if (!fieldName)
        return false;

    uint32_t exportKind;
    if (!d.readVarU32(&exportKind))
        return Fail(d, "failed to read export kind");

    switch (DefinitionKind(exportKind)) {
      case DefinitionKind::Function: {
        uint32_t funcIndex;
        if (!d.readVarU32(&funcIndex))
            return Fail(d, "expected export internal index");

        if (funcIndex >= mg.numFuncs())
            return Fail(d, "exported function index out of bounds");

        return mg.addFuncDefExport(Move(fieldName), funcIndex);
      }
      case DefinitionKind::Table: {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return Fail(d, "expected table index");

        if (tableIndex >= mg.tables().length())
            return Fail(d, "exported table index out of bounds");

        return mg.addTableExport(Move(fieldName));
      }
      case DefinitionKind::Memory: {
        uint32_t memoryIndex;
        if (!d.readVarU32(&memoryIndex))
            return Fail(d, "expected memory index");

        if (memoryIndex > 0 || !mg.usesMemory())
            return Fail(d, "exported memory index out of bounds");

        return mg.addMemoryExport(Move(fieldName));
      }
      case DefinitionKind::Global: {
        uint32_t globalIndex;
        if (!d.readVarU32(&globalIndex))
            return Fail(d, "expected global index");

        if (globalIndex >= mg.globals().length())
            return Fail(d, "exported global index out of bounds");

        const GlobalDesc& global = mg.globals()[globalIndex];
        if (!GlobalIsJSCompatible(d, global.type(), global.isMutable()))
            return false;

        return mg.addGlobalExport(Move(fieldName), globalIndex);
      }
      default:
        return Fail(d, "unexpected export kind");
    }

    MOZ_CRASH("unreachable");
}

static bool
DecodeExportSection(Decoder& d, bool newFormat, bool memoryExported, ModuleGenerator& mg)
{
    if (!newFormat && memoryExported) {
        UniqueChars fieldName = DuplicateString("memory");
        if (!fieldName || !mg.addMemoryExport(Move(fieldName)))
            return false;
    }

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(ExportSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    CStringSet dupSet;
    if (!dupSet.init())
        return false;

    uint32_t numExports;
    if (!d.readVarU32(&numExports))
        return Fail(d, "failed to read number of exports");

    if (numExports > MaxExports)
        return Fail(d, "too many exports");

    for (uint32_t i = 0; i < numExports; i++) {
        if (!DecodeExport(d, newFormat, mg, &dupSet))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "export section byte size mismatch");

    return true;
}

static bool
DecodeFunctionBody(Decoder& d, bool newFormat, ModuleGenerator& mg, uint32_t funcDefIndex)
{
    uint32_t bodySize;
    if (!d.readVarU32(&bodySize))
        return Fail(d, "expected number of function body bytes");

    if (d.bytesRemain() < bodySize)
        return Fail(d, "function body length too big");

    const uint8_t* bodyBegin = d.currentPosition();
    const uint8_t* bodyEnd = bodyBegin + bodySize;

    FunctionGenerator fg;
    if (!mg.startFuncDef(d.currentOffset(), &fg))
        return false;

    ValTypeVector locals;
    const Sig& sig = mg.funcDefSig(funcDefIndex);
    if (!locals.appendAll(sig.args()))
        return false;

    if (!DecodeLocalEntries(d, &locals))
        return Fail(d, "failed decoding local entries");

    for (ValType type : locals) {
        if (!CheckValType(d, type))
            return false;
    }

    FunctionDecoder f(mg, locals, d, newFormat);

    if (!f.iter().readFunctionStart())
        return false;

    while (d.currentPosition() < bodyEnd) {
        if (!DecodeExpr(f))
            return false;
    }

    if (!f.iter().readFunctionEnd(sig.ret(), nullptr))
        return false;

    if (d.currentPosition() != bodyEnd)
        return Fail(d, "function body length mismatch");

    if (!fg.bytes().resize(bodySize))
        return false;

    memcpy(fg.bytes().begin(), bodyBegin, bodySize);

    return mg.finishFuncDef(funcDefIndex, &fg);
}

static bool
DecodeStartSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(StartSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t funcIndex;
    if (!d.readVarU32(&funcIndex))
        return Fail(d, "failed to read start func index");

    if (funcIndex >= mg.numFuncs())
        return Fail(d, "unknown start function");

    const Sig& sig = mg.funcSig(funcIndex);
    if (sig.ret() != ExprType::Void)
        return Fail(d, "start function must not return anything");

    if (sig.args().length())
        return Fail(d, "start function must be nullary");

    if (!mg.setStartFunction(funcIndex))
        return false;

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "data section byte size mismatch");

    return true;
}

static bool
DecodeCodeSection(Decoder& d, bool newFormat, ModuleGenerator& mg)
{
    if (!mg.startFuncDefs())
        return false;

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(CodeSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");

    if (sectionStart == Decoder::NotStarted) {
        if (mg.numFuncDefs() != 0)
            return Fail(d, "expected function bodies");

        return mg.finishFuncDefs();
    }

    uint32_t numFuncDefs;
    if (!d.readVarU32(&numFuncDefs))
        return Fail(d, "expected function body count");

    if (numFuncDefs != mg.numFuncDefs())
        return Fail(d, "function body count does not match function signature count");

    for (uint32_t i = 0; i < numFuncDefs; i++) {
        if (!DecodeFunctionBody(d, newFormat, mg, i))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "function section byte size mismatch");

    return mg.finishFuncDefs();
}

static bool
DecodeElemSection(Decoder& d, bool newFormat, Uint32Vector&& oldElems, ModuleGenerator& mg)
{
    if (!newFormat) {
        if (mg.tables().empty()) {
            MOZ_ASSERT(oldElems.empty());
            return true;
        }

        return mg.addElemSegment(InitExpr(Val(uint32_t(0))), Move(oldElems));
    }

    uint32_t sectionStart, sectionSize;
    if (!d.startSection(ElemSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return Fail(d, "failed to read number of elem segments");

    if (numSegments > MaxElemSegments)
        return Fail(d, "too many elem segments");

    for (uint32_t i = 0; i < numSegments; i++) {
        uint32_t tableIndex;
        if (!d.readVarU32(&tableIndex))
            return Fail(d, "expected table index");

        MOZ_ASSERT(mg.tables().length() <= 1);
        if (tableIndex >= mg.tables().length())
            return Fail(d, "table index out of range");

        InitExpr offset;
        if (!DecodeInitializerExpression(d, mg.globals(), ValType::I32, &offset))
            return false;

        uint32_t numElems;
        if (!d.readVarU32(&numElems))
            return Fail(d, "expected segment size");

        uint32_t tableLength = mg.tables()[tableIndex].limits.initial;
        if (offset.isVal()) {
            uint32_t off = offset.val().i32();
            if (off > tableLength || tableLength - off < numElems)
                return Fail(d, "element segment does not fit");
        }

        Uint32Vector elemFuncIndices;
        if (!elemFuncIndices.resize(numElems))
            return false;

        for (uint32_t i = 0; i < numElems; i++) {
            if (!d.readVarU32(&elemFuncIndices[i]))
                return Fail(d, "failed to read element function index");
            if (elemFuncIndices[i] >= mg.numFuncs())
                return Fail(d, "table element out of range");
        }

        if (!mg.addElemSegment(offset, Move(elemFuncIndices)))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "data section byte size mismatch");

    return true;
}

static bool
DecodeDataSection(Decoder& d, bool newFormat, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(DataSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!mg.usesMemory())
        return Fail(d, "data section requires a memory section");

    uint32_t numSegments;
    if (!d.readVarU32(&numSegments))
        return Fail(d, "failed to read number of data segments");

    if (numSegments > MaxDataSegments)
        return Fail(d, "too many data segments");

    uint32_t max = mg.minMemoryLength();
    for (uint32_t i = 0; i < numSegments; i++) {
        DataSegment seg;
        if (newFormat) {
            uint32_t linearMemoryIndex;
            if (!d.readVarU32(&linearMemoryIndex))
                return Fail(d, "expected linear memory index");

            if (linearMemoryIndex != 0)
                return Fail(d, "linear memory index must currently be 0");

            if (!DecodeInitializerExpression(d, mg.globals(), ValType::I32, &seg.offset))
                return false;
        } else {
            uint32_t offset;
            if (!d.readVarU32(&offset))
                return Fail(d, "expected segment destination offset");

            seg.offset = InitExpr(Val(offset));
        }

        if (!d.readVarU32(&seg.length))
            return Fail(d, "expected segment size");

        if (seg.offset.isVal()) {
            uint32_t off = seg.offset.val().i32();
            if (off > max || max - off < seg.length)
                return Fail(d, "data segment does not fit");
        }

        seg.bytecodeOffset = d.currentOffset();

        if (!d.readBytes(seg.length))
            return Fail(d, "data segment shorter than declared");

        if (!mg.addDataSegment(seg))
            return false;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "data section byte size mismatch");

    return true;
}

static bool
MaybeDecodeNameSectionBody(Decoder& d, ModuleGenerator& mg)
{
    uint32_t numFuncNames;
    if (!d.readVarU32(&numFuncNames))
        return false;

    if (numFuncNames > MaxFuncs)
        return Fail(d, "too many function names");

    NameInBytecodeVector funcNames;
    if (!funcNames.resize(numFuncNames))
        return false;

    for (uint32_t i = 0; i < numFuncNames; i++) {
        uint32_t numBytes;
        if (!d.readVarU32(&numBytes))
            return false;

        NameInBytecode name;
        name.offset = d.currentOffset();
        name.length = numBytes;
        funcNames[i] = name;

        if (!d.readBytes(numBytes))
            return false;

        // Skip local names for a function.
        uint32_t numLocals;
        if (!d.readVarU32(&numLocals))
            return false;
        for (uint32_t j = 0; j < numLocals; j++) {
            uint32_t numBytes;
            if (!d.readVarU32(&numBytes))
                return false;
            if (!d.readBytes(numBytes))
                return false;
        }
    }

    mg.setFuncNames(Move(funcNames));
    return true;
}

static bool
DecodeNameSection(Decoder& d, ModuleGenerator& mg)
{
    uint32_t sectionStart, sectionSize;
    if (!d.startSection(NameSectionId, &sectionStart, &sectionSize))
        return Fail(d, "failed to start section");
    if (sectionStart == Decoder::NotStarted)
        return true;

    if (!MaybeDecodeNameSectionBody(d, mg)) {
        // This section does not cause validation for the whole module to fail and
        // is instead treated as if the section was absent.
        d.ignoreSection(sectionStart, sectionSize);
        d.clearError();
        return true;
    }

    if (!d.finishSection(sectionStart, sectionSize))
        return Fail(d, "names section byte size mismatch");

    return true;
}

static bool
DecodeUnknownSections(Decoder& d)
{
    while (!d.done()) {
        if (!d.skipSection())
            return Fail(d, "failed to skip unknown section at end");
    }

    return true;
}

bool
CompileArgs::initFromContext(ExclusiveContext* cx, ScriptedCaller&& scriptedCaller)
{
    alwaysBaseline = cx->options().wasmAlwaysBaseline();
    this->scriptedCaller = Move(scriptedCaller);
    return assumptions.initBuildIdFromContext(cx);
}

SharedModule
wasm::Compile(const ShareableBytes& bytecode, const CompileArgs& args, UniqueChars* error)
{
    MOZ_RELEASE_ASSERT(wasm::HaveSignalHandlers());

    bool newFormat = args.assumptions.newFormat;

    auto init = js::MakeUnique<ModuleGeneratorData>();
    if (!init)
        return nullptr;

    Decoder d(bytecode.begin(), bytecode.end(), error);

    if (!DecodePreamble(d))
        return nullptr;

    if (!DecodeTypeSection(d, init.get()))
        return nullptr;

    ImportVector imports;
    if (!DecodeImportSection(d, newFormat, init.get(), &imports))
        return nullptr;

    if (!DecodeFunctionSection(d, init.get()))
        return nullptr;

    Uint32Vector oldElems;
    if (!DecodeTableSection(d, newFormat, init.get(), &oldElems))
        return nullptr;

    bool memoryExported = false;
    if (!DecodeMemorySection(d, newFormat, init.get(), &memoryExported))
        return nullptr;

    if (!DecodeGlobalSection(d, init.get()))
        return nullptr;

    ModuleGenerator mg(Move(imports));
    if (!mg.init(Move(init), args))
        return nullptr;

    if (!DecodeExportSection(d, newFormat, memoryExported, mg))
        return nullptr;

    if (!DecodeStartSection(d, mg))
        return nullptr;

    if (!DecodeElemSection(d, newFormat, Move(oldElems), mg))
        return nullptr;

    if (!DecodeCodeSection(d, newFormat, mg))
        return nullptr;

    if (!DecodeDataSection(d, newFormat, mg))
        return nullptr;

    if (!DecodeNameSection(d, mg))
        return nullptr;

    if (!DecodeUnknownSections(d))
        return nullptr;

    MOZ_ASSERT(!*error, "unreported error in decoding");

    return mg.finish(bytecode);
}
