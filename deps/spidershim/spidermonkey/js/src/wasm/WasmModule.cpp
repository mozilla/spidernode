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

#include "wasm/WasmModule.h"

#include "jsnspr.h"

#include "jit/JitOptions.h"
#include "threading/LockGuard.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmInstance.h"
#include "wasm/WasmJS.h"
#include "wasm/WasmSerialize.h"

#include "jsatominlines.h"

#include "vm/ArrayBufferObject-inl.h"
#include "vm/Debugger-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::IsNaN;

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
// On MIPS, CodeLabels are instruction immediates so InternalLinks only
// patch instruction immediates.
LinkDataTier::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == InstructionImmediate);
}

bool
LinkDataTier::InternalLink::isRawPointerPatch()
{
    return false;
}
#else
// On the rest, CodeLabels are raw pointers so InternalLinks only patch
// raw pointers.
LinkDataTier::InternalLink::InternalLink(Kind kind)
{
    MOZ_ASSERT(kind == CodeLabel || kind == RawPointer);
}

bool
LinkDataTier::InternalLink::isRawPointerPatch()
{
    return true;
}
#endif

size_t
LinkDataTier::SymbolicLinkArray::serializedSize() const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += SerializedPodVectorSize(offsets);
    return size;
}

uint8_t*
LinkDataTier::SymbolicLinkArray::serialize(uint8_t* cursor) const
{
    for (const Uint32Vector& offsets : *this)
        cursor = SerializePodVector(cursor, offsets);
    return cursor;
}

const uint8_t*
LinkDataTier::SymbolicLinkArray::deserialize(const uint8_t* cursor)
{
    for (Uint32Vector& offsets : *this) {
        cursor = DeserializePodVector(cursor, &offsets);
        if (!cursor)
            return nullptr;
    }
    return cursor;
}

size_t
LinkDataTier::SymbolicLinkArray::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t size = 0;
    for (const Uint32Vector& offsets : *this)
        size += offsets.sizeOfExcludingThis(mallocSizeOf);
    return size;
}

size_t
LinkDataTier::serializedSize() const
{
    return sizeof(pod()) +
           SerializedPodVectorSize(internalLinks) +
           symbolicLinks.serializedSize();
}

uint8_t*
LinkDataTier::serialize(uint8_t* cursor) const
{
    MOZ_ASSERT(tier == Tier::Serialized);

    cursor = WriteBytes(cursor, &pod(), sizeof(pod()));
    cursor = SerializePodVector(cursor, internalLinks);
    cursor = symbolicLinks.serialize(cursor);
    return cursor;
}

const uint8_t*
LinkDataTier::deserialize(const uint8_t* cursor)
{
    (cursor = ReadBytes(cursor, &pod(), sizeof(pod()))) &&
    (cursor = DeserializePodVector(cursor, &internalLinks)) &&
    (cursor = symbolicLinks.deserialize(cursor));
    return cursor;
}

size_t
LinkDataTier::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    return internalLinks.sizeOfExcludingThis(mallocSizeOf) +
           symbolicLinks.sizeOfExcludingThis(mallocSizeOf);
}

void
LinkData::setTier2(UniqueLinkDataTier linkData) const
{
    MOZ_RELEASE_ASSERT(linkData->tier == Tier::Ion && linkData1_->tier != Tier::Ion);
    MOZ_RELEASE_ASSERT(!linkData2_.get());
    linkData2_ = Move(linkData);
}

Tiers
LinkData::tiers() const
{
    if (hasTier2())
        return Tiers(linkData1_->tier, linkData2_->tier);
    return Tiers(linkData1_->tier);
}

const LinkDataTier&
LinkData::linkData(Tier tier) const
{
    switch (tier) {
      case Tier::Baseline:
        if (linkData1_->tier == Tier::Baseline)
            return *linkData1_;
        MOZ_CRASH("No linkData at this tier");
      case Tier::Ion:
        if (linkData1_->tier == Tier::Ion)
            return *linkData1_;
        if (hasTier2())
            return *linkData2_;
        MOZ_CRASH("No linkData at this tier");
      default:
        MOZ_CRASH();
    }
}

LinkDataTier&
LinkData::linkData(Tier tier)
{
    switch (tier) {
      case Tier::Baseline:
        if (linkData1_->tier == Tier::Baseline)
            return *linkData1_;
        MOZ_CRASH("No linkData at this tier");
      case Tier::Ion:
        if (linkData1_->tier == Tier::Ion)
            return *linkData1_;
        if (hasTier2())
            return *linkData2_;
        MOZ_CRASH("No linkData at this tier");
      default:
        MOZ_CRASH();
    }
}

bool
LinkData::initTier1(Tier tier, const Metadata& metadata)
{
    MOZ_ASSERT(!linkData1_);
    metadata_ = &metadata;
    linkData1_ = js::MakeUnique<LinkDataTier>(tier);
    return linkData1_ != nullptr;
}

size_t
LinkData::serializedSize() const
{
    return linkData(Tier::Serialized).serializedSize();
}

uint8_t*
LinkData::serialize(uint8_t* cursor) const
{
    cursor = linkData(Tier::Serialized).serialize(cursor);
    return cursor;
}

const uint8_t*
LinkData::deserialize(const uint8_t* cursor)
{
    (cursor = linkData(Tier::Serialized).deserialize(cursor));
    return cursor;
}

size_t
LinkData::sizeOfExcludingThis(MallocSizeOf mallocSizeOf) const
{
    size_t sum = 0;
    for (auto t : tiers())
        sum += linkData(t).sizeOfExcludingThis(mallocSizeOf);
    return sum;
}

class Module::Tier2GeneratorTaskImpl : public Tier2GeneratorTask
{
    SharedModule            module_;
    SharedCompileArgs       compileArgs_;
    Atomic<bool>            cancelled_;
    bool                    finished_;

  public:
    Tier2GeneratorTaskImpl(Module& module, const CompileArgs& compileArgs)
      : module_(&module),
        compileArgs_(&compileArgs),
        cancelled_(false),
        finished_(false)
    {}

    ~Tier2GeneratorTaskImpl() override {
        if (!finished_)
            module_->unblockOnTier2GeneratorFinished(CompileMode::Once);
    }

    void cancel() override {
        cancelled_ = true;
    }

    void execute() override {
        MOZ_ASSERT(!finished_);
        finished_ = CompileTier2(*module_, *compileArgs_, &cancelled_);
    }
};

void
Module::startTier2(const CompileArgs& args)
{
    MOZ_ASSERT(mode_ == CompileMode::Once);

    // If a Module initiates tier-2 compilation, we must ensure that eventually
    // unblockOnTier2GeneratorFinished() is called. Since we must ensure
    // Tier2GeneratorTaskImpl objects are destroyed *anyway*, we use
    // ~Tier2GeneratorTaskImpl() to call unblockOnTier2GeneratorFinished() if it
    // hasn't been already.

    UniqueTier2GeneratorTask task(js_new<Tier2GeneratorTaskImpl>(*this, args));
    if (!task)
        return;

    {
        LockGuard<Mutex> l(tier2Lock_);
        mode_ = CompileMode::Tier1;
    }

    StartOffThreadWasmTier2Generator(Move(task));
}

void
Module::finishTier2(UniqueLinkDataTier linkData2, UniqueMetadataTier metadata2,
                    UniqueConstCodeSegment code2, UniqueModuleEnvironment env2)
{
    // Install the data in the data structures. They will not be visible yet.

    metadata().setTier2(Move(metadata2));
    linkData().setTier2(Move(linkData2));
    code().setTier2(Move(code2));
    for (uint32_t i = 0; i < elemSegments_.length(); i++)
        elemSegments_[i].setTier2(Move(env2->elemSegments[i].elemCodeRangeIndices(Tier::Ion)));

    // Now that all the code and metadata is valid, make tier 2 code visible and
    // unblock anyone waiting on it.

    metadata().commitTier2();
    unblockOnTier2GeneratorFinished(CompileMode::Tier2);

    // And we update the jump vector.

    void** jumpTable = code().jumpTable();
    uint8_t* base = code().segment(Tier::Ion).base();

    for (auto cr : metadata(Tier::Ion).codeRanges) {
        if (!cr.isFunction())
            continue;

        // This is a racy write that we just want to be visible, atomically,
        // eventually.  All hardware we care about will do this right.  But
        // we depend on the compiler not splitting the store.

        jumpTable[cr.funcIndex()] = base + cr.funcTierEntry();
    }
}

/* virtual */ size_t
Module::bytecodeSerializedSize() const
{
    return bytecode_->bytes.length();
}

/* virtual */ void
Module::bytecodeSerialize(uint8_t* bytecodeBegin, size_t bytecodeSize) const
{
    MOZ_ASSERT(!!bytecodeBegin == !!bytecodeSize);

    // Bytecode deserialization is not guarded by Assumptions and thus must not
    // change incompatibly between builds. For simplicity, the format of the
    // bytecode file is a .wasm file which ensures backwards compatibility.

    const Bytes& bytes = bytecode_->bytes;
    uint8_t* bytecodeEnd = WriteBytes(bytecodeBegin, bytes.begin(), bytes.length());
    MOZ_RELEASE_ASSERT(bytecodeEnd == bytecodeBegin + bytecodeSize);
}

/* virtual */ size_t
Module::compiledSerializedSize() const
{
    // The compiled debug code must not be saved, set compiled size to 0,
    // so Module::assumptionsMatch will return false during assumptions
    // deserialization.
    if (metadata().debugEnabled)
        return 0;

    blockOnIonCompileFinished();
    if (!code_->hasTier(Tier::Serialized))
        return 0;

    return assumptions_.serializedSize() +
           linkData_.serializedSize() +
           SerializedVectorSize(imports_) +
           SerializedVectorSize(exports_) +
           SerializedPodVectorSize(dataSegments_) +
           SerializedVectorSize(elemSegments_) +
           code_->serializedSize();
}

/* virtual */ void
Module::compiledSerialize(uint8_t* compiledBegin, size_t compiledSize) const
{
    if (metadata().debugEnabled) {
        MOZ_RELEASE_ASSERT(compiledSize == 0);
        return;
    }

    blockOnIonCompileFinished();
    if (!code_->hasTier(Tier::Serialized)) {
        MOZ_RELEASE_ASSERT(compiledSize == 0);
        return;
    }

    uint8_t* cursor = compiledBegin;
    cursor = assumptions_.serialize(cursor);
    cursor = linkData_.serialize(cursor);
    cursor = SerializeVector(cursor, imports_);
    cursor = SerializeVector(cursor, exports_);
    cursor = SerializePodVector(cursor, dataSegments_);
    cursor = SerializeVector(cursor, elemSegments_);
    cursor = code_->serialize(cursor, linkData_);
    MOZ_RELEASE_ASSERT(cursor == compiledBegin + compiledSize);
}

void
Module::blockOnIonCompileFinished() const
{
    LockGuard<Mutex> l(tier2Lock_);
    while (mode_ == CompileMode::Tier1 && !metadata().hasTier2())
        tier2Cond_.wait(l);
}

void
Module::unblockOnTier2GeneratorFinished(CompileMode newMode) const
{
    LockGuard<Mutex> l(tier2Lock_);
    mode_ = newMode;
    tier2Cond_.notify_all();
}

/* static */ bool
Module::assumptionsMatch(const Assumptions& current, const uint8_t* compiledBegin, size_t remain)
{
    Assumptions cached;
    if (!cached.deserialize(compiledBegin, remain))
        return false;

    return current == cached;
}

/* static */ SharedModule
Module::deserialize(const uint8_t* bytecodeBegin, size_t bytecodeSize,
                    const uint8_t* compiledBegin, size_t compiledSize,
                    Metadata* maybeMetadata)
{
    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode || !bytecode->bytes.initLengthUninitialized(bytecodeSize))
        return nullptr;

    memcpy(bytecode->bytes.begin(), bytecodeBegin, bytecodeSize);

    Assumptions assumptions;
    const uint8_t* cursor = assumptions.deserialize(compiledBegin, compiledSize);
    if (!cursor)
        return nullptr;

    MutableMetadata metadata(maybeMetadata);
    if (!metadata) {
        auto tierMetadata = js::MakeUnique<MetadataTier>(Tier::Ion);
        if (!tierMetadata)
            return nullptr;

        metadata = js_new<Metadata>(Move(tierMetadata));
        if (!metadata)
            return nullptr;
    }

    LinkData linkData;
    if (!linkData.initTier1(Tier::Serialized, *metadata))
        return nullptr;

    cursor = linkData.deserialize(cursor);
    if (!cursor)
        return nullptr;

    ImportVector imports;
    cursor = DeserializeVector(cursor, &imports);
    if (!cursor)
        return nullptr;

    ExportVector exports;
    cursor = DeserializeVector(cursor, &exports);
    if (!cursor)
        return nullptr;

    DataSegmentVector dataSegments;
    cursor = DeserializePodVector(cursor, &dataSegments);
    if (!cursor)
        return nullptr;

    ElemSegmentVector elemSegments;
    cursor = DeserializeVector(cursor, &elemSegments);
    if (!cursor)
        return nullptr;

    MutableCode code = js_new<Code>();
    cursor = code->deserialize(cursor, bytecode, linkData, *metadata);
    if (!cursor)
        return nullptr;

    MOZ_RELEASE_ASSERT(cursor == compiledBegin + compiledSize);
    MOZ_RELEASE_ASSERT(!!maybeMetadata == code->metadata().isAsmJS());

    return js_new<Module>(Move(assumptions),
                          *code,
                          nullptr,            // Serialized code is never debuggable
                          Move(linkData),
                          Move(imports),
                          Move(exports),
                          Move(dataSegments),
                          Move(elemSegments),
                          *bytecode);
}

/* virtual */ JSObject*
Module::createObject(JSContext* cx)
{
    if (!GlobalObject::ensureConstructor(cx, cx->global(), JSProto_WebAssembly))
        return nullptr;

    RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmModule).toObject());
    return WasmModuleObject::create(cx, *this, proto);
}

struct MemUnmap
{
    uint32_t size;
    MemUnmap() : size(0) {}
    explicit MemUnmap(uint32_t size) : size(size) {}
    void operator()(uint8_t* p) { MOZ_ASSERT(size); PR_MemUnmap(p, size); }
};

typedef UniquePtr<uint8_t, MemUnmap> UniqueMapping;

static UniqueMapping
MapFile(PRFileDesc* file, PRFileInfo* info)
{
    if (PR_GetOpenFileInfo(file, info) != PR_SUCCESS)
        return nullptr;

    PRFileMap* map = PR_CreateFileMap(file, info->size, PR_PROT_READONLY);
    if (!map)
        return nullptr;

    // PRFileMap objects do not need to be kept alive after the memory has been
    // mapped, so unconditionally close the PRFileMap, regardless of whether
    // PR_MemMap succeeds.
    uint8_t* memory = (uint8_t*)PR_MemMap(map, 0, info->size);
    PR_CloseFileMap(map);
    return UniqueMapping(memory, MemUnmap(info->size));
}

bool
wasm::CompiledModuleAssumptionsMatch(PRFileDesc* compiled, JS::BuildIdCharVector&& buildId)
{
    PRFileInfo info;
    UniqueMapping mapping = MapFile(compiled, &info);
    if (!mapping)
        return false;

    Assumptions assumptions(Move(buildId));
    return Module::assumptionsMatch(assumptions, mapping.get(), info.size);
}

SharedModule
wasm::DeserializeModule(PRFileDesc* bytecodeFile, PRFileDesc* maybeCompiledFile,
                        JS::BuildIdCharVector&& buildId, UniqueChars filename,
                        unsigned line, unsigned column)
{
    PRFileInfo bytecodeInfo;
    UniqueMapping bytecodeMapping = MapFile(bytecodeFile, &bytecodeInfo);
    if (!bytecodeMapping)
        return nullptr;

    if (PRFileDesc* compiledFile = maybeCompiledFile) {
        PRFileInfo compiledInfo;
        UniqueMapping compiledMapping = MapFile(compiledFile, &compiledInfo);
        if (!compiledMapping)
            return nullptr;

        return Module::deserialize(bytecodeMapping.get(), bytecodeInfo.size,
                                   compiledMapping.get(), compiledInfo.size);
    }

    // Since the compiled file's assumptions don't match, we must recompile from
    // bytecode. The bytecode file format is simply that of a .wasm (see
    // Module::serialize).

    MutableBytes bytecode = js_new<ShareableBytes>();
    if (!bytecode || !bytecode->bytes.initLengthUninitialized(bytecodeInfo.size))
        return nullptr;

    memcpy(bytecode->bytes.begin(), bytecodeMapping.get(), bytecodeInfo.size);

    ScriptedCaller scriptedCaller;
    scriptedCaller.filename = Move(filename);
    scriptedCaller.line = line;
    scriptedCaller.column = column;

    SharedCompileArgs args = js_new<CompileArgs>(Assumptions(Move(buildId)), Move(scriptedCaller));
    if (!args)
        return nullptr;

    UniqueChars error;
    return CompileInitialTier(*bytecode, *args, &error);
}

/* virtual */ void
Module::addSizeOfMisc(MallocSizeOf mallocSizeOf,
                      Metadata::SeenSet* seenMetadata,
                      ShareableBytes::SeenSet* seenBytes,
                      Code::SeenSet* seenCode,
                      size_t* code,
                      size_t* data) const
{
    code_->addSizeOfMiscIfNotSeen(mallocSizeOf, seenMetadata, seenCode, code, data);
    *data += mallocSizeOf(this) +
             assumptions_.sizeOfExcludingThis(mallocSizeOf) +
             linkData_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(imports_, mallocSizeOf) +
             SizeOfVectorExcludingThis(exports_, mallocSizeOf) +
             dataSegments_.sizeOfExcludingThis(mallocSizeOf) +
             SizeOfVectorExcludingThis(elemSegments_, mallocSizeOf) +
             bytecode_->sizeOfIncludingThisIfNotSeen(mallocSizeOf, seenBytes);
    if (unlinkedCodeForDebugging_)
        *data += unlinkedCodeForDebugging_->sizeOfExcludingThis(mallocSizeOf);
}


// Extracting machine code as JS object. The result has the "code" property, as
// a Uint8Array, and the "segments" property as array objects. The objects
// contain offsets in the "code" array and basic information about a code
// segment/function body.
bool
Module::extractCode(JSContext* cx, Tier tier, MutableHandleValue vp) const
{
    RootedPlainObject result(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!result)
        return false;

    if (tier == Tier::Ion)
        blockOnIonCompileFinished();

    if (!code_->hasTier(tier)) {
        vp.setNull();
        return true;
    }

    const CodeSegment& codeSegment = code_->segment(tier);
    RootedObject code(cx, JS_NewUint8Array(cx, codeSegment.length()));
    if (!code)
        return false;

    memcpy(code->as<TypedArrayObject>().viewDataUnshared(), codeSegment.base(), codeSegment.length());

    RootedValue value(cx, ObjectValue(*code));
    if (!JS_DefineProperty(cx, result, "code", value, JSPROP_ENUMERATE))
        return false;

    RootedObject segments(cx, NewDenseEmptyArray(cx));
    if (!segments)
        return false;

    for (const CodeRange& p : metadata(tier).codeRanges) {
        RootedObject segment(cx, NewObjectWithGivenProto<PlainObject>(cx, nullptr));
        if (!segment)
            return false;

        value.setNumber((uint32_t)p.begin());
        if (!JS_DefineProperty(cx, segment, "begin", value, JSPROP_ENUMERATE))
            return false;

        value.setNumber((uint32_t)p.end());
        if (!JS_DefineProperty(cx, segment, "end", value, JSPROP_ENUMERATE))
            return false;

        value.setNumber((uint32_t)p.kind());
        if (!JS_DefineProperty(cx, segment, "kind", value, JSPROP_ENUMERATE))
            return false;

        if (p.isFunction()) {
            value.setNumber((uint32_t)p.funcIndex());
            if (!JS_DefineProperty(cx, segment, "funcIndex", value, JSPROP_ENUMERATE))
                return false;

            value.setNumber((uint32_t)p.funcNormalEntry());
            if (!JS_DefineProperty(cx, segment, "funcBodyBegin", value, JSPROP_ENUMERATE))
                return false;

            value.setNumber((uint32_t)p.end());
            if (!JS_DefineProperty(cx, segment, "funcBodyEnd", value, JSPROP_ENUMERATE))
                return false;
        }

        if (!NewbornArrayPush(cx, segments, ObjectValue(*segment)))
            return false;
    }

    value.setObject(*segments);
    if (!JS_DefineProperty(cx, result, "segments", value, JSPROP_ENUMERATE))
        return false;

    vp.setObject(*result);
    return true;
}

static uint32_t
EvaluateInitExpr(const ValVector& globalImports, InitExpr initExpr)
{
    switch (initExpr.kind()) {
      case InitExpr::Kind::Constant:
        return initExpr.val().i32();
      case InitExpr::Kind::GetGlobal:
        return globalImports[initExpr.globalIndex()].i32();
    }

    MOZ_CRASH("bad initializer expression");
}

bool
Module::initSegments(JSContext* cx,
                     HandleWasmInstanceObject instanceObj,
                     Handle<FunctionVector> funcImports,
                     HandleWasmMemoryObject memoryObj,
                     const ValVector& globalImports) const
{
    Instance& instance = instanceObj->instance();
    const SharedTableVector& tables = instance.tables();

    Tier tier = code().bestTier();

    // Perform all error checks up front so that this function does not perform
    // partial initialization if an error is reported.

    for (const ElemSegment& seg : elemSegments_) {
        uint32_t numElems = seg.elemCodeRangeIndices(tier).length();

        uint32_t tableLength = tables[seg.tableIndex]->length();
        uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

        if (offset > tableLength || tableLength - offset < numElems) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                      "elem", "table");
            return false;
        }
    }

    if (memoryObj) {
        for (const DataSegment& seg : dataSegments_) {
            uint32_t memoryLength = memoryObj->buffer().byteLength();
            uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);

            if (offset > memoryLength || memoryLength - offset < seg.length) {
                JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_FIT,
                                          "data", "memory");
                return false;
            }
        }
    } else {
        MOZ_ASSERT(dataSegments_.empty());
    }

    // Now that initialization can't fail partway through, write data/elem
    // segments into memories/tables.

    for (const ElemSegment& seg : elemSegments_) {
        Table& table = *tables[seg.tableIndex];
        uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);
        const CodeRangeVector& codeRanges = metadata(tier).codeRanges;
        uint8_t* codeBase = instance.codeBase(tier);

        for (uint32_t i = 0; i < seg.elemCodeRangeIndices(tier).length(); i++) {
            uint32_t funcIndex = seg.elemFuncIndices[i];
            if (funcIndex < funcImports.length() && IsExportedWasmFunction(funcImports[funcIndex])) {
                MOZ_ASSERT(!metadata().isAsmJS());
                MOZ_ASSERT(!table.isTypedFunction());

                HandleFunction f = funcImports[funcIndex];
                WasmInstanceObject* exportInstanceObj = ExportedFunctionToInstanceObject(f);
                Instance& exportInstance = exportInstanceObj->instance();
                Tier exportTier = exportInstance.code().bestTier();
                const CodeRange& cr = exportInstanceObj->getExportedFunctionCodeRange(f, exportTier);
                table.set(offset + i, exportInstance.codeBase(exportTier) + cr.funcTableEntry(), exportInstance);
            } else {
                const CodeRange& cr = codeRanges[seg.elemCodeRangeIndices(tier)[i]];
                uint32_t entryOffset = table.isTypedFunction()
                                       ? cr.funcNormalEntry()
                                       : cr.funcTableEntry();
                table.set(offset + i, codeBase + entryOffset, instance);
            }
        }
    }

    if (memoryObj) {
        uint8_t* memoryBase = memoryObj->buffer().dataPointerEither().unwrap(/* memcpy */);

        for (const DataSegment& seg : dataSegments_) {
            MOZ_ASSERT(seg.bytecodeOffset <= bytecode_->length());
            MOZ_ASSERT(seg.length <= bytecode_->length() - seg.bytecodeOffset);
            uint32_t offset = EvaluateInitExpr(globalImports, seg.offset);
            memcpy(memoryBase + offset, bytecode_->begin() + seg.bytecodeOffset, seg.length);
        }
    }

    return true;
}

static const Import&
FindImportForFuncImport(const ImportVector& imports, uint32_t funcImportIndex)
{
    for (const Import& import : imports) {
        if (import.kind != DefinitionKind::Function)
            continue;
        if (funcImportIndex == 0)
            return import;
        funcImportIndex--;
    }
    MOZ_CRASH("ran out of imports");
}

bool
Module::instantiateFunctions(JSContext* cx, Handle<FunctionVector> funcImports) const
{
#ifdef DEBUG
    for (auto t : code().tiers())
        MOZ_ASSERT(funcImports.length() == metadata(t).funcImports.length());
#endif

    if (metadata().isAsmJS())
        return true;

    Tier tier = code().stableTier();

    for (size_t i = 0; i < metadata(tier).funcImports.length(); i++) {
        HandleFunction f = funcImports[i];
        if (!IsExportedFunction(f) || ExportedFunctionToInstance(f).isAsmJS())
            continue;

        uint32_t funcIndex = ExportedFunctionToFuncIndex(f);
        Instance& instance = ExportedFunctionToInstance(f);
        const FuncExport& funcExport = instance.metadata(tier).lookupFuncExport(funcIndex);

        if (funcExport.sig() != metadata(tier).funcImports[i].sig()) {
            const Import& import = FindImportForFuncImport(imports_, i);
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMPORT_SIG,
                                      import.module.get(), import.field.get());
            return false;
        }
    }

    return true;
}

static bool
CheckLimits(JSContext* cx, uint32_t declaredMin, const Maybe<uint32_t>& declaredMax, uint32_t actualLength,
            const Maybe<uint32_t>& actualMax, bool isAsmJS, const char* kind)
{
    if (isAsmJS) {
        MOZ_ASSERT(actualLength >= declaredMin);
        MOZ_ASSERT(!declaredMax);
        MOZ_ASSERT(actualLength == actualMax.value());
        return true;
    }

    if (actualLength < declaredMin || actualLength > declaredMax.valueOr(UINT32_MAX)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_SIZE, kind);
        return false;
    }

    if ((actualMax && declaredMax && *actualMax > *declaredMax) || (!actualMax && declaredMax)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_WASM_BAD_IMP_MAX, kind);
        return false;
    }

    return true;
}

// asm.js module instantiation supplies its own buffer, but for wasm, create and
// initialize the buffer if one is requested. Either way, the buffer is wrapped
// in a WebAssembly.Memory object which is what the Instance stores.
bool
Module::instantiateMemory(JSContext* cx, MutableHandleWasmMemoryObject memory) const
{
    if (!metadata().usesMemory()) {
        MOZ_ASSERT(!memory);
        MOZ_ASSERT(dataSegments_.empty());
        return true;
    }

    uint32_t declaredMin = metadata().minMemoryLength;
    Maybe<uint32_t> declaredMax = metadata().maxMemoryLength;

    if (memory) {
        ArrayBufferObjectMaybeShared& buffer = memory->buffer();
        MOZ_ASSERT_IF(metadata().isAsmJS(), buffer.isPreparedForAsmJS());
        MOZ_ASSERT_IF(!metadata().isAsmJS(), buffer.as<ArrayBufferObject>().isWasm());

        if (!CheckLimits(cx, declaredMin, declaredMax, buffer.byteLength(), buffer.wasmMaxSize(),
                         metadata().isAsmJS(), "Memory")) {
            return false;
        }
    } else {
        MOZ_ASSERT(!metadata().isAsmJS());
        MOZ_ASSERT(metadata().memoryUsage == MemoryUsage::Unshared);

        RootedArrayBufferObjectMaybeShared buffer(cx,
            ArrayBufferObject::createForWasm(cx, declaredMin, declaredMax));
        if (!buffer)
            return false;

        RootedObject proto(cx, &cx->global()->getPrototype(JSProto_WasmMemory).toObject());

        memory.set(WasmMemoryObject::create(cx, buffer, proto));
        if (!memory)
            return false;
    }

    return true;
}

bool
Module::instantiateTable(JSContext* cx, MutableHandleWasmTableObject tableObj,
                         SharedTableVector* tables) const
{
    if (tableObj) {
        MOZ_ASSERT(!metadata().isAsmJS());

        MOZ_ASSERT(metadata().tables.length() == 1);
        const TableDesc& td = metadata().tables[0];
        MOZ_ASSERT(td.external);

        Table& table = tableObj->table();
        if (!CheckLimits(cx, td.limits.initial, td.limits.maximum, table.length(), table.maximum(),
                         metadata().isAsmJS(), "Table")) {
            return false;
        }

        if (!tables->append(&table)) {
            ReportOutOfMemory(cx);
            return false;
        }
    } else {
        for (const TableDesc& td : metadata().tables) {
            SharedTable table;
            if (td.external) {
                MOZ_ASSERT(!tableObj);
                MOZ_ASSERT(td.kind == TableKind::AnyFunction);

                tableObj.set(WasmTableObject::create(cx, td.limits));
                if (!tableObj)
                    return false;

                table = &tableObj->table();
            } else {
                table = Table::create(cx, td, /* HandleWasmTableObject = */ nullptr);
                if (!table)
                    return false;
            }

            if (!tables->emplaceBack(table)) {
                ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    return true;
}

static bool
GetFunctionExport(JSContext* cx,
                  HandleWasmInstanceObject instanceObj,
                  Handle<FunctionVector> funcImports,
                  const Export& exp,
                  MutableHandleValue val)
{
    if (exp.funcIndex() < funcImports.length() &&
        IsExportedWasmFunction(funcImports[exp.funcIndex()]))
    {
        val.setObject(*funcImports[exp.funcIndex()]);
        return true;
    }

    RootedFunction fun(cx);
    if (!instanceObj->getExportedFunction(cx, instanceObj, exp.funcIndex(), &fun))
        return false;

    val.setObject(*fun);
    return true;
}

static bool
GetGlobalExport(JSContext* cx, const GlobalDescVector& globals, uint32_t globalIndex,
                const ValVector& globalImports, MutableHandleValue jsval)
{
    const GlobalDesc& global = globals[globalIndex];

    // Imports are located upfront in the globals array.
    Val val;
    switch (global.kind()) {
      case GlobalKind::Import:   val = globalImports[globalIndex]; break;
      case GlobalKind::Variable: MOZ_CRASH("mutable variables can't be exported");
      case GlobalKind::Constant: val = global.constantValue(); break;
    }

    switch (global.type()) {
      case ValType::I32: {
        jsval.set(Int32Value(val.i32()));
        return true;
      }
      case ValType::I64: {
        MOZ_ASSERT(JitOptions.wasmTestMode, "no int64 in asm.js/wasm");
        RootedObject obj(cx, CreateI64Object(cx, val.i64()));
        if (!obj)
            return false;
        jsval.set(ObjectValue(*obj));
        return true;
      }
      case ValType::F32: {
        float f = val.f32();
        if (JitOptions.wasmTestMode && IsNaN(f)) {
            RootedObject obj(cx, CreateCustomNaNObject(cx, &f));
            if (!obj)
                return false;
            jsval.set(ObjectValue(*obj));
            return true;
        }
        jsval.set(DoubleValue(JS::CanonicalizeNaN(double(f))));
        return true;
      }
      case ValType::F64: {
        double d = val.f64();
        if (JitOptions.wasmTestMode && IsNaN(d)) {
            RootedObject obj(cx, CreateCustomNaNObject(cx, &d));
            if (!obj)
                return false;
            jsval.set(ObjectValue(*obj));
            return true;
        }
        jsval.set(DoubleValue(JS::CanonicalizeNaN(d)));
        return true;
      }
      default: {
        break;
      }
    }
    MOZ_CRASH("unexpected type when creating global exports");
}

static bool
CreateExportObject(JSContext* cx,
                   HandleWasmInstanceObject instanceObj,
                   Handle<FunctionVector> funcImports,
                   HandleWasmTableObject tableObj,
                   HandleWasmMemoryObject memoryObj,
                   const ValVector& globalImports,
                   const ExportVector& exports)
{
    const Instance& instance = instanceObj->instance();
    const Metadata& metadata = instance.metadata();

    if (metadata.isAsmJS() && exports.length() == 1 && strlen(exports[0].fieldName()) == 0) {
        RootedValue val(cx);
        if (!GetFunctionExport(cx, instanceObj, funcImports, exports[0], &val))
            return false;
        instanceObj->initExportsObj(val.toObject());
        return true;
    }

    RootedObject exportObj(cx);
    if (metadata.isAsmJS())
        exportObj = NewBuiltinClassInstance<PlainObject>(cx);
    else
        exportObj = NewObjectWithGivenProto<PlainObject>(cx, nullptr);
    if (!exportObj)
        return false;

    for (const Export& exp : exports) {
        JSAtom* atom = AtomizeUTF8Chars(cx, exp.fieldName(), strlen(exp.fieldName()));
        if (!atom)
            return false;

        RootedId id(cx, AtomToId(atom));
        RootedValue val(cx);
        switch (exp.kind()) {
          case DefinitionKind::Function:
            if (!GetFunctionExport(cx, instanceObj, funcImports, exp, &val))
                return false;
            break;
          case DefinitionKind::Table:
            val = ObjectValue(*tableObj);
            break;
          case DefinitionKind::Memory:
            val = ObjectValue(*memoryObj);
            break;
          case DefinitionKind::Global:
            if (!GetGlobalExport(cx, metadata.globals, exp.globalIndex(), globalImports, &val))
                return false;
            break;
        }

        if (!JS_DefinePropertyById(cx, exportObj, id, val, JSPROP_ENUMERATE))
            return false;
    }

    if (!metadata.isAsmJS()) {
        if (!JS_FreezeObject(cx, exportObj))
            return false;
    }

    instanceObj->initExportsObj(*exportObj);
    return true;
}

bool
Module::instantiate(JSContext* cx,
                    Handle<FunctionVector> funcImports,
                    HandleWasmTableObject tableImport,
                    HandleWasmMemoryObject memoryImport,
                    const ValVector& globalImports,
                    HandleObject instanceProto,
                    MutableHandleWasmInstanceObject instance) const
{
    if (!instantiateFunctions(cx, funcImports))
        return false;

    RootedWasmMemoryObject memory(cx, memoryImport);
    if (!instantiateMemory(cx, &memory))
        return false;

    RootedWasmTableObject table(cx, tableImport);
    SharedTableVector tables;
    if (!instantiateTable(cx, &table, &tables))
        return false;

    auto globalSegment = GlobalSegment::create(metadata().globalDataLength);
    if (!globalSegment) {
        ReportOutOfMemory(cx);
        return false;
    }

    SharedCode code(code_);

    if (metadata().debugEnabled) {
        // The first time through, use the pre-linked code in the module but
        // mark it as busy. Subsequently, instantiate the copy of the code
        // bytes that we keep around for debugging instead, because the debugger
        // may patch the pre-linked code at any time.
        if (!codeIsBusy_.compareExchange(false, true)) {
            auto codeSegment = CodeSegment::create(Tier::Baseline,
                                                   *unlinkedCodeForDebugging_,
                                                   *bytecode_,
                                                   linkData_.linkData(Tier::Baseline),
                                                   metadata());
            if (!codeSegment) {
                ReportOutOfMemory(cx);
                return false;
            }

            UniqueJumpTable maybeJumpTable;
            code = js_new<Code>(Move(codeSegment), metadata(), Move(maybeJumpTable));
            if (!code) {
                ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    // To support viewing the source of an instance (Instance::createText), the
    // instance must hold onto a ref of the bytecode (keeping it alive). This
    // wastes memory for most users, so we try to only save the source when a
    // developer actually cares: when the compartment is debuggable (which is
    // true when the web console is open), has code compiled with debug flag
    // enabled or a names section is present (since this going to be stripped
    // for non-developer builds).

    const ShareableBytes* maybeBytecode = nullptr;
    if (cx->compartment()->isDebuggee() || metadata().debugEnabled ||
        !metadata().funcNames.empty())
    {
        maybeBytecode = bytecode_.get();
    }

    // The debug object must be present even when debugging is not enabled: It
    // provides the lazily created source text for the program, even if that
    // text is a placeholder message when debugging is not enabled.

    bool binarySource = cx->compartment()->debuggerObservesBinarySource();
    auto debug = cx->make_unique<DebugState>(code, maybeBytecode, binarySource);
    if (!debug)
        return false;

    instance.set(WasmInstanceObject::create(cx,
                                            code,
                                            Move(debug),
                                            Move(globalSegment),
                                            memory,
                                            Move(tables),
                                            funcImports,
                                            globalImports,
                                            instanceProto));
    if (!instance)
        return false;

    if (!CreateExportObject(cx, instance, funcImports, table, memory, globalImports, exports_))
        return false;

    // Register the instance with the JSCompartment so that it can find out
    // about global events like profiling being enabled in the compartment.
    // Registration does not require a fully-initialized instance and must
    // precede initSegments as the final pre-requisite for a live instance.

    if (!cx->compartment()->wasm.registerInstance(cx, instance))
        return false;

    // Perform initialization as the final step after the instance is fully
    // constructed since this can make the instance live to content (even if the
    // start function fails).

    if (!initSegments(cx, instance, funcImports, memory, globalImports))
        return false;

    // Now that the instance is fully live and initialized, the start function.
    // Note that failure may cause instantiation to throw, but the instance may
    // still be live via edges created by initSegments or the start function.

    if (metadata().startFuncIndex) {
        FixedInvokeArgs<0> args(cx);
        if (!instance->instance().callExport(cx, *metadata().startFuncIndex, args))
            return false;
    }

    uint32_t mode = uint32_t(metadata().isAsmJS() ? Telemetry::ASMJS : Telemetry::WASM);
    cx->runtime()->addTelemetry(JS_TELEMETRY_AOT_USAGE, mode);

    JSUseCounter useCounter = metadata().isAsmJS() ? JSUseCounter::ASMJS : JSUseCounter::WASM;
    cx->runtime()->setUseCounter(instance, useCounter);

    return true;
}
