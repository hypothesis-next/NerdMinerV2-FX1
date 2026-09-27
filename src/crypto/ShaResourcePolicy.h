#pragma once
#include <atomic>
#include <stdint.h>

// A transport whose digest contexts may own the SHA peripheral selects exact
// software mining fallback. Classic statistics TLS instead uses SDK software
// SHA contexts and keeps hardware mining eligible.
inline std::atomic<uint32_t> &secureTransportSessions() {
  static std::atomic<uint32_t> sessions{0};
  return sessions;
}
inline bool secureTransportActive() {
  return secureTransportSessions().load(std::memory_order_acquire) != 0;
}
// Software TLS cryptography needs CPU windows between completed hardware
// groups. The signal never weakens the documented SHA/DPORT access contract.
inline std::atomic<uint32_t> &secureTransportCpuSessions() {
  static std::atomic<uint32_t> sessions{0};
  return sessions;
}
inline bool secureTransportCpuActive() {
  return secureTransportCpuSessions().load(std::memory_order_acquire) != 0;
}
inline std::atomic<uint32_t> &secureTransportHandshakeSessions() {
  static std::atomic<uint32_t> sessions{0};
  return sessions;
}
inline bool secureTransportHandshakeActive() {
  return secureTransportHandshakeSessions().load(std::memory_order_acquire) != 0;
}
inline std::atomic<uint32_t> &shaCpuWindowMicroseconds() {
  static std::atomic<uint32_t> value{0};
  return value;
}
inline std::atomic<uint32_t> &shaHandshakeWindowMicroseconds() {
  static std::atomic<uint32_t> value{0};
  return value;
}
class SecureTransportCpuWork {
 public:
  explicit SecureTransportCpuWork(bool active, bool handshake = false)
      : active_(active), handshake_(active && handshake) {
    if (handshake_) secureTransportHandshakeSessions().fetch_add(1, std::memory_order_acq_rel);
    if (active_) secureTransportCpuSessions().fetch_add(1, std::memory_order_acq_rel);
  }
  ~SecureTransportCpuWork() {
    if (active_) secureTransportCpuSessions().fetch_sub(1, std::memory_order_acq_rel);
    if (handshake_) secureTransportHandshakeSessions().fetch_sub(1, std::memory_order_acq_rel);
  }
  SecureTransportCpuWork(const SecureTransportCpuWork &) = delete;
  SecureTransportCpuWork &operator=(const SecureTransportCpuWork &) = delete;
 private:
  bool active_;
  bool handshake_;
};
// Bounded diagnostics only on the hardware worker's software-fallback path.
// Unsigned deltas intentionally tolerate counter wrap between snapshots.
inline std::atomic<uint32_t> &shaFallbackMicroseconds() {
  static std::atomic<uint32_t> value{0};
  return value;
}
inline std::atomic<uint32_t> &shaFallbackNonces() {
  static std::atomic<uint32_t> value{0};
  return value;
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
