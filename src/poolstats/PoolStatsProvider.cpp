#include "PoolStatsProvider.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <esp_heap_caps.h>
#include <memory>
#include <new>

#include "PoolStatsParsers.h"
#include "PoolStatsPolicy.h"
#include "StatsTlsClient.h"
#include "HeliosUserPrefix.h"
#include "HeliosUserCapture.h"
#include "../version.h"
#include "../crypto/ShaResourcePolicy.h"
#include "../drivers/displays/display.h"

namespace {
StatsTlsClient *activeTransport = nullptr;

constexpr uint32_t CONNECT_TIMEOUT_MS = 4000;
constexpr uint16_t READ_TIMEOUT_MS = 5000;
// The current multi-certificate P-384 chain exceeds five seconds on classic
// ESP32. Keep a bounded handshake deadline without weakening verification.
constexpr uint32_t TLS_HANDSHAKE_TIMEOUT_SECONDS = 60;
constexpr int MAX_RESPONSE_BYTES = 16384;
constexpr size_t MAX_HELIOS_BODY_BYTES = 512U * 1024U;
constexpr char USER_AGENT[] = "NerdMinerV2-MultiPool/" CURRENT_VERSION;

extern const uint8_t rootca_crt_bundle_start[]
    asm("_binary_data_cert_x509_crt_bundle_bin_start");
extern const char isrg_root_x2_pem_start[]
    asm("_binary_data_cert_isrg_root_x2_pem_start");

enum class PoolResponseFormat : uint8_t {
  PublicPool,
  Helios
};

enum class TlsTrust : uint8_t {
  GenericBundle,
  IsrgRootX2
};

class LimitedStream : public Stream {
 public:
  LimitedStream(Client &source, size_t limit)
      : source_(source), remaining_(limit), exhausted_(false) {
    setTimeout(READ_TIMEOUT_MS);
  }

  int available() override {
    if (bufferPosition_ < bufferSize_) return bufferSize_ - bufferPosition_;
    const int sourceAvailable = source_.available();
    if (remaining_ == 0) return 0;
    return sourceAvailable > static_cast<int>(remaining_)
               ? static_cast<int>(remaining_)
               : sourceAvailable;
  }

  int read() override {
    if (bufferPosition_ < bufferSize_) return buffer_[bufferPosition_++];
    if (remaining_ == 0) {
      exhausted_ = true;
      return -1;
    }
    const uint32_t started = millis();
    int sourceAvailable;
    while ((sourceAvailable = source_.available()) == 0) {
      if (millis() - started >= READ_TIMEOUT_MS) return -1;
      delay(1);
    }
    if (sourceAvailable < 0) return -1;
    size_t count = static_cast<size_t>(sourceAvailable);
    if (count > sizeof(buffer_)) count = sizeof(buffer_);
    if (count > remaining_) count = remaining_;
    const int received = source_.read(buffer_, count);
    if (received <= 0) return -1;
    remaining_ -= static_cast<size_t>(received);
    bufferPosition_ = 1;
    bufferSize_ = static_cast<size_t>(received);
    return buffer_[0];
  }

  int peek() override {
    if (bufferPosition_ < bufferSize_) return buffer_[bufferPosition_];
    if (remaining_ == 0) return -1;
    return source_.peek();
  }

  void flush() override { source_.flush(); }
  size_t write(uint8_t) override { return 0; }
  bool exhausted() const { return exhausted_; }
  size_t bufferedBytes() const { return MAX_RESPONSE_BYTES - remaining_; }

 private:
  Client &source_;
  size_t remaining_;
  bool exhausted_;
  // JSON filtering reads bytes individually. Batch the underlying TLS reads
  // without reading beyond the same bounded prefix or retaining the history.
  uint8_t buffer_[512] = {};
  size_t bufferPosition_ = 0;
  size_t bufferSize_ = 0;
};

bool safeWallet(const char *wallet) {
  if (wallet == nullptr || wallet[0] == '\0') return false;
  for (const char *cursor = wallet; *cursor != '\0'; cursor++) {
    if (!isalnum(static_cast<unsigned char>(*cursor))) return false;
  }
  return true;
}

PoolFetchResult parsePublicPoolResponse(HTTPClient &http,
                                        PoolStatsSnapshot &snapshot) {
  StaticJsonDocument<256> filter;
  filter["bestDifficulty"] = true;
  filter["workersCount"] = true;
  filter["workers"][0]["hashRate"] = true;

  StaticJsonDocument<2048> document;
  LimitedStream stream(*http.getStreamPtr(), MAX_RESPONSE_BYTES);
  const DeserializationError error = deserializeJson(
      document, stream, DeserializationOption::Filter(filter));
  Serial.printf("[PoolStats] prefix bytes=%u json=%u t=%u\n",
    static_cast<unsigned>(stream.bufferedBytes()), static_cast<unsigned>(error.code()), millis());
  if (error || stream.exhausted() || document.overflowed() ||
      !parsePublicPoolDocument(document.as<JsonVariantConst>(), snapshot)) {
    return {PoolFetchStatus::InvalidResponse, HTTP_CODE_OK};
  }
  return {PoolFetchStatus::Success, HTTP_CODE_OK};
}

PoolFetchResult parseHeliosResponse(HTTPClient &http,
                                    PoolStatsSnapshot &snapshot) {
  uint64_t serverEpoch = 0;
  if (!parseHttpDateUtc(http.header("Date").c_str(), serverEpoch)) {
    return {PoolFetchStatus::InvalidResponse, HTTP_CODE_OK};
  }

  // Drain HTTP framing, including chunked transfer, without keeping histories.
  // This lets subsequent 60-second refreshes reuse the verified TLS socket.
  constexpr size_t INITIAL_USER_BYTES = 2048;
  std::unique_ptr<uint8_t[]> prefix(new (std::nothrow) uint8_t[INITIAL_USER_BYTES]);
  if (!prefix) return {PoolFetchStatus::RetryableError, HTTP_CODE_OK};
  class CaptureStream final : public Stream {
   public:
    explicit CaptureStream(std::unique_ptr<uint8_t[]> &buffer)
        : capture(buffer.get(), INITIAL_USER_BYTES, MAX_HELIOS_BODY_BYTES),
          began(millis()), storage(buffer) {}
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t *data, size_t length) override {
      if (millis() - began > 30000U) return 0;
      for (size_t i = 0; i < length; ++i) {
        if (capture.needsCapacity()) {
          const size_t nextSize = capacity * 2;
          if (nextSize > MAX_RESPONSE_BYTES) return 0;
          std::unique_ptr<uint8_t[]> next(new (std::nothrow) uint8_t[nextSize]);
          if (!next) return 0;
          memcpy(next.get(), storage.get(), capture.size());
          storage.swap(next);
          capacity = nextSize;
          capture.rebind(storage.get(), capacity);
        }
        if (!capture.consume(data[i])) return 0;
      }
      return length;
    }
    HeliosUserCapture capture;
    uint32_t began;
    std::unique_ptr<uint8_t[]> &storage;
    size_t capacity = INITIAL_USER_BYTES;
  } captured(prefix);
  const int bodyBytes = http.writeToStream(&captured);
  Serial.printf("[PoolStats] drained=%d userPrefix=%u complete=%u\n", bodyBytes,
      static_cast<unsigned>(captured.capture.size()), captured.capture.complete());
  if (bodyBytes < 0 || !captured.capture.complete())
    return {PoolFetchStatus::InvalidResponse, HTTP_CODE_OK};
  size_t position = 0;
  if (!enterHeliosUserObject([&]() {
        return position < captured.capture.size() ? static_cast<int>(prefix[position++]) : -1;
      }))
    return {PoolFetchStatus::InvalidResponse, HTTP_CODE_OK};

  StaticJsonDocument<256> filter;
  filter["stats"][0]["bestEver"] = true;
  filter["stats"][0]["hashrate5m"] = true;
  filter["workers"][0]["lastUpdate"] = true;

  StaticJsonDocument<4096> document;
  const DeserializationError error = deserializeJson(
      document, prefix.get() + position, captured.capture.size() - position,
      DeserializationOption::Filter(filter));
  if (error || document.overflowed() ||
      !parseHeliosSnapshotDocument(document.as<JsonVariantConst>(), serverEpoch,
                                   snapshot)) {
    return {PoolFetchStatus::InvalidResponse, HTTP_CODE_OK};
  }
  return {PoolFetchStatus::Success, HTTP_CODE_OK};
}

PoolFetchResult executeRequest(const PoolIdentity &identity,
                               PoolStatsSnapshot &snapshot,
                               PoolResponseFormat responseFormat,
                               TlsTrust tlsTrust,
                               char *lastModified,
                               size_t lastModifiedSize) {
  if (!safeWallet(identity.wallet)) {
    return {PoolFetchStatus::Unavailable, HTTP_CODE_BAD_REQUEST};
  }

  char url[POOL_API_URL_MAX_LENGTH + POOL_WALLET_MAX_LENGTH + 1] = {};
  const int urlLength = snprintf(url, sizeof(url), "%s%s",
                                 identity.definition.apiBaseUrl,
                                 identity.wallet);
  if (urlLength < 0 || static_cast<size_t>(urlLength) >= sizeof(url)) {
    return {PoolFetchStatus::InvalidResponse, 0};
  }

  const bool secure = strncmp(url, "https://", 8) == 0;
  // Single statistics-task ownership; configuration/provider is immutable for
  // this boot. Retain verified connections, never share them with mining.
  static HTTPClient http;
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(READ_TIMEOUT_MS);
  const bool helios = responseFormat == PoolResponseFormat::Helios;
  http.useHTTP10(!helios);
  http.setReuse(helios);
  http.setUserAgent(USER_AGENT);

  const char *headerKeys[] = {"Date", "Last-Modified"};
  http.collectHeaders(headerKeys, 2);

  static WiFiClient plainClient;
  static StatsTlsClient secureClient(helios);
  activeTransport = &secureClient;
  secureClient.beginRequest();
  bool began = false;
  if (secure) {
    if (tlsTrust == TlsTrust::IsrgRootX2) {
      secureClient.setCACert(isrg_root_x2_pem_start);
    } else {
      secureClient.setCACertBundle(rootca_crt_bundle_start);
    }
    secureClient.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_SECONDS);
    secureClient.setTimeout(READ_TIMEOUT_MS / 1000);
    began = http.begin(secureClient, url);
  } else {
    plainClient.setTimeout(READ_TIMEOUT_MS);
    began = http.begin(plainClient, url);
  }
  if (!began) {
    return {PoolFetchStatus::RetryableError, 0};
  }

  if (helios && lastModified != nullptr && lastModified[0] != '\0') {
    http.addHeader("If-Modified-Since", lastModified);
  }

  const int status = http.GET();
  if (helios) {
    Serial.printf("[PoolStats] Helios HTTP=%d\n", status);
    if (status < 0) {
      char tlsError[128] = {};
      const int tlsCode = secureClient.lastError(tlsError, sizeof(tlsError));
      Serial.printf("[PoolStats] TLS=%d detail=%s\n", tlsCode, tlsError);
    }
  }
  if (status == HTTP_CODE_NOT_MODIFIED) {
    http.end();
    return {PoolFetchStatus::NotModified, static_cast<int16_t>(status)};
  }
  if (status != HTTP_CODE_OK) {
    secureClient.stop();
    http.end();
    return classifyPoolHttpStatus(status);
  }

  const int responseSize = http.getSize();
  // Certificate/handshake scratch is now gone. Restore rendering before the
  // bounded body drain, rather than holding the previous frame during history
  // transfer. The service's scope guard remains the error-path safety net.
  if (helios) endStatsDisplayMemoryWindow();
  if (!helios && responseSize > MAX_RESPONSE_BYTES) {
    http.end();
    return {PoolFetchStatus::InvalidResponse, static_cast<int16_t>(status)};
  }

  PoolFetchResult result = helios ? parseHeliosResponse(http, snapshot)
                                  : parsePublicPoolResponse(http, snapshot);
  if (result.status != PoolFetchStatus::Success) secureClient.stop();
  if (result.status == PoolFetchStatus::Success && helios &&
      lastModified != nullptr && lastModifiedSize > 0) {
    const String value = http.header("Last-Modified");
    if (value.length() > 0) {
      snprintf(lastModified, lastModifiedSize, "%s", value.c_str());
    }
  }
  http.end();
  Serial.printf("[PoolStats] response bytes=%d parsed=%u tlsCryptoMs=%llu t=%u\n",
      responseSize, static_cast<unsigned>(result.status),
      static_cast<unsigned long long>(secureClient.guardedUs()/1000), millis());
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  Serial.printf("[PoolStats] done heap=%u max=%u min=%u stackFree=%u\n",
    heap_caps_get_free_size(caps), heap_caps_get_largest_free_block(caps),
    heap_caps_get_minimum_free_size(caps), uxTaskGetStackHighWaterMark(nullptr));
  return result;
}

class PublicPoolCompatibleProvider final : public PoolStatsProvider {
 public:
  PoolFetchResult fetch(const PoolIdentity &identity,
                        PoolStatsSnapshot &snapshot, char *, size_t) override {
    return executeRequest(identity, snapshot, PoolResponseFormat::PublicPool,
                          TlsTrust::GenericBundle, nullptr, 0);
  }
};

class HeliosPoolProvider final : public PoolStatsProvider {
 public:
  PoolFetchResult fetch(const PoolIdentity &identity,
                        PoolStatsSnapshot &snapshot, char *lastModified,
                        size_t lastModifiedSize) override {
    return executeRequest(identity, snapshot, PoolResponseFormat::Helios,
                          TlsTrust::IsrgRootX2, lastModified, lastModifiedSize);
  }
};

PublicPoolCompatibleProvider publicPoolProvider;
HeliosPoolProvider heliosPoolProvider;

}  // namespace

bool poolStatsHasReusableTransport() {
  if (!activeTransport) return false;
  const bool ready = activeTransport->verifiedConnection() && activeTransport->connected();
  activeTransport->requireExistingConnection(ready);
  return ready;
}

PoolStatsProvider *providerFor(PoolProviderKind provider) {
  switch (provider) {
    case PoolProviderKind::PublicPoolCompatible:
      return &publicPoolProvider;
    case PoolProviderKind::HeliosPool:
      return &heliosPoolProvider;
    default:
      return nullptr;
  }
}
