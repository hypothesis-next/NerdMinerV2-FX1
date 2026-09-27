#include "StatsTlsClient.h"
#include "../crypto/ShaResourcePolicy.h"
#include <esp_timer.h>
#include <atomic>
#include <string.h>
#include <mbedtls/sha256.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha512.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {
struct SessionCache {
  mbedtls_ssl_session session;
  bool valid = false;
  SessionCache() { mbedtls_ssl_session_init(&session); }
  ~SessionCache() { mbedtls_ssl_session_free(&session); }
  void clear() {
    valid = false;
    mbedtls_ssl_session_free(&session);
    mbedtls_ssl_session_init(&session);
  }
};
SessionCache cache;
// The provider is single-task owned. Other TLS users only read this atomic
// pointer; they never access this provider's session or mutate hook state.
std::atomic<mbedtls_ssl_context *> resumeContext{nullptr};
bool sessionOffered = false;
std::atomic<TaskHandle_t> tlsTask{nullptr};
uint64_t *tlsTotal = nullptr;
uint32_t *tlsDepth = nullptr;
bool ownTlsTask() {
  return tlsTask.load(std::memory_order_acquire) == xTaskGetCurrentTaskHandle();
}

class TimedTlsWork {
 public:
  TimedTlsWork(uint64_t &total, uint32_t &depth, bool handshake = false) : total_(total), depth_(depth),
      outer_(depth++ == 0), start_(esp_timer_get_time()), work_(needsMiningFallback()),
      cpuWork_(!needsMiningFallback(), handshake) {}
  ~TimedTlsWork() {
    --depth_;
    if (outer_) total_ += esp_timer_get_time() - start_;
  }
 private:
  static constexpr bool needsMiningFallback() {
#if CONFIG_IDF_TARGET_ESP32
    // This task's SHA contexts use the official software ALT path. Other
    // peripheral users retain the SDK engine/memory locks; socket waits and
    // certificate arithmetic need not evict the hardware miner.
    return false;
#else
    return true;
#endif
  }
  uint64_t &total_;
  uint32_t &depth_;
  bool outer_;
  int64_t start_;
  SecureTransportWork work_;
  SecureTransportCpuWork cpuWork_;
};
}

extern "C" int __real_mbedtls_ssl_handshake(mbedtls_ssl_context *ssl);
extern "C" int __wrap_mbedtls_ssl_handshake(mbedtls_ssl_context *ssl) {
  if (resumeContext.load(std::memory_order_acquire) == ssl && !sessionOffered) {
    // Standard authenticated TLS 1.2 with forward-secret P-256 ECDHE.
    // Avoid negotiating a larger ephemeral curve on this constrained CPU;
    // certificate signatures still use the normal verifier and trust anchor.
    static const mbedtls_ecp_group_id curves[] = {
        MBEDTLS_ECP_DP_SECP256R1, MBEDTLS_ECP_DP_NONE};
    static const int suites[] = {
        MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, 0};
    auto *configuration = const_cast<mbedtls_ssl_config *>(ssl->conf);
    mbedtls_ssl_conf_curves(configuration, curves);
    mbedtls_ssl_conf_ciphersuites(configuration, suites);
    Serial.printf("[StatsTLS] configured P-256 ECDHE / AES-128-GCM state=%d\n", ssl->state);
  }
  if (resumeContext.load(std::memory_order_acquire) == ssl && !sessionOffered) {
    sessionOffered = true;
    if (cache.valid && mbedtls_ssl_set_session(ssl, &cache.session) != 0)
      cache.clear();
  }
  if (ownTlsTask()) {
    TimedTlsWork work(*tlsTotal, *tlsDepth, true);
    return __real_mbedtls_ssl_handshake(ssl);
  }
  return __real_mbedtls_ssl_handshake(ssl);
}

// Classic ESP32's official SHA ALT implementation provides a complete software
// mode. Select it before processing the first block of this task's TLS digest:
// transcript contexts must not hold a hardware engine across socket waits.
// Never modify an active hardware context or another task's context.
extern "C" int __real_mbedtls_sha256_starts_ret(mbedtls_sha256_context *, int);
extern "C" int __real_mbedtls_sha1_starts_ret(mbedtls_sha1_context *);
extern "C" int __wrap_mbedtls_sha1_starts_ret(mbedtls_sha1_context *ctx) {
  const int result = __real_mbedtls_sha1_starts_ret(ctx);
#if CONFIG_IDF_TARGET_ESP32
  if (result == 0 && ownTlsTask() && ctx->mode == ESP_MBEDTLS_SHA1_UNUSED)
    ctx->mode = ESP_MBEDTLS_SHA1_SOFTWARE;
#endif
  return result;
}
extern "C" int __wrap_mbedtls_sha256_starts_ret(mbedtls_sha256_context *ctx, int is224) {
  const int result = __real_mbedtls_sha256_starts_ret(ctx, is224);
#if CONFIG_IDF_TARGET_ESP32
  if (result == 0 && ownTlsTask() && ctx->mode == ESP_MBEDTLS_SHA256_UNUSED)
    ctx->mode = ESP_MBEDTLS_SHA256_SOFTWARE;
#endif
  return result;
}
extern "C" int __real_mbedtls_sha512_starts_ret(mbedtls_sha512_context *, int);
extern "C" int __wrap_mbedtls_sha512_starts_ret(mbedtls_sha512_context *ctx, int is384) {
  const int result = __real_mbedtls_sha512_starts_ret(ctx, is384);
#if CONFIG_IDF_TARGET_ESP32
  if (result == 0 && ownTlsTask() && ctx->mode == ESP_MBEDTLS_SHA512_UNUSED)
    ctx->mode = ESP_MBEDTLS_SHA512_SOFTWARE;
#endif
  return result;
}
extern "C" int __real_mbedtls_x509_crt_parse(mbedtls_x509_crt *, const unsigned char *, size_t);
extern "C" int __wrap_mbedtls_x509_crt_parse(mbedtls_x509_crt *crt, const unsigned char *buf, size_t size) {
  if (ownTlsTask()) {
    TimedTlsWork work(*tlsTotal, *tlsDepth);
    return __real_mbedtls_x509_crt_parse(crt, buf, size);
  }
  return __real_mbedtls_x509_crt_parse(crt, buf, size);
}
extern "C" int __real_mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *,
    int (*)(void *, unsigned char *, size_t), void *, const unsigned char *, size_t);
extern "C" int __wrap_mbedtls_ctr_drbg_seed(mbedtls_ctr_drbg_context *ctx,
    int (*entropy)(void *, unsigned char *, size_t), void *arg,
    const unsigned char *custom, size_t size) {
  if (ownTlsTask()) {
    TimedTlsWork work(*tlsTotal, *tlsDepth);
    return __real_mbedtls_ctr_drbg_seed(ctx, entropy, arg, custom, size);
  }
  return __real_mbedtls_ctr_drbg_seed(ctx, entropy, arg, custom, size);
}

StatsTlsClient::~StatsTlsClient() {
  stop();
  if (ownTlsTask()) tlsTask.store(nullptr, std::memory_order_release);
}
void StatsTlsClient::beginRequest() {
  guardedUs_ = 0;
  tlsTotal = &guardedUs_;
  tlsDepth = &guardDepth_;
  tlsTask.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
}
int StatsTlsClient::connect(const char *host, uint16_t port, int32_t timeout) {
  _timeout = timeout;
  return connect(host, port);
}
int StatsTlsClient::connect(const char *host, uint16_t port) {
  // An already-open transport may disappear after the service's heap check.
  // Fail this refresh; retry with the cold-handshake memory window next time.
  if (requireExisting_) return 0;
  IPAddress address;
  const uint32_t begin = millis();
  if (!WiFi.hostByName(host, address)) {
    Serial.printf("[StatsTLS] DNS failed t=%u\n", millis());
    return 0;
  }
  const bool useCache = resumeHelios_ && port == 443 &&
      strcmp(host, "stats-btc.heliospool.com") == 0;
  const bool offered = useCache && cache.valid;
  tlsTotal = &guardedUs_;
  tlsDepth = &guardDepth_;
  tlsTask.store(xTaskGetCurrentTaskHandle(), std::memory_order_release);
  if (useCache) {
    sessionOffered = false;
    resumeContext.store(&sslclient->ssl_ctx, std::memory_order_release);
  }
  int result;
  {
    // Framework connection keeps VERIFY_REQUIRED, trusted CA and hostname
    // validation. A cached session is saved only after that call succeeds.
    result = WiFiClientSecure::connect(address, port, host, _CA_cert, _cert, _private_key);
  }
  verified_ = result != 0;
  if (result) Serial.printf("[StatsTLS] verified %s %s outputLimit=%d\n",
      mbedtls_ssl_get_version(&sslclient->ssl_ctx),
      mbedtls_ssl_get_ciphersuite(&sslclient->ssl_ctx),
      mbedtls_ssl_get_max_out_record_payload(&sslclient->ssl_ctx));
  const bool resumed = result && useCache && cache.valid && sslclient->ssl_ctx.session &&
      memcmp(cache.session.master, sslclient->ssl_ctx.session->master, 48) == 0;
  if (useCache) {
    resumeContext.store(nullptr, std::memory_order_release);
    // Keep no second certificate/session copy while the socket stays open.
    // A verified session is cloned only when that connection is closed.
    if (result) cache.clear();
  }
  Serial.printf("[StatsTLS] connect t=%u elapsed=%u offered=%u resumed=%u success=%u cryptoMs=%llu\n",
      millis(), millis()-begin, offered, resumed, result != 0,
      static_cast<unsigned long long>(guardedUs_/1000));
  return result;
}
int StatsTlsClient::available() {
  // An already decrypted record needs only a memory copy, not SHA/AES work.
  // HTTP headers are consumed one byte at a time; do not evict mining for each
  // byte of the same verified record. Empty input still uses the guarded path.
  if (mbedtls_ssl_get_bytes_avail(&sslclient->ssl_ctx) > 0)
    return WiFiClientSecure::available();
  TimedTlsWork work(guardedUs_, guardDepth_);
  return WiFiClientSecure::available();
}
int StatsTlsClient::read() {
  uint8_t value;
  return read(&value, 1) == 1 ? value : -1;
}
int StatsTlsClient::read(uint8_t *buffer, size_t size) {
  if (mbedtls_ssl_get_bytes_avail(&sslclient->ssl_ctx) > 0)
    return WiFiClientSecure::read(buffer, size);
  TimedTlsWork work(guardedUs_, guardDepth_);
  return WiFiClientSecure::read(buffer, size);
}
size_t StatsTlsClient::write(uint8_t value) { return write(&value, 1); }
size_t StatsTlsClient::write(const uint8_t *buffer, size_t size) {
  TimedTlsWork work(guardedUs_, guardDepth_);
  return WiFiClientSecure::write(buffer, size);
}
void StatsTlsClient::stop() {
  TimedTlsWork work(guardedUs_, guardDepth_);
  if (verified_ && resumeHelios_) {
    cache.clear();
    cache.valid = mbedtls_ssl_get_session(&sslclient->ssl_ctx, &cache.session) == 0;
    if (!cache.valid) cache.clear();
  }
  WiFiClientSecure::stop();
  verified_ = false;
}
