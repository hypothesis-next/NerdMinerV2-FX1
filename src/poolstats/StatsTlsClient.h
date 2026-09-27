#pragma once
#include <WiFiClientSecure.h>

// Classic ESP32 uses official software SHA ALT for this task's TLS contexts.
// Other targets retain the secure-transport coordination token.
class StatsTlsClient final : public WiFiClientSecure {
 public:
  explicit StatsTlsClient(bool resumeHelios) : resumeHelios_(resumeHelios) {}
  ~StatsTlsClient() override;
  void beginRequest();
  void requireExistingConnection(bool required) { requireExisting_ = required; }
  bool verifiedConnection() const { return verified_; }
  int connect(const char *host, uint16_t port) override;
  int connect(const char *host, uint16_t port, int32_t timeout) override;
  int available() override;
  int read() override;
  int read(uint8_t *buffer, size_t size) override;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  void stop() override;
  uint64_t guardedUs() const { return guardedUs_; }
 private:
  bool resumeHelios_;
  uint64_t guardedUs_ = 0;
  uint32_t guardDepth_ = 0;
  bool requireExisting_ = false;
  bool verified_ = false;
};
