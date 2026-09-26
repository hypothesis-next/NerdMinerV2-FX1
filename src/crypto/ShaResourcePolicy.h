#pragma once
#include <atomic>
#include <stdint.h>

// A secure transport needs CPU time on the other core and may use SHA through
// mbedTLS. Mining continues with its validated software fallback during it.
inline std::atomic<uint32_t> &secureTransportSessions() {
  static std::atomic<uint32_t> sessions{0};
  return sessions;
}
inline bool secureTransportActive() {
  return secureTransportSessions().load(std::memory_order_acquire) != 0;
}
class SecureTransportWork {
 public:
  explicit SecureTransportWork(bool secure) : secure_(secure) {
    if (secure_) secureTransportSessions().fetch_add(1, std::memory_order_acq_rel);
  }
  ~SecureTransportWork() {
    if (secure_) secureTransportSessions().fetch_sub(1, std::memory_order_acq_rel);
  }
  SecureTransportWork(const SecureTransportWork &) = delete;
  SecureTransportWork &operator=(const SecureTransportWork &) = delete;
 private:
  bool secure_;
};
