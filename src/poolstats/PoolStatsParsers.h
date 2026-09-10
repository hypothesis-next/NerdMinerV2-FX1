#ifndef POOL_STATS_PARSERS_H
#define POOL_STATS_PARSERS_H

#include <ArduinoJson.h>

#include "PoolStatsTypes.h"

bool parsePublicPoolDocument(JsonVariantConst root,
                             PoolStatsSnapshot &snapshot);
bool parseHeliosSnapshotDocument(JsonVariantConst root, uint64_t serverEpoch,
                                 PoolStatsSnapshot &snapshot);
bool parseHttpDateUtc(const char *value, uint64_t &epoch);
bool parseIso8601Utc(const char *value, uint64_t &epoch);
void formatPoolNumber(double value, char *output, size_t outputSize);

#endif
