// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/streaming-decoder.h"

#include "src/base/template-utils.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/wasm/decoder.h"
#include "src/wasm/leb-helper.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {

void StreamingDecoder::OnBytesReceived(Vector<const uint8_t> bytes) {
  size_t current = 0;
  while (ok() && current < bytes.size()) {
    size_t num_bytes =
        state_->ReadBytes(this, bytes.SubVector(current, bytes.size()));
    current += num_bytes;
    module_offset_ += num_bytes;
    if (state_->is_finished()) {
      state_ = state_->Next(this);
    }
  }
  total_size_ += bytes.size();
  if (ok()) {
    processor_->OnFinishedChunk();
  }
}

size_t StreamingDecoder::DecodingState::ReadBytes(StreamingDecoder* streaming,
                                                  Vector<const uint8_t> bytes) {
  size_t num_bytes = std::min(bytes.size(), remaining());
  memcpy(buffer() + offset(), &bytes.first(), num_bytes);
  set_offset(offset() + num_bytes);
  return num_bytes;
}

void StreamingDecoder::Finish() {
  if (!ok()) {
    return;
  }

  if (!state_->is_finishing_allowed()) {
    // The byte stream ended too early, we report an error.
    Error("unexpected end of stream");
    return;
  }

  std::unique_ptr<uint8_t[]> bytes(new uint8_t[total_size_]);
  uint8_t* cursor = bytes.get();
  {
#define BYTES(x) (x & 0xff), (x >> 8) & 0xff, (x >> 16) & 0xff, (x >> 24) & 0xff
    uint8_t module_header[]{BYTES(kWasmMagic), BYTES(kWasmVersion)};
#undef BYTES
    memcpy(cursor, module_header, arraysize(module_header));
    cursor += arraysize(module_header);
  }
  for (auto&& buffer : section_buffers_) {
    DCHECK_LE(cursor - bytes.get() + buffer->length(), total_size_);
    memcpy(cursor, buffer->bytes(), buffer->length());
    cursor += buffer->length();
  }
  processor_->OnFinishedStream(std::move(bytes), total_size_);
}

void StreamingDecoder::Abort() {
  if (ok()) processor_->OnAbort();
}

// An abstract class to share code among the states which decode VarInts. This
// class takes over the decoding of the VarInt and then calls the actual decode
// code with the decoded value.
class StreamingDecoder::DecodeVarInt32 : public DecodingState {
 public:
  explicit DecodeVarInt32(size_t max_value, const char* field_name)
      : max_value_(max_value), field_name_(field_name) {}
  uint8_t* buffer() override { return byte_buffer_; }
  size_t size() const override { return kMaxVarInt32Size; }

  size_t ReadBytes(StreamingDecoder* streaming,
                   Vector<const uint8_t> bytes) override;

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

  virtual std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) = 0;

  size_t value() const { return value_; }
  size_t bytes_needed() const { return bytes_needed_; }

 private:
  uint8_t byte_buffer_[kMaxVarInt32Size];
  // The maximum valid value decoded in this state. {Next} returns an error if
  // this value is exceeded.
  size_t max_value_;
  const char* field_name_;
  size_t value_ = 0;
  size_t bytes_needed_ = 0;
};

class StreamingDecoder::DecodeModuleHeader : public DecodingState {
 public:
  size_t size() const override { return kModuleHeaderSize; }
  uint8_t* buffer() override { return byte_buffer_; }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  // Checks if the magic bytes of the module header are correct.
  void CheckHeader(Decoder* decoder);

  // The size of the module header.
  static constexpr size_t kModuleHeaderSize = 8;
  uint8_t byte_buffer_[kModuleHeaderSize];
};

class StreamingDecoder::DecodeSectionID : public DecodingState {
 public:
  explicit DecodeSectionID(uint32_t module_offset)
      : module_offset_(module_offset) {}

  size_t size() const override { return 1; }
  uint8_t* buffer() override { return &id_; }
  bool is_finishing_allowed() const override { return true; }

  uint8_t id() const { return id_; }

  uint32_t module_offset() const { return module_offset_; }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  uint8_t id_ = 0;
  // The start offset of this section in the module.
  uint32_t module_offset_;
};

class StreamingDecoder::DecodeSectionLength : public DecodeVarInt32 {
 public:
  explicit DecodeSectionLength(uint8_t id, uint32_t module_offset)
      : DecodeVarInt32(kV8MaxWasmModuleSize, "section length"),
        section_id_(id),
        module_offset_(module_offset) {}

  uint8_t section_id() const { return section_id_; }

  uint32_t module_offset() const { return module_offset_; }

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  uint8_t section_id_;
  // The start offset of this section in the module.
  uint32_t module_offset_;
};

class StreamingDecoder::DecodeSectionPayload : public DecodingState {
 public:
  explicit DecodeSectionPayload(SectionBuffer* section_buffer)
      : section_buffer_(section_buffer) {}

  size_t size() const override { return section_buffer_->payload_length(); }
  uint8_t* buffer() override {
    return section_buffer_->bytes() + section_buffer_->payload_offset();
  }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

  SectionBuffer* section_buffer() const { return section_buffer_; }

 private:
  SectionBuffer* section_buffer_;
};

class StreamingDecoder::DecodeNumberOfFunctions : public DecodeVarInt32 {
 public:
  explicit DecodeNumberOfFunctions(SectionBuffer* section_buffer)
      : DecodeVarInt32(kV8MaxWasmFunctions, "functions count"),
        section_buffer_(section_buffer) {}

  SectionBuffer* section_buffer() const { return section_buffer_; }

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  SectionBuffer* section_buffer_;
};

class StreamingDecoder::DecodeFunctionLength : public DecodeVarInt32 {
 public:
  explicit DecodeFunctionLength(SectionBuffer* section_buffer,
                                size_t buffer_offset,
                                size_t num_remaining_functions)
      : DecodeVarInt32(kV8MaxWasmFunctionSize, "body size"),
        section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        // We are reading a new function, so one function less is remaining.
        num_remaining_functions_(num_remaining_functions - 1) {
    DCHECK_GT(num_remaining_functions, 0);
  }

  size_t num_remaining_functions() const { return num_remaining_functions_; }
  size_t buffer_offset() const { return buffer_offset_; }
  SectionBuffer* section_buffer() const { return section_buffer_; }

  std::unique_ptr<DecodingState> NextWithValue(
      StreamingDecoder* streaming) override;

 private:
  SectionBuffer* section_buffer_;
  size_t buffer_offset_;
  size_t num_remaining_functions_;
};

class StreamingDecoder::DecodeFunctionBody : public DecodingState {
 public:
  explicit DecodeFunctionBody(SectionBuffer* section_buffer,
                              size_t buffer_offset, size_t function_length,
                              size_t num_remaining_functions,
                              uint32_t module_offset)
      : section_buffer_(section_buffer),
        buffer_offset_(buffer_offset),
        size_(function_length),
        num_remaining_functions_(num_remaining_functions),
        module_offset_(module_offset) {}

  size_t buffer_offset() const { return buffer_offset_; }
  size_t size() const override { return size_; }
  uint8_t* buffer() override {
    return section_buffer_->bytes() + buffer_offset_;
  }
  size_t num_remaining_functions() const { return num_remaining_functions_; }
  uint32_t module_offset() const { return module_offset_; }
  SectionBuffer* section_buffer() const { return section_buffer_; }

  std::unique_ptr<DecodingState> Next(StreamingDecoder* streaming) override;

 private:
  SectionBuffer* section_buffer_;
  size_t buffer_offset_;
  size_t size_;
  size_t num_remaining_functions_;
  uint32_t module_offset_;
};

size_t StreamingDecoder::DecodeVarInt32::ReadBytes(
    StreamingDecoder* streaming, Vector<const uint8_t> bytes) {
  size_t bytes_read = std::min(bytes.size(), remaining());
  memcpy(buffer() + offset(), &bytes.first(), bytes_read);
  Decoder decoder(buffer(), buffer() + offset() + bytes_read,
                  streaming->module_offset());
  value_ = decoder.consume_u32v(field_name_);
  // The number of bytes we actually needed to read.
  DCHECK_GT(decoder.pc(), buffer());
  bytes_needed_ = static_cast<size_t>(decoder.pc() - buffer());

  if (decoder.failed()) {
    if (offset() + bytes_read == size()) {
      // We only report an error if we read all bytes.
      streaming->Error(decoder.toResult(nullptr));
    }
    set_offset(offset() + bytes_read);
    return bytes_read;
  } else {
    DCHECK_GT(bytes_needed_, offset());
    size_t result = bytes_needed_ - offset();
    // We read all the bytes we needed.
    set_offset(size());
    return result;
  }
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeVarInt32::Next(StreamingDecoder* streaming) {
  if (!streaming->ok()) {
    return nullptr;
  }
  if (value() > max_value_) {
    std::ostringstream oss;
    oss << "function size > maximum function size: " << value() << " < "
        << max_value_;
    return streaming->Error(oss.str());
  }

  return NextWithValue(streaming);
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeModuleHeader::Next(StreamingDecoder* streaming) {
  streaming->ProcessModuleHeader();
  if (streaming->ok()) {
    return base::make_unique<DecodeSectionID>(streaming->module_offset());
  }
  return nullptr;
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionID::Next(StreamingDecoder* streaming) {
  return base::make_unique<DecodeSectionLength>(id(), module_offset());
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionLength::NextWithValue(
    StreamingDecoder* streaming) {
  SectionBuffer* buf = streaming->CreateNewBuffer(
      module_offset(), section_id(), value(),
      Vector<const uint8_t>(buffer(), static_cast<int>(bytes_needed())));
  if (!buf) return nullptr;
  if (value() == 0) {
    if (section_id() == SectionCode::kCodeSectionCode) {
      return streaming->Error("Code section cannot have size 0");
    } else {
      streaming->ProcessSection(buf);
      if (streaming->ok()) {
        // There is no payload, we go to the next section immediately.
        return base::make_unique<DecodeSectionID>(streaming->module_offset());
      } else {
        return nullptr;
      }
    }
  } else {
    if (section_id() == SectionCode::kCodeSectionCode) {
      // We reached the code section. All functions of the code section are put
      // into the same SectionBuffer.
      return base::make_unique<DecodeNumberOfFunctions>(buf);
    } else {
      return base::make_unique<DecodeSectionPayload>(buf);
    }
  }
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeSectionPayload::Next(StreamingDecoder* streaming) {
  streaming->ProcessSection(section_buffer());
  if (streaming->ok()) {
    return base::make_unique<DecodeSectionID>(streaming->module_offset());
  }
  return nullptr;
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeNumberOfFunctions::NextWithValue(
    StreamingDecoder* streaming) {
  // Copy the bytes we read into the section buffer.
  if (section_buffer_->payload_length() >= bytes_needed()) {
    memcpy(section_buffer_->bytes() + section_buffer_->payload_offset(),
           buffer(), bytes_needed());
  } else {
    return streaming->Error("Invalid code section length");
  }

  // {value} is the number of functions.
  if (value() > 0) {
    streaming->StartCodeSection(value());
    if (!streaming->ok()) return nullptr;
    return base::make_unique<DecodeFunctionLength>(
        section_buffer(), section_buffer()->payload_offset() + bytes_needed(),
        value());
  } else {
    return base::make_unique<DecodeSectionID>(streaming->module_offset());
  }
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeFunctionLength::NextWithValue(
    StreamingDecoder* streaming) {
  // Copy the bytes we read into the section buffer.
  if (section_buffer_->length() >= buffer_offset_ + bytes_needed()) {
    memcpy(section_buffer_->bytes() + buffer_offset_, buffer(), bytes_needed());
  } else {
    return streaming->Error("Invalid code section length");
  }

  // {value} is the length of the function.
  if (value() == 0) {
    return streaming->Error("Invalid function length (0)");
  } else if (buffer_offset() + bytes_needed() + value() >
             section_buffer()->length()) {
    streaming->Error("not enough code section bytes");
    return nullptr;
  }

  return base::make_unique<DecodeFunctionBody>(
      section_buffer(), buffer_offset() + bytes_needed(), value(),
      num_remaining_functions(), streaming->module_offset());
}

std::unique_ptr<StreamingDecoder::DecodingState>
StreamingDecoder::DecodeFunctionBody::Next(StreamingDecoder* streaming) {
  streaming->ProcessFunctionBody(
      Vector<const uint8_t>(buffer(), static_cast<int>(size())),
      module_offset());
  if (!streaming->ok()) {
    return nullptr;
  }
  if (num_remaining_functions() != 0) {
    return base::make_unique<DecodeFunctionLength>(
        section_buffer(), buffer_offset() + size(), num_remaining_functions());
  } else {
    if (buffer_offset() + size() != section_buffer()->length()) {
      return streaming->Error("not all code section bytes were used");
    }
    return base::make_unique<DecodeSectionID>(streaming->module_offset());
  }
}

StreamingDecoder::StreamingDecoder(
    std::unique_ptr<StreamingProcessor> processor)
    : processor_(std::move(processor)),
      // A module always starts with a module header.
      state_(new DecodeModuleHeader()) {}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
