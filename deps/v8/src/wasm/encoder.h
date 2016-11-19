// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_ENCODER_H_
#define V8_WASM_ENCODER_H_

#include "src/signature.h"
#include "src/zone-containers.h"

#include "src/wasm/leb-helper.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

class ZoneBuffer : public ZoneObject {
 public:
  static const uint32_t kInitialSize = 4096;
  explicit ZoneBuffer(Zone* zone, size_t initial = kInitialSize)
      : zone_(zone), buffer_(reinterpret_cast<byte*>(zone->New(initial))) {
    pos_ = buffer_;
    end_ = buffer_ + initial;
  }

  void write_u8(uint8_t x) {
    EnsureSpace(1);
    *(pos_++) = x;
  }

  void write_u16(uint16_t x) {
    EnsureSpace(2);
    WriteLittleEndianValue<uint16_t>(pos_, x);
    pos_ += 2;
  }

  void write_u32(uint32_t x) {
    EnsureSpace(4);
    WriteLittleEndianValue<uint32_t>(pos_, x);
    pos_ += 4;
  }

  void write_u32v(uint32_t val) {
    EnsureSpace(kMaxVarInt32Size);
    LEBHelper::write_u32v(&pos_, val);
  }

  void write_size(size_t val) {
    EnsureSpace(kMaxVarInt32Size);
    DCHECK_EQ(val, static_cast<uint32_t>(val));
    LEBHelper::write_u32v(&pos_, static_cast<uint32_t>(val));
  }

  void write(const byte* data, size_t size) {
    EnsureSpace(size);
    memcpy(pos_, data, size);
    pos_ += size;
  }

  size_t reserve_u32v() {
    size_t off = offset();
    EnsureSpace(kMaxVarInt32Size);
    pos_ += kMaxVarInt32Size;
    return off;
  }

  // Patch a (padded) u32v at the given offset to be the given value.
  void patch_u32v(size_t offset, uint32_t val) {
    byte* ptr = buffer_ + offset;
    for (size_t pos = 0; pos != kPaddedVarInt32Size; ++pos) {
      uint32_t next = val >> 7;
      byte out = static_cast<byte>(val & 0x7f);
      if (pos != kPaddedVarInt32Size - 1) {
        *(ptr++) = 0x80 | out;
        val = next;
      } else {
        *(ptr++) = out;
      }
    }
  }

  size_t offset() { return static_cast<size_t>(pos_ - buffer_); }
  size_t size() { return static_cast<size_t>(pos_ - buffer_); }
  const byte* begin() { return buffer_; }
  const byte* end() { return pos_; }

  void EnsureSpace(size_t size) {
    if ((pos_ + size) > end_) {
      size_t new_size = 4096 + (end_ - buffer_) * 3;
      byte* new_buffer = reinterpret_cast<byte*>(zone_->New(new_size));
      memcpy(new_buffer, buffer_, (pos_ - buffer_));
      pos_ = new_buffer + (pos_ - buffer_);
      buffer_ = new_buffer;
      end_ = new_buffer + new_size;
    }
  }

  byte** pos_ptr() { return &pos_; }

 private:
  Zone* zone_;
  byte* buffer_;
  byte* pos_;
  byte* end_;
};

class WasmModuleBuilder;

class WasmFunctionBuilder : public ZoneObject {
 public:
  // Building methods.
  void SetSignature(FunctionSig* sig);
  uint32_t AddLocal(LocalType type);
  void EmitVarInt(uint32_t val);
  void EmitCode(const byte* code, uint32_t code_size);
  void Emit(WasmOpcode opcode);
  void EmitGetLocal(uint32_t index);
  void EmitSetLocal(uint32_t index);
  void EmitI32Const(int32_t val);
  void EmitWithU8(WasmOpcode opcode, const byte immediate);
  void EmitWithU8U8(WasmOpcode opcode, const byte imm1, const byte imm2);
  void EmitWithVarInt(WasmOpcode opcode, uint32_t immediate);
  void SetExported();
  void SetName(const char* name, int name_length);
  bool exported() { return exported_; }

  // Writing methods.
  void WriteSignature(ZoneBuffer& buffer) const;
  void WriteExport(ZoneBuffer& buffer, uint32_t func_index) const;
  void WriteBody(ZoneBuffer& buffer) const;

 private:
  explicit WasmFunctionBuilder(WasmModuleBuilder* builder);
  friend class WasmModuleBuilder;
  WasmModuleBuilder* builder_;
  LocalDeclEncoder locals_;
  uint32_t signature_index_;
  bool exported_;
  ZoneVector<uint8_t> body_;
  ZoneVector<char> name_;
};

// TODO(titzer): kill!
class WasmDataSegmentEncoder : public ZoneObject {
 public:
  WasmDataSegmentEncoder(Zone* zone, const byte* data, uint32_t size,
                         uint32_t dest);
  void Write(ZoneBuffer& buffer) const;

 private:
  ZoneVector<byte> data_;
  uint32_t dest_;
};

struct WasmFunctionImport {
  uint32_t sig_index;
  const char* name;
  int name_length;
};

class WasmModuleBuilder : public ZoneObject {
 public:
  explicit WasmModuleBuilder(Zone* zone);

  // Building methods.
  uint32_t AddFunction();
  uint32_t AddGlobal(LocalType type, bool exported);
  WasmFunctionBuilder* FunctionAt(size_t index);
  void AddDataSegment(WasmDataSegmentEncoder* data);
  uint32_t AddSignature(FunctionSig* sig);
  void AddIndirectFunction(uint32_t index);
  void MarkStartFunction(uint32_t index);
  uint32_t AddImport(const char* name, int name_length, FunctionSig* sig);

  // Writing methods.
  void WriteTo(ZoneBuffer& buffer) const;

  struct CompareFunctionSigs {
    bool operator()(FunctionSig* a, FunctionSig* b) const;
  };
  typedef ZoneMap<FunctionSig*, uint32_t, CompareFunctionSigs> SignatureMap;

  Zone* zone() { return zone_; }

 private:
  Zone* zone_;
  ZoneVector<FunctionSig*> signatures_;
  ZoneVector<WasmFunctionImport> imports_;
  ZoneVector<WasmFunctionBuilder*> functions_;
  ZoneVector<WasmDataSegmentEncoder*> data_segments_;
  ZoneVector<uint32_t> indirect_functions_;
  ZoneVector<std::pair<LocalType, bool>> globals_;
  SignatureMap signature_map_;
  int start_function_index_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_ENCODER_H_
