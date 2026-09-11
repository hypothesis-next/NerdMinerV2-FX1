#include "ReferenceSha256.h"

#include <string.h>

namespace mining_validation {
namespace {

struct Sha256Context {
  uint32_t state[8];
  uint64_t bytes;
  uint8_t block[64];
  size_t used;
};

static const uint32_t kRound[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotateRight(uint32_t value, unsigned bits) {
  return (value >> bits) | (value << (32U - bits));
}

inline uint32_t loadBigEndian(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

void transform(Sha256Context &ctx, const uint8_t block[64]) {
  uint32_t w[64];
  for (size_t i = 0; i < 16; ++i) w[i] = loadBigEndian(block + i * 4);
  for (size_t i = 16; i < 64; ++i) {
    const uint32_t s0 = rotateRight(w[i - 15], 7) ^
                        rotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const uint32_t s1 = rotateRight(w[i - 2], 17) ^
                        rotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = ctx.state[0], b = ctx.state[1], c = ctx.state[2];
  uint32_t d = ctx.state[3], e = ctx.state[4], f = ctx.state[5];
  uint32_t g = ctx.state[6], h = ctx.state[7];
  for (size_t i = 0; i < 64; ++i) {
    const uint32_t sum1 = rotateRight(e, 6) ^ rotateRight(e, 11) ^
                          rotateRight(e, 25);
    const uint32_t choose = (e & f) ^ (~e & g);
    const uint32_t temp1 = h + sum1 + choose + kRound[i] + w[i];
    const uint32_t sum0 = rotateRight(a, 2) ^ rotateRight(a, 13) ^
                          rotateRight(a, 22);
    const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
    const uint32_t temp2 = sum0 + majority;
    h = g; g = f; f = e; e = d + temp1;
    d = c; c = b; b = a; a = temp1 + temp2;
  }
  ctx.state[0] += a; ctx.state[1] += b; ctx.state[2] += c;
  ctx.state[3] += d; ctx.state[4] += e; ctx.state[5] += f;
  ctx.state[6] += g; ctx.state[7] += h;
}

void init(Sha256Context &ctx) {
  static const uint32_t initial[8] = {
      0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  memcpy(ctx.state, initial, sizeof(initial));
  ctx.bytes = 0;
  ctx.used = 0;
}

void update(Sha256Context &ctx, const uint8_t *data, size_t length) {
  ctx.bytes += length;
  while (length != 0) {
    const size_t available = sizeof(ctx.block) - ctx.used;
    const size_t take = length < available ? length : available;
    memcpy(ctx.block + ctx.used, data, take);
    ctx.used += take;
    data += take;
    length -= take;
    if (ctx.used == sizeof(ctx.block)) {
      transform(ctx, ctx.block);
      ctx.used = 0;
    }
  }
}

void finish(Sha256Context &ctx, uint8_t digest[32]) {
  const uint64_t bits = ctx.bytes * 8U;
  ctx.block[ctx.used++] = 0x80;
  if (ctx.used > 56) {
    memset(ctx.block + ctx.used, 0, sizeof(ctx.block) - ctx.used);
    transform(ctx, ctx.block);
    ctx.used = 0;
  }
  memset(ctx.block + ctx.used, 0, 56 - ctx.used);
  for (size_t i = 0; i < 8; ++i)
    ctx.block[63 - i] = static_cast<uint8_t>(bits >> (i * 8));
  transform(ctx, ctx.block);
  for (size_t i = 0; i < 8; ++i) {
    digest[i * 4] = static_cast<uint8_t>(ctx.state[i] >> 24);
    digest[i * 4 + 1] = static_cast<uint8_t>(ctx.state[i] >> 16);
    digest[i * 4 + 2] = static_cast<uint8_t>(ctx.state[i] >> 8);
    digest[i * 4 + 3] = static_cast<uint8_t>(ctx.state[i]);
  }
}

}  // namespace

void referenceSha256(const uint8_t *data, size_t length, uint8_t digest[32]) {
  Sha256Context ctx;
  init(ctx);
  update(ctx, data, length);
  finish(ctx, digest);
}

void referenceSha256d(const uint8_t *data, size_t length, uint8_t digest[32]) {
  uint8_t first[32];
  referenceSha256(data, length, first);
  referenceSha256(first, sizeof(first), digest);
}

bool hashMeetsTarget(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 31; i >= 0; --i) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

CandidateValidationResult validateCandidate(
    uint32_t candidateGeneration, uint32_t currentGeneration,
    const uint8_t header[80], const uint8_t optimizedHash[32],
    const uint8_t networkTarget[32], bool *meetsNetworkTarget) {
  if (candidateGeneration != currentGeneration)
    return CandidateValidationResult::StaleGeneration;

  uint8_t reference[32];
  referenceSha256d(header, 80, reference);
  if (memcmp(reference, optimizedHash, sizeof(reference)) != 0)
    return CandidateValidationResult::HashMismatch;

  if (meetsNetworkTarget != nullptr)
    *meetsNetworkTarget = hashMeetsTarget(reference, networkTarget);
  return CandidateValidationResult::Valid;
}

}  // namespace mining_validation
