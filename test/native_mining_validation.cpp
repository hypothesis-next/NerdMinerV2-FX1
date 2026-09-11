#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>

#include "../src/crypto/ReferenceSha256.h"
#include "../src/ShaTests/nerdSHA256plus.h"

namespace {
void require(bool condition, const char *message) {
  if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

bool decodeHex(const char *hex, uint8_t *out, size_t length) {
  for (size_t i = 0; i < length; ++i) {
    unsigned value = 0;
    if (sscanf(hex + i * 2, "%2x", &value) != 1) return false;
    out[i] = static_cast<uint8_t>(value);
  }
  return true;
}

void requireDigest(const uint8_t *data, size_t length, const char *expected) {
  uint8_t actual[32], wanted[32];
  require(decodeHex(expected, wanted, 32), "invalid test vector");
  mining_validation::referenceSha256(data, length, actual);
  require(memcmp(actual, wanted, 32) == 0, "SHA-256 vector mismatch");
}

void requireDoubleDigest(const char *headerHex, const char *expected) {
  uint8_t header[80], actual[32], wanted[32];
  require(decodeHex(headerHex, header, sizeof(header)), "invalid block header vector");
  require(decodeHex(expected, wanted, sizeof(wanted)), "invalid block hash vector");
  mining_validation::referenceSha256d(header, sizeof(header), actual);
  require(memcmp(actual, wanted, sizeof(actual)) == 0, "historical block hash mismatch");
}

void testReferenceVectors() {
  requireDigest(reinterpret_cast<const uint8_t *>(""), 0,
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  requireDigest(reinterpret_cast<const uint8_t *>("abc"), 3,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  uint8_t actual[32], wanted[32];
  mining_validation::referenceSha256d(nullptr, 0, actual);
  require(decodeHex("5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456", wanted, 32), "invalid SHA-256d vector");
  require(memcmp(actual, wanted, 32) == 0, "SHA-256d vector mismatch");

  uint8_t genesis[80];
  require(decodeHex(
      "010000000000000000000000000000000000000000000000000000000000000000000000"
      "3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"
      "29ab5f49ffff001d1dac2b7c", genesis, 80), "invalid genesis header");
  mining_validation::referenceSha256d(genesis, 80, actual);
  require(decodeHex("6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000", wanted, 32), "invalid genesis hash");
  require(memcmp(actual, wanted, 32) == 0, "genesis hash mismatch");

  requireDoubleDigest(
      "010000006fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d619000000000"
      "0982051fd1e4ba744bbbe680e1fee14677ba1a3c3540bf7b1cdb606e857233e0e61bc6649"
      "ffff001d01e36299",
      "4860eb18bf1b1620e37e9490fc8a427514416fd75159ab86688e9a8300000000");
  requireDoubleDigest(
      "0100000050120119172a610421a6c3011dd330d9df07b63616c2cc1f1cd0020000000000"
      "6657a9252aacd5c0b2940996ecff952228c3067cc38d4885efb5a4ac4247e9f337221b4d"
      "4c86041b0f2b5710",
      "06e533fd1ada86391f3f6c343204b0d278d4aaec1c0b20aa27ba030000000000");
  requireDoubleDigest(
      "04e0ff3feb36c62f0471cee034811019e43b14f459b50e00cea30a000000000000000000"
      "659cecf4a06ed500031b741384e87d40ce5c16c3ec8c09b09ffe4b863c218d1f282d3c61"
      "e4480f17d767c2ab",
      "59a90c771a9e84e9372b0b223485273a19ba3e0ffc9005000000000000000000");
}

void testTargetComparison() {
  uint8_t hash[32] = {}, target[32] = {};
  require(mining_validation::hashMeetsTarget(hash, target), "zero equality");
  memset(target, 0xff, 32);
  require(mining_validation::hashMeetsTarget(hash, target), "zero below maximum");
  memset(hash, 0xff, 32); memset(target, 0, 32);
  require(!mining_validation::hashMeetsTarget(hash, target), "maximum above zero");
  memset(target, 0xff, 32);
  require(mining_validation::hashMeetsTarget(hash, target), "maximum equality");
  for (int position = 0; position < 32; ++position) {
    memset(hash, 0x5a, 32); memset(target, 0x5a, 32);
    hash[position] = 0x59;
    for (int lower = 0; lower < position; ++lower) hash[lower] = 0xff;
    require(mining_validation::hashMeetsTarget(hash, target), "byte below target");
    hash[position] = 0x5b;
    for (int lower = 0; lower < position; ++lower) hash[lower] = 0;
    require(!mining_validation::hashMeetsTarget(hash, target), "byte above target");
  }
}

void testCandidateValidation() {
  uint8_t header[80] = {}, hash[32], target[32];
  mining_validation::referenceSha256d(header, sizeof(header), hash);
  memset(target, 0xff, sizeof(target));
  bool meets = false;
  require(mining_validation::validateCandidate(7, 7, header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::Valid && meets,
          "valid candidate rejected");
  require(mining_validation::validateCandidate(7, 8, header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::StaleGeneration,
          "stale candidate accepted");
  hash[0] ^= 1;
  require(mining_validation::validateCandidate(8, 8, header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::HashMismatch,
          "corrupt candidate accepted");
  mining_validation::referenceSha256d(header, sizeof(header), hash);
  memset(target, 0, sizeof(target));
  require(mining_validation::validateCandidate(0xffffffffU, 0xffffffffU,
              header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::Valid && !meets,
          "generation boundary or target result incorrect");
  require(mining_validation::validateCandidate(0xffffffffU, 0,
              header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::StaleGeneration,
          "wrapped generation accepted stale candidate");
}

void testForcedCandidateGate() {
  uint8_t header[80] = {}, hash[32], corrupt[32], target[32];
  for (uint32_t nonce = 0; nonce < 10000; ++nonce) {
    memcpy(header + 76, &nonce, sizeof(nonce));
    mining_validation::referenceSha256d(header, sizeof(header), hash);

    memcpy(target, hash, sizeof(target));
    bool meets = false;
    require(mining_validation::validateCandidate(42, 42, header, hash,
                target, &meets) == mining_validation::CandidateValidationResult::Valid && meets,
            "forced exact-target candidate rejected");

    memcpy(corrupt, hash, sizeof(corrupt));
    corrupt[nonce % sizeof(corrupt)] ^= 1;
    require(mining_validation::validateCandidate(42, 42, header, corrupt,
                target, &meets) == mining_validation::CandidateValidationResult::HashMismatch,
            "forced corrupt candidate accepted");
    require(mining_validation::validateCandidate(42, 43, header, hash,
                target, &meets) == mining_validation::CandidateValidationResult::StaleGeneration,
            "forced stale candidate accepted");
  }
}

void testGenerationAndRanges() {
  uint32_t nonce = 0xda54e700U;
  uint64_t expected = nonce;
  for (uint32_t range = 0; range < 100000; ++range) {
    const uint32_t count = (range & 1U) ? 16384U : 4096U;
    require(nonce == static_cast<uint32_t>(expected), "nonce range overlap or gap");
    nonce += count;
    expected += count;
  }

  uint8_t header[80] = {}, hash[32], target[32];
  memset(target, 0xff, sizeof(target));
  mining_validation::referenceSha256d(header, sizeof(header), hash);
  for (uint32_t generation = 1; generation < 100000; ++generation) {
    bool meets = false;
    require(mining_validation::validateCandidate(generation, generation + 1,
              header, hash, target, &meets) ==
              mining_validation::CandidateValidationResult::StaleGeneration,
            "rapid replacement accepted stale candidate");
  }
}

uint64_t nextRandom(uint64_t &state) {
  state ^= state >> 12; state ^= state << 25; state ^= state >> 27;
  return state * 0x2545f4914f6cdd1dULL;
}

void testOptimizedDifferential(uint32_t cases) {
  uint64_t random = 0x4e6572644d696e65ULL;
  uint32_t filterPasses = 0;
  uint8_t padded[128], reference[32], optimized[32];
  uint32_t midstate[8], bake[16];
  for (uint32_t test = 0; test < cases; ++test) {
    for (size_t i = 0; i < 80; i += 8) {
      const uint64_t value = nextRandom(random);
      memcpy(padded + i, &value, 8);
    }
    memset(padded + 80, 0, 48); padded[80] = 0x80;
    padded[126] = 0x02; padded[127] = 0x80;
    nerd_mids(midstate, padded);
    nerd_sha256_bake(midstate, padded + 64, bake);
    const bool passed = nerd_sha256d_baked(midstate, padded + 64, bake, optimized);
    mining_validation::referenceSha256d(padded, 80, reference);
    const bool expected = reference[30] == 0 && reference[31] == 0;
    require(passed == expected, "early filter false result");
    if (passed) {
      ++filterPasses;
      require(memcmp(optimized, reference, 32) == 0, "optimized SHA mismatch");
    }
  }
  printf("Differential cases: %u; exact filter passes: %u\n", cases, filterPasses);
}

void benchmarkOptimized(uint32_t cases) {
  uint8_t padded[128] = {}, hash[32];
  uint32_t midstate[8], bake[16], passes = 0;
  padded[80] = 0x80; padded[126] = 0x02; padded[127] = 0x80;
  nerd_mids(midstate, padded);
  nerd_sha256_bake(midstate, padded + 64, bake);
  const auto start = std::chrono::steady_clock::now();
  for (uint32_t nonce = 0; nonce < cases; ++nonce) {
    memcpy(padded + 76, &nonce, sizeof(nonce));
    passes += nerd_sha256d_baked(midstate, padded + 64, bake, hash) ? 1U : 0U;
  }
  const double seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - start).count();
  printf("Host optimized benchmark: %.0f nonce/s; passes=%u\n", cases / seconds, passes);
}
}  // namespace

int main(int argc, char **argv) {
  uint32_t cases = argc >= 2 ? static_cast<uint32_t>(strtoul(argv[1], nullptr, 10)) : 2000000;
  testReferenceVectors();
  testTargetComparison();
  testCandidateValidation();
  testForcedCandidateGate();
  testGenerationAndRanges();
  testOptimizedDifferential(cases);
  if (argc == 3 && strcmp(argv[2], "--bench") == 0) benchmarkOptimized(cases);
  puts("All mining validation tests passed.");
  return 0;
}
