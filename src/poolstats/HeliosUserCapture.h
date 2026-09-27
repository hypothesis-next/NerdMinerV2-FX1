#pragma once
#include <stddef.h>
#include <stdint.h>

// Retain the first root value only; the HTTP layer still drains and validates
// response framing before a verified keep-alive connection can be reused.
class HeliosUserCapture {
 public:
  HeliosUserCapture(uint8_t *buffer, size_t capacity, size_t bodyLimit)
      : buffer_(buffer), capacity_(capacity), bodyLimit_(bodyLimit) {}
  bool consume(uint8_t value) {
    if (++bodyBytes_ > bodyLimit_) return false;
    if (complete_) return true;
    if (!buffer_ || used_ >= capacity_) return false;
    buffer_[used_++] = value;
    if (inString_) {
      if (escaped_) escaped_ = false;
      else if (value == '\\') escaped_ = true;
      else if (value == '"') inString_ = false;
    } else if (value == '"') inString_ = true;
    else if (value == '{' || value == '[') ++depth_;
    else if (value == '}' || value == ']') {
      if (!depth_) return false;
      --depth_;
      if (depth_ == 1) complete_ = true;
    }
    return true;
  }
  size_t size() const { return used_; }
  size_t bodyBytes() const { return bodyBytes_; }
  bool complete() const { return complete_; }
  bool needsCapacity() const { return !complete_ && used_ == capacity_; }
  void rebind(uint8_t *buffer, size_t capacity) { buffer_ = buffer; capacity_ = capacity; }
 private:
  uint8_t *buffer_;
  size_t capacity_, bodyLimit_, used_ = 0, bodyBytes_ = 0;
  unsigned depth_ = 0;
  bool inString_ = false, escaped_ = false, complete_ = false;
};
