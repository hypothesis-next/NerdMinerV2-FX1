#include "PoolStatsPolicy.h"

#include <stdio.h>

namespace {

constexpr uint32_t FAILURE_BACKOFF_INITIAL_MS = 15UL * 1000UL;
constexpr uint32_t FAILURE_BACKOFF_MAX_MS = 5UL * 60UL * 1000UL;

void setUnavailableValues(PoolStatsSnapshot &snapshot) {
  snprintf(snapshot.bestDifficulty, sizeof(snapshot.bestDifficulty), "N/A");
  snprintf(snapshot.workersCount, sizeof(snapshot.workersCount), "N/A");
  snprintf(snapshot.totalHashRate, sizeof(snapshot.totalHashRate), "N/A");
  snapshot.hasData = false;
}

}  // namespace

PoolFetchResult classifyPoolHttpStatus(int status) {
  switch (status) {
    case 200:
      return {PoolFetchStatus::Success, 200};
    case 304:
      return {PoolFetchStatus::NotModified, 304};
    case 400:
    case 404:
      return {PoolFetchStatus::Unavailable, static_cast<int16_t>(status)};
    default:
      return {PoolFetchStatus::RetryableError, static_cast<int16_t>(status)};
  }
}

PoolClockAction poolStatsClockAction(bool requiresTls, int64_t epoch,
                                     bool syncStarted) {
  if (!requiresTls || epoch >= POOL_STATS_MIN_VALID_TLS_EPOCH) {
    return PoolClockAction::Fetch;
  }
  return syncStarted ? PoolClockAction::Wait : PoolClockAction::StartSync;
}

uint32_t poolStatsFailureBackoffMs(uint8_t failures, int16_t httpStatus) {
  if (httpStatus == 429) return FAILURE_BACKOFF_MAX_MS;

  uint32_t delayMs = FAILURE_BACKOFF_INITIAL_MS;
  const uint8_t shifts = failures > 1 ? failures - 1 : 0;
  for (uint8_t i = 0; i < shifts && delayMs < FAILURE_BACKOFF_MAX_MS; i++) {
    const uint32_t doubled = delayMs * 2U;
    delayMs = doubled < FAILURE_BACKOFF_MAX_MS ? doubled
                                               : FAILURE_BACKOFF_MAX_MS;
  }
  return delayMs;
}

PoolUpdateDecision updatePoolSnapshotAfterFetch(
    const PoolStatsSnapshot &current, const PoolStatsSnapshot &candidate,
    PoolFetchResult result, uint32_t now) {
  PoolUpdateDecision decision{};
  decision.snapshot = current;

  if (result.status == PoolFetchStatus::Success) {
    decision.snapshot = candidate;
    decision.snapshot.hasData = true;
    decision.snapshot.state = PoolMetricState::Current;
    decision.snapshot.lastAttemptMs = now;
    decision.snapshot.lastSuccessMs = now;
    decision.snapshot.httpStatus = result.httpStatus;
    decision.snapshot.consecutiveFailures = 0;
    decision.nextDelayMs = POOL_STATS_SUCCESS_REFRESH_MS;
    return decision;
  }

  if (result.status == PoolFetchStatus::NotModified && current.hasData) {
    decision.snapshot.state = PoolMetricState::Current;
    decision.snapshot.lastAttemptMs = now;
    decision.snapshot.lastSuccessMs = now;
    decision.snapshot.httpStatus = result.httpStatus;
    decision.snapshot.consecutiveFailures = 0;
    decision.nextDelayMs = POOL_STATS_SUCCESS_REFRESH_MS;
    return decision;
  }

  decision.snapshot.lastAttemptMs = now;
  decision.snapshot.httpStatus = result.httpStatus;
  if (decision.snapshot.consecutiveFailures < UINT8_MAX) {
    decision.snapshot.consecutiveFailures++;
  }

  if (result.status == PoolFetchStatus::Unavailable) {
    setUnavailableValues(decision.snapshot);
    decision.snapshot.state = PoolMetricState::Unavailable;
  } else if (decision.snapshot.hasData) {
    decision.snapshot.state = PoolMetricState::Stale;
  } else {
    setUnavailableValues(decision.snapshot);
    decision.snapshot.state = PoolMetricState::Error;
  }

  decision.nextDelayMs = poolStatsFailureBackoffMs(
      decision.snapshot.consecutiveFailures, result.httpStatus);
  return decision;
}
