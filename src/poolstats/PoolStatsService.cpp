#include "PoolStatsService.h"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#include "PoolRegistry.h"
#include "PoolStatsPolicy.h"
#include "PoolStatsProvider.h"

namespace {

constexpr uint32_t WIFI_RECHECK_MS = 5UL * 1000UL;
constexpr uint32_t MIN_FREE_HEAP = 45000;
constexpr uint32_t MIN_LARGEST_HEAP_BLOCK = 24000;
constexpr uint32_t SERVICE_TASK_STACK = 10240;
constexpr UBaseType_t SERVICE_TASK_PRIORITY = 1;

portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
PoolIdentity identity{};
PoolStatsSnapshot snapshot{};
TaskHandle_t serviceTaskHandle = nullptr;
char lastModified[48] = {};
uint32_t nextAttemptMs = 0;

void copyText(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) return;
  snprintf(destination, destinationSize, "%s", source == nullptr ? "" : source);
}

void setUnavailableValues(PoolStatsSnapshot &value) {
  copyText(value.bestDifficulty, sizeof(value.bestDifficulty), "N/A");
  copyText(value.workersCount, sizeof(value.workersCount), "N/A");
  copyText(value.totalHashRate, sizeof(value.totalHashRate), "N/A");
  value.hasData = false;
}

bool timeReached(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

void publishSnapshot(PoolStatsSnapshot value) {
  portENTER_CRITICAL(&snapshotMux);
  value.revision = snapshot.revision + 1;
  snapshot = value;
  portEXIT_CRITICAL(&snapshotMux);
}

void refreshStats(uint32_t now) {
  PoolStatsProvider *provider = providerFor(identity.definition.provider);
  if (provider == nullptr) return;

  PoolStatsSnapshot current = getPoolStatsSnapshot();
  current.lastAttemptMs = now;
  publishSnapshot(current);

  if (ESP.getFreeHeap() < MIN_FREE_HEAP ||
      ESP.getMaxAllocHeap() < MIN_LARGEST_HEAP_BLOCK) {
    const PoolUpdateDecision decision = updatePoolSnapshotAfterFetch(
        getPoolStatsSnapshot(), current,
        {PoolFetchStatus::RetryableError, 0}, now);
    publishSnapshot(decision.snapshot);
    nextAttemptMs = now + decision.nextDelayMs;
    return;
  }

  PoolStatsSnapshot candidate = current;
  const PoolFetchResult result =
      provider->fetch(identity, candidate, lastModified, sizeof(lastModified));

  const PoolUpdateDecision decision =
      updatePoolSnapshotAfterFetch(current, candidate, result, now);
  publishSnapshot(decision.snapshot);
  nextAttemptMs = now + decision.nextDelayMs;
}

void poolStatsTask(void *) {
  for (;;) {
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      PoolStatsSnapshot current = getPoolStatsSnapshot();
      const PoolMetricState expected =
          current.hasData ? PoolMetricState::Stale : PoolMetricState::Error;
      if (current.state != expected) {
        current.state = expected;
        publishSnapshot(current);
      }
      nextAttemptMs = now + WIFI_RECHECK_MS;
    } else if (timeReached(now, nextAttemptMs)) {
      refreshStats(now);
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

}  // namespace

void beginPoolStatsService(const char *host, uint16_t port,
                           const char *username) {
  if (serviceTaskHandle != nullptr) return;

  identity.definition = resolvePoolDefinition(host, port);
  extractPoolWallet(username, identity.wallet, sizeof(identity.wallet));

  PoolStatsSnapshot initial{};
  copyText(initial.poolName, sizeof(initial.poolName),
           identity.definition.displayName);
  setUnavailableValues(initial);
  initial.state = PoolMetricState::Unavailable;
  initial.revision = 1;

  if (identity.definition.provider == PoolProviderKind::Testnet) {
    copyText(initial.bestDifficulty, sizeof(initial.bestDifficulty), "TESTNET");
    copyText(initial.workersCount, sizeof(initial.workersCount), "1");
    copyText(initial.totalHashRate, sizeof(initial.totalHashRate), "TESTNET");
    initial.state = PoolMetricState::Current;
    initial.hasData = true;
  } else if (providerFor(identity.definition.provider) != nullptr) {
    initial.state = PoolMetricState::Loading;
  }
  publishSnapshot(initial);

  if (providerFor(identity.definition.provider) == nullptr) return;

#if CONFIG_FREERTOS_UNICORE
  const BaseType_t created =
      xTaskCreate(poolStatsTask, "PoolStats", SERVICE_TASK_STACK, nullptr,
                  SERVICE_TASK_PRIORITY, &serviceTaskHandle);
#else
  const BaseType_t created = xTaskCreatePinnedToCore(
      poolStatsTask, "PoolStats", SERVICE_TASK_STACK, nullptr,
      SERVICE_TASK_PRIORITY, &serviceTaskHandle, 1);
#endif
  if (created != pdPASS) {
    serviceTaskHandle = nullptr;
    PoolStatsSnapshot failed = getPoolStatsSnapshot();
    failed.state = PoolMetricState::Error;
    publishSnapshot(failed);
  }
}

PoolStatsSnapshot getPoolStatsSnapshot() {
  PoolStatsSnapshot copy;
  portENTER_CRITICAL(&snapshotMux);
  copy = snapshot;
  portEXIT_CRITICAL(&snapshotMux);

  if (copy.hasData && copy.state == PoolMetricState::Current &&
      copy.lastSuccessMs != 0 &&
      millis() - copy.lastSuccessMs > POOL_STATS_STALE_AFTER_MS) {
    copy.state = PoolMetricState::Stale;
  }
  return copy;
}

const char *poolMetricStateLabel(PoolMetricState state) {
  switch (state) {
    case PoolMetricState::Loading:
      return "WAIT";
    case PoolMetricState::Stale:
      return "STALE";
    case PoolMetricState::Error:
      return "ERROR";
    case PoolMetricState::Unavailable:
      return "N/A";
    default:
      return "";
  }
}
