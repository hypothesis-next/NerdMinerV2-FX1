#ifndef REFERENCE_SHA256_H
#define REFERENCE_SHA256_H

#include <stddef.h>
#include <stdint.h>

namespace mining_validation {

// Intentionally small and independent from the optimized mining SHA path.
void referenceSha256(const uint8_t *data, size_t length, uint8_t digest[32]);
void referenceSha256d(const uint8_t *data, size_t length, uint8_t digest[32]);

// Both operands use NerdMiner's little-endian 256-bit integer representation.
bool hashMeetsTarget(const uint8_t hash[32], const uint8_t target[32]);

enum class CandidateValidationResult : uint8_t {
  Valid,
  StaleGeneration,
  HashMismatch
};

CandidateValidationResult validateCandidate(
    uint32_t candidateGeneration, uint32_t currentGeneration,
    const uint8_t header[80], const uint8_t optimizedHash[32],
    const uint8_t networkTarget[32], bool *meetsNetworkTarget);

}  // namespace mining_validation

#endif
