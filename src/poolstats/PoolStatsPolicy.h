#ifndef POOL_STATS_POLICY_H
#define POOL_STATS_POLICY_H

#include "PoolStatsTypes.h"

constexpr uint32_t POOL_STATS_SUCCESS_REFRESH_MS = 60UL * 1000UL;
constexpr uint32_t POOL_STATS_STALE_AFTER_MS =
    3UL * POOL_STATS_SUCCESS_REFRESH_MS;

struct PoolUpdateDecision {
  PoolStatsSnapshot snapshot;
  uint32_t nextDelayMs;
};

PoolFetchResult classifyPoolHttpStatus(int status);
uint32_t poolStatsFailureBackoffMs(uint8_t failures, int16_t httpStatus);
PoolUpdateDecision updatePoolSnapshotAfterFetch(
    const PoolStatsSnapshot &current, const PoolStatsSnapshot &candidate,
    PoolFetchResult result, uint32_t now);

#endif
