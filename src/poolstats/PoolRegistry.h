#ifndef POOL_REGISTRY_H
#define POOL_REGISTRY_H

#include "PoolStatsTypes.h"

void normalizePoolHost(const char *input, char *output, size_t outputSize);
void makePoolDisplayName(const char *host, char *output, size_t outputSize);
PoolDefinition resolvePoolDefinition(const char *host, uint16_t port);
void extractPoolWallet(const char *username, char *output, size_t outputSize);

#endif
