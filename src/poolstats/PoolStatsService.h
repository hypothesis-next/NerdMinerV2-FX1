#ifndef POOL_STATS_SERVICE_H
#define POOL_STATS_SERVICE_H

#include "PoolStatsTypes.h"

void beginPoolStatsService(const char *host, uint16_t port,
                           const char *username);
PoolStatsSnapshot getPoolStatsSnapshot();
const char *poolMetricStateLabel(PoolMetricState state);

#endif
