#ifndef POOL_STATS_PROVIDER_H
#define POOL_STATS_PROVIDER_H

#include "PoolStatsTypes.h"

class PoolStatsProvider {
 public:
  virtual ~PoolStatsProvider() = default;
  virtual PoolFetchResult fetch(const PoolIdentity &identity,
                                PoolStatsSnapshot &snapshot,
                                char *lastModified,
                                size_t lastModifiedSize) = 0;
};

PoolStatsProvider *providerFor(PoolProviderKind provider);
// Statistics-task-only: prepare a warm request without allowing a surprise
// cold handshake while the display still owns its rendering allocation.
bool poolStatsHasReusableTransport();

#endif
