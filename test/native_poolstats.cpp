#include <ArduinoJson.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <string.h>

#include "poolstats/PoolRegistry.h"
#include "poolstats/PoolStatsParsers.h"
#include "poolstats/PoolStatsPolicy.h"

namespace {

int failures = 0;

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    failures++;
  }
}

std::string readFixture(const std::string &directory, const char *name) {
  std::ifstream input(directory + "/" + name, std::ios::binary);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

void testRegistry() {
  char host[POOL_HOST_MAX_LENGTH + 1] = {};
  normalizePoolHost("  STRATUM+TCP://BTC.HELIOSPOOL.COM:3333/path  ", host,
                    sizeof(host));
  check(strcmp(host, "btc.heliospool.com") == 0, "hostname normalization");

  struct RegistryCase {
    const char *host;
    uint16_t port;
    PoolProviderKind provider;
    const char *name;
  };
  const RegistryCase cases[] = {
      {"public-pool.io", 21496, PoolProviderKind::PublicPoolCompatible,
       "Public Pool"},
      {"btc.heliospool.com", 3333, PoolProviderKind::HeliosPool,
       "HeliosPool"},
      {"pool.nerdminers.org", 3333,
       PoolProviderKind::PublicPoolCompatible, "NerdMiner Pool"},
      {"pool.sethforprivacy.com", 3333,
       PoolProviderKind::PublicPoolCompatible, "Seth for Privacy"},
      {"pool.solomining.de", 3333,
       PoolProviderKind::PublicPoolCompatible, "SoloMining.de"},
      {"tn.vkbit.com", 3333, PoolProviderKind::Testnet, "TESTNET"},
      {"solo.example.com", 2018, PoolProviderKind::None,
       "solo.example.com"},
  };
  for (const RegistryCase &item : cases) {
    const PoolDefinition definition = resolvePoolDefinition(item.host, item.port);
    check(definition.provider == item.provider, item.host);
    check(strcmp(definition.displayName, item.name) == 0, item.name);
    if (item.provider == PoolProviderKind::None) {
      check(definition.apiBaseUrl[0] == '\0', "unknown pool has no API URL");
    }
  }

  const PoolDefinition longName = resolvePoolDefinition(
      "this-is-a-very-long-custom-pool-hostname.example.com", 3333);
  check(strlen(longName.displayName) == POOL_NAME_MAX_LENGTH,
        "long display name length");
  check(strcmp(longName.displayName + POOL_NAME_MAX_LENGTH - 3, "...") == 0,
        "long display name ellipsis");

  char wallet[POOL_WALLET_MAX_LENGTH + 1] = {};
  extractPoolWallet("bc1qsanitizedtestaddress.worker-01", wallet,
                    sizeof(wallet));
  check(strcmp(wallet, "bc1qsanitizedtestaddress") == 0,
        "worker suffix removal");
}

void testParsers(const std::string &fixtures) {
  DynamicJsonDocument publicDocument(2048);
  DeserializationError error = deserializeJson(
      publicDocument, readFixture(fixtures, "public_pool.json"));
  check(!error, "Public Pool fixture deserializes");
  PoolStatsSnapshot publicSnapshot{};
  check(parsePublicPoolDocument(publicDocument.as<JsonVariantConst>(),
                                publicSnapshot),
        "Public Pool fixture parses");
  check(strcmp(publicSnapshot.bestDifficulty, "1.23K") == 0,
        "Public Pool best difficulty");
  check(strcmp(publicSnapshot.workersCount, "2") == 0,
        "Public Pool worker count");
  check(strcmp(publicSnapshot.totalHashRate, "3.50K") == 0,
        "Public Pool aggregate hashrate");

  uint64_t serverEpoch = 0;
  check(parseHttpDateUtc("Thu, 10 Sep 2026 12:00:00 GMT", serverEpoch),
        "HTTP Date parser");
  DynamicJsonDocument heliosDocument(4096);
  error = deserializeJson(heliosDocument,
                          readFixture(fixtures, "helios_snapshot.json"));
  check(!error, "Helios fixture deserializes");
  check(heliosDocument["stats"][0]["bestEver"].is<double>(),
        "Helios bestEver is numeric");
  check(heliosDocument["stats"][0]["hashrate5m"].is<double>(),
        "Helios hashrate5m is numeric");
  JsonVariantConst heliosRoot = heliosDocument.as<JsonVariantConst>();
  check(heliosRoot["stats"].is<JsonArrayConst>(), "Helios stats is an array");
  check(heliosRoot["workers"].is<JsonArrayConst>(),
        "Helios workers is an array");
  for (JsonObjectConst worker : heliosRoot["workers"].as<JsonArrayConst>()) {
    const char *lastUpdate = worker["lastUpdate"].as<const char *>();
    uint64_t parsedWorkerEpoch = 0;
    check(lastUpdate != nullptr, "Helios worker timestamp exists");
    check(parseIso8601Utc(lastUpdate, parsedWorkerEpoch),
          "Helios fixture worker timestamp parses");
  }
  uint64_t workerEpoch = 0;
  check(parseIso8601Utc("2026-09-10T11:55:00.000Z", workerEpoch),
        "fractional Helios worker timestamp parses");
  check(parseIso8601Utc("2026-09-09T12:00:00Z", workerEpoch),
        "whole-second Helios worker timestamp parses");
  check(parseIso8601Utc("2026-09-09T11:59:59Z", workerEpoch),
        "old Helios worker timestamp parses");
  check(workerEpoch < serverEpoch, "worker timestamp precedes server Date");
  PoolStatsSnapshot heliosSnapshot{};
  check(parseHeliosSnapshotDocument(heliosRoot, serverEpoch, heliosSnapshot),
        "Helios fixture parses");
  check(strcmp(heliosSnapshot.bestDifficulty, "7.65M") == 0,
        "Helios bestEver mapping");
  check(strcmp(heliosSnapshot.workersCount, "2") == 0,
        "Helios 24-hour active workers");
  check(strcmp(heliosSnapshot.totalHashRate, "123K") == 0,
        "Helios hashrate5m mapping");

  DynamicJsonDocument missingDocument(1024);
  error = deserializeJson(missingDocument,
                          readFixture(fixtures, "missing_fields.json"));
  check(!error, "missing-field fixture deserializes");
  PoolStatsSnapshot missingSnapshot{};
  check(!parseHeliosSnapshotDocument(missingDocument.as<JsonVariantConst>(),
                                     serverEpoch, missingSnapshot),
        "missing field is rejected");

  DynamicJsonDocument malformedDocument(1024);
  error = deserializeJson(malformedDocument,
                          readFixture(fixtures, "malformed.json"));
  check(error.code() != DeserializationError::Ok,
        "malformed JSON is rejected");

  uint64_t ignored = 0;
  check(!parseIso8601Utc("2026-02-30T12:00:00Z", ignored),
        "invalid calendar date is rejected");
  check(!parseIso8601Utc("2026-09-10T12:00:00+02:00", ignored),
        "non-UTC worker timestamp is rejected");
  check(parseIso8601Utc("2024-02-29T12:00:00.123Z", ignored),
        "UTC leap-day timestamp parses");
}

void testPolicy() {
  check(poolStatsClockAction(false, 0, false) == PoolClockAction::Fetch,
        "non-TLS provider does not require system time");
  check(poolStatsClockAction(true, 0, false) == PoolClockAction::StartSync,
        "invalid TLS clock starts synchronization");
  check(poolStatsClockAction(true, 0, true) == PoolClockAction::Wait,
        "invalid TLS clock waits after synchronization starts");
  check(poolStatsClockAction(true, POOL_STATS_MIN_VALID_TLS_EPOCH - 1, true) ==
            PoolClockAction::Wait,
        "TLS clock rejects the lower-bound predecessor");
  check(poolStatsClockAction(true, POOL_STATS_MIN_VALID_TLS_EPOCH, true) ==
            PoolClockAction::Fetch,
        "TLS clock transition permits fetch at valid UTC time");

  check(classifyPoolHttpStatus(200).status == PoolFetchStatus::Success,
        "HTTP 200 classification");
  check(classifyPoolHttpStatus(304).status == PoolFetchStatus::NotModified,
        "HTTP 304 classification");
  check(classifyPoolHttpStatus(400).status == PoolFetchStatus::Unavailable,
        "HTTP 400 classification");
  check(classifyPoolHttpStatus(404).status == PoolFetchStatus::Unavailable,
        "HTTP 404 classification");
  check(classifyPoolHttpStatus(429).status == PoolFetchStatus::RetryableError,
        "HTTP 429 classification");
  check(classifyPoolHttpStatus(500).status == PoolFetchStatus::RetryableError,
        "HTTP 500 classification");
  check(classifyPoolHttpStatus(503).status == PoolFetchStatus::RetryableError,
        "HTTP 503 classification");
  check(classifyPoolHttpStatus(-11).status == PoolFetchStatus::RetryableError,
        "timeout classification");

  PoolStatsSnapshot empty{};
  PoolStatsSnapshot candidate{};
  snprintf(candidate.bestDifficulty, sizeof(candidate.bestDifficulty), "42.0K");
  snprintf(candidate.workersCount, sizeof(candidate.workersCount), "3");
  snprintf(candidate.totalHashRate, sizeof(candidate.totalHashRate), "150K");
  PoolUpdateDecision update = updatePoolSnapshotAfterFetch(
      empty, candidate, {PoolFetchStatus::Success, 200}, 1000);
  check(update.snapshot.state == PoolMetricState::Current,
        "successful update is current");
  check(update.snapshot.lastAttemptMs == 1000 &&
            update.snapshot.lastSuccessMs == 1000,
        "attempt and success timestamps tracked separately");
  check(update.nextDelayMs == 60000, "successful refresh interval");

  PoolStatsSnapshot cached = update.snapshot;
  update = updatePoolSnapshotAfterFetch(
      cached, cached, {PoolFetchStatus::RetryableError, 500}, 2000);
  check(update.snapshot.state == PoolMetricState::Stale &&
            update.snapshot.hasData,
        "server error preserves last-good data as stale");
  check(strcmp(update.snapshot.bestDifficulty, "42.0K") == 0,
        "server error preserves cached values");
  check(update.snapshot.lastSuccessMs == 1000 &&
            update.snapshot.lastAttemptMs == 2000,
        "failure does not overwrite last success");
  check(update.nextDelayMs == 15000, "first failure backoff");

  PoolStatsSnapshot failedOnce = update.snapshot;
  update = updatePoolSnapshotAfterFetch(
      failedOnce, failedOnce, {PoolFetchStatus::InvalidResponse, 200}, 3000);
  check(update.snapshot.state == PoolMetricState::Stale,
        "malformed response marks cache stale");
  check(update.nextDelayMs == 30000, "second failure backoff");

  update = updatePoolSnapshotAfterFetch(
      cached, cached, {PoolFetchStatus::RetryableError, 429}, 4000);
  check(update.nextDelayMs == 300000, "rate-limit backoff");

  update = updatePoolSnapshotAfterFetch(
      cached, cached, {PoolFetchStatus::NotModified, 304}, 5000);
  check(update.snapshot.state == PoolMetricState::Current &&
            update.snapshot.lastSuccessMs == 5000,
        "304 refreshes a cached snapshot");

  update = updatePoolSnapshotAfterFetch(
      empty, empty, {PoolFetchStatus::NotModified, 304}, 6000);
  check(update.snapshot.state == PoolMetricState::Error &&
            !update.snapshot.hasData,
        "304 without a cache is an error");

  update = updatePoolSnapshotAfterFetch(
      cached, cached, {PoolFetchStatus::Unavailable, 404}, 7000);
  check(update.snapshot.state == PoolMetricState::Unavailable &&
            !update.snapshot.hasData &&
            strcmp(update.snapshot.workersCount, "N/A") == 0,
        "404 produces explicit unavailable values");

  check(poolStatsFailureBackoffMs(1, 0) == 15000,
        "DNS/connect first backoff");
  check(poolStatsFailureBackoffMs(6, 0) == 300000,
        "backoff is capped");
}

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: native_poolstats <fixture-directory>\n";
    return 2;
  }
  testRegistry();
  testParsers(argv[1]);
  testPolicy();
  if (failures != 0) {
    std::cerr << failures << " test(s) failed\n";
    return 1;
  }
  std::cout << "All pool statistics tests passed\n";
  return 0;
}
