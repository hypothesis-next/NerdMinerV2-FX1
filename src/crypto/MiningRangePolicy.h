#pragma once
#include <stdint.h>
#include <stdio.h>

namespace mining_validation {
inline bool requiresFullDigest(double difficulty, const uint8_t networkTarget[32]) {
  // Conservatively require the complete digest below the 16-zero-bit filter
  // domain. Bitcoin difficulty-one target is strictly below 2^224.
  return !(difficulty >= 1.0 / 65536.0) || networkTarget[31] != 0 || networkTarget[30] != 0;
}
inline bool candidateEligible(bool networkBlock, double difficulty, double required) {
  return networkBlock || difficulty >= required;
}
inline void accumulateCompleted(uint32_t &millions, uint32_t &remainder, uint32_t count) {
  const uint64_t total = static_cast<uint64_t>(remainder) + count;
  millions += static_cast<uint32_t>(total / 1000000U);
  remainder = static_cast<uint32_t>(total % 1000000U);
}
inline void formatSubmitNonce(char (&text)[9], uint32_t nonce) {
  snprintf(text, sizeof(text), "%08lx", static_cast<unsigned long>(nonce));
}
// A worker retains the unprocessed suffix of its exclusively owned range.
template <typename Job>
bool resumeCompletedPrefix(Job &job, uint32_t completed, uint32_t generation) {
  if (job.generation != generation || completed == 0 || completed >= job.nonce_count)
    return false;
  job.nonce_start += completed;  // Defined modulo-2^32 nonce arithmetic.
  job.nonce_count -= completed;
  return true;
}
}  // namespace mining_validation
