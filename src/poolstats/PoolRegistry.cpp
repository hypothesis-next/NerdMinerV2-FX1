#include "PoolRegistry.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace {

void copyText(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) return;
  if (source == nullptr) source = "";
  snprintf(destination, destinationSize, "%s", source);
}

bool equals(const char *left, const char *right) {
  return strcmp(left, right) == 0;
}

}  // namespace

void normalizePoolHost(const char *input, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  output[0] = '\0';
  if (input == nullptr) return;

  while (isspace(static_cast<unsigned char>(*input))) input++;

  const char *scheme = strstr(input, "://");
  if (scheme != nullptr) input = scheme + 3;

  size_t length = 0;
  while (input[length] != '\0' && input[length] != '/' &&
         input[length] != '?' && input[length] != '#') {
    length++;
  }
  while (length > 0 && isspace(static_cast<unsigned char>(input[length - 1]))) {
    length--;
  }

  if (length > 0 && input[0] != '[') {
    const char *colon = static_cast<const char *>(memchr(input, ':', length));
    if (colon != nullptr) length = static_cast<size_t>(colon - input);
  }

  while (length > 0 && input[length - 1] == '.') length--;
  if (length >= outputSize) length = outputSize - 1;

  for (size_t i = 0; i < length; i++) {
    output[i] = static_cast<char>(tolower(static_cast<unsigned char>(input[i])));
  }
  output[length] = '\0';
}

void makePoolDisplayName(const char *host, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  const size_t maxVisible = outputSize - 1;
  const size_t length = host == nullptr ? 0 : strlen(host);

  if (length <= maxVisible) {
    copyText(output, outputSize, host);
    return;
  }

  if (maxVisible <= 3) {
    for (size_t i = 0; i < maxVisible; i++) output[i] = '.';
    output[maxVisible] = '\0';
    return;
  }

  const size_t prefixLength = maxVisible - 3;
  memcpy(output, host, prefixLength);
  memcpy(output + prefixLength, "...", 3);
  output[maxVisible] = '\0';
}

PoolDefinition resolvePoolDefinition(const char *host, uint16_t port) {
  PoolDefinition definition{};
  definition.provider = PoolProviderKind::None;
  definition.stratumPort = port;
  normalizePoolHost(host, definition.normalizedHost,
                    sizeof(definition.normalizedHost));
  makePoolDisplayName(definition.normalizedHost, definition.displayName,
                      sizeof(definition.displayName));

  if (equals(definition.normalizedHost, "public-pool.io")) {
    definition.provider = PoolProviderKind::PublicPoolCompatible;
    copyText(definition.displayName, sizeof(definition.displayName),
             "Public Pool");
    copyText(definition.apiBaseUrl, sizeof(definition.apiBaseUrl),
             "https://public-pool.io:40557/api/client/");
  } else if (equals(definition.normalizedHost, "btc.heliospool.com")) {
    definition.provider = PoolProviderKind::HeliosPool;
    copyText(definition.displayName, sizeof(definition.displayName),
             "HeliosPool");
    copyText(definition.apiBaseUrl, sizeof(definition.apiBaseUrl),
             "https://stats-btc.heliospool.com/api/users/snapshot?address=");
  } else if (equals(definition.normalizedHost, "pool.nerdminers.org")) {
    definition.provider = PoolProviderKind::PublicPoolCompatible;
    copyText(definition.displayName, sizeof(definition.displayName),
             "NerdMiner Pool");
    copyText(definition.apiBaseUrl, sizeof(definition.apiBaseUrl),
             "https://pool.nerdminers.org/users/");
  } else if (equals(definition.normalizedHost, "pool.sethforprivacy.com")) {
    definition.provider = PoolProviderKind::PublicPoolCompatible;
    copyText(definition.displayName, sizeof(definition.displayName),
             "Seth for Privacy");
    copyText(definition.apiBaseUrl, sizeof(definition.apiBaseUrl),
             "https://pool.sethforprivacy.com/api/client/");
  } else if (equals(definition.normalizedHost, "pool.solomining.de")) {
    definition.provider = PoolProviderKind::PublicPoolCompatible;
    copyText(definition.displayName, sizeof(definition.displayName),
             "SoloMining.de");
    copyText(definition.apiBaseUrl, sizeof(definition.apiBaseUrl),
             "https://pool.solomining.de/api/client/");
  } else if (equals(definition.normalizedHost, "tn.vkbit.com")) {
    definition.provider = PoolProviderKind::Testnet;
    copyText(definition.displayName, sizeof(definition.displayName), "TESTNET");
  }

  return definition;
}

void extractPoolWallet(const char *username, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  output[0] = '\0';
  if (username == nullptr) return;

  const char *separator = strchr(username, '.');
  size_t length = separator == nullptr ? strlen(username)
                                       : static_cast<size_t>(separator - username);
  if (length >= outputSize) length = outputSize - 1;
  memcpy(output, username, length);
  output[length] = '\0';
}
