#ifndef POOL_STATS_TYPES_H
#define POOL_STATS_TYPES_H

#include <stddef.h>
#include <stdint.h>

constexpr size_t POOL_HOST_MAX_LENGTH = 80;
constexpr size_t POOL_NAME_MAX_LENGTH = 32;
constexpr size_t POOL_VALUE_MAX_LENGTH = 16;
constexpr size_t POOL_WALLET_MAX_LENGTH = 80;
constexpr size_t POOL_API_URL_MAX_LENGTH = 160;

enum class PoolProviderKind : uint8_t {
  None,
  PublicPoolCompatible,
  HeliosPool,
  Testnet
};

enum class PoolMetricState : uint8_t {
  Unavailable,
  Loading,
  Current,
  Stale,
  Error
};

struct PoolDefinition {
  PoolProviderKind provider;
  char normalizedHost[POOL_HOST_MAX_LENGTH + 1];
  char displayName[POOL_NAME_MAX_LENGTH + 1];
  char apiBaseUrl[POOL_API_URL_MAX_LENGTH + 1];
  uint16_t stratumPort;
};

struct PoolIdentity {
  PoolDefinition definition;
  char wallet[POOL_WALLET_MAX_LENGTH + 1];
};

struct PoolStatsSnapshot {
  char poolName[POOL_NAME_MAX_LENGTH + 1];
  char bestDifficulty[POOL_VALUE_MAX_LENGTH + 1];
  char workersCount[POOL_VALUE_MAX_LENGTH + 1];
  char totalHashRate[POOL_VALUE_MAX_LENGTH + 1];
  PoolMetricState state;
  uint32_t lastAttemptMs;
  uint32_t lastSuccessMs;
  int16_t httpStatus;
  uint8_t consecutiveFailures;
  bool hasData;
  uint32_t revision;
};

enum class PoolFetchStatus : uint8_t {
  Success,
  NotModified,
  Unavailable,
  RetryableError,
  InvalidResponse
};

struct PoolFetchResult {
  PoolFetchStatus status;
  int16_t httpStatus;
};

#endif
