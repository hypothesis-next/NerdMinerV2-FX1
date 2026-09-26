#include "PoolStatsProvider.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ctype.h>

#include "PoolStatsParsers.h"
#include "PoolStatsPolicy.h"
#include "../version.h"
#include "../crypto/ShaResourcePolicy.h"

namespace {

constexpr uint32_t CONNECT_TIMEOUT_MS = 4000;
constexpr uint16_t READ_TIMEOUT_MS = 5000;
// The current multi-certificate P-384 chain exceeds five seconds on classic
// ESP32. Keep a bounded handshake deadline without weakening verification.
constexpr uint32_t TLS_HANDSHAKE_TIMEOUT_SECONDS = 15;
constexpr int MAX_RESPONSE_BYTES = 16384;
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
  LimitedStream(Stream &source, size_t limit)
      : source_(source), remaining_(limit), exhausted_(false) {
    setTimeout(READ_TIMEOUT_MS);
  }

  int available() override {
    const int sourceAvailable = source_.available();
    if (remaining_ == 0) return 0;
    return sourceAvailable > static_cast<int>(remaining_)
               ? static_cast<int>(remaining_)
               : sourceAvailable;
  }

  int read() override {
    if (remaining_ == 0) {
      exhausted_ = true;
      return -1;
    }
    const uint32_t started = millis();
    while (source_.available() == 0) {
      if (millis() - started >= READ_TIMEOUT_MS) return -1;
      delay(1);
    }
    const int value = source_.read();
    if (value >= 0) remaining_--;
    return value;
  }

  int peek() override {
    if (remaining_ == 0) return -1;
    return source_.peek();
  }

  void flush() override { source_.flush(); }
  size_t write(uint8_t) override { return 0; }
  bool exhausted() const { return exhausted_; }

 private:
  Stream &source_;
  size_t remaining_;
  bool exhausted_;
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

  StaticJsonDocument<256> filter;
  filter["stats"][0]["bestEver"] = true;
  filter["stats"][0]["hashrate5m"] = true;
  filter["workers"][0]["lastUpdate"] = true;

  StaticJsonDocument<4096> document;
  LimitedStream stream(*http.getStreamPtr(), MAX_RESPONSE_BYTES);
  const DeserializationError error = deserializeJson(
      document, stream, DeserializationOption::Filter(filter));
  if (error || stream.exhausted() || document.overflowed() ||
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
  // Declared first so this guard outlives all HTTP/TLS cleanup on every return.
  SecureTransportWork transportWork(secure);
  HTTPClient http;
  http.setConnectTimeout(CONNECT_TIMEOUT_MS);
  http.setTimeout(READ_TIMEOUT_MS);
  http.setReuse(false);
  http.setUserAgent(USER_AGENT);
  http.useHTTP10(true);

  const char *headerKeys[] = {"Date", "Last-Modified"};
  http.collectHeaders(headerKeys, 2);

  WiFiClient plainClient;
  WiFiClientSecure secureClient;
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

  const bool helios = responseFormat == PoolResponseFormat::Helios;
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
    http.end();
    return classifyPoolHttpStatus(status);
  }

  const int responseSize = http.getSize();
  if (responseSize > MAX_RESPONSE_BYTES) {
    http.end();
    return {PoolFetchStatus::InvalidResponse, static_cast<int16_t>(status)};
  }

  PoolFetchResult result = helios ? parseHeliosResponse(http, snapshot)
                                  : parsePublicPoolResponse(http, snapshot);
  if (result.status == PoolFetchStatus::Success && helios &&
      lastModified != nullptr && lastModifiedSize > 0) {
    const String value = http.header("Last-Modified");
    if (value.length() > 0) {
      snprintf(lastModified, lastModifiedSize, "%s", value.c_str());
    }
  }
  http.end();
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
