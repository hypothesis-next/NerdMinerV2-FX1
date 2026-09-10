#include "PoolStatsParsers.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint64_t HELIOS_ACTIVE_WINDOW_SECONDS = 24ULL * 60ULL * 60ULL;

int monthNumber(const char *month) {
  static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  for (int i = 0; i < 12; i++) {
    if (strncmp(month, months[i], 3) == 0) return i + 1;
  }
  return 0;
}

int64_t daysFromCivil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra =
      yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return static_cast<int64_t>(era) * 146097 +
         static_cast<int64_t>(dayOfEra) - 719468;
}

bool makeEpoch(int year, int month, int day, int hour, int minute, int second,
               uint64_t &epoch) {
  static const uint8_t daysPerMonth[] = {31, 28, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
  if (year < 1970 || month < 1 || month > 12 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 ||
      second > 60) {
    return false;
  }
  int maxDay = daysPerMonth[month - 1];
  const bool leapYear =
      year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  if (month == 2 && leapYear) maxDay = 29;
  if (day < 1 || day > maxDay) return false;
  const int64_t days = daysFromCivil(year, static_cast<unsigned>(month),
                                     static_cast<unsigned>(day));
  if (days < 0) return false;
  epoch = static_cast<uint64_t>(days) * 86400ULL +
          static_cast<uint64_t>(hour) * 3600ULL +
          static_cast<uint64_t>(minute) * 60ULL +
          static_cast<uint64_t>(second);
  return true;
}

bool validMetric(double value) {
  return isfinite(value) && value >= 0.0;
}

}  // namespace

void formatPoolNumber(double value, char *output, size_t outputSize) {
  if (outputSize == 0) return;
  if (!validMetric(value)) {
    snprintf(output, outputSize, "N/A");
    return;
  }

  static const char suffixes[] = {'\0', 'K', 'M', 'G', 'T', 'P', 'E'};
  size_t suffixIndex = 0;
  while (value >= 1000.0 && suffixIndex < sizeof(suffixes) - 1) {
    value /= 1000.0;
    suffixIndex++;
  }

  if (suffixIndex == 0) {
    if (value == 0.0) {
      snprintf(output, outputSize, "0");
    } else if (value < 10.0) {
      snprintf(output, outputSize, "%.2f", value);
    } else if (value < 100.0) {
      snprintf(output, outputSize, "%.1f", value);
    } else {
      snprintf(output, outputSize, "%.0f", value);
    }
    return;
  }

  const int decimals = value < 10.0 ? 2 : (value < 100.0 ? 1 : 0);
  snprintf(output, outputSize, "%.*f%c", decimals, value,
           suffixes[suffixIndex]);
}

bool parseHttpDateUtc(const char *value, uint64_t &epoch) {
  if (value == nullptr) return false;
  char weekday[4] = {};
  char month[4] = {};
  char zone[4] = {};
  int day = 0;
  int year = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  const int parsed = sscanf(value, "%3s, %d %3s %d %d:%d:%d %3s", weekday,
                            &day, month, &year, &hour, &minute, &second, zone);
  if (parsed != 8 || strcmp(zone, "GMT") != 0) return false;
  const int monthValue = monthNumber(month);
  return makeEpoch(year, monthValue, day, hour, minute, second, epoch);
}

bool parseIso8601Utc(const char *value, uint64_t &epoch) {
  if (value == nullptr) return false;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int consumed = 0;
  if (sscanf(value, "%d-%d-%dT%d:%d:%d%n", &year, &month, &day, &hour,
             &minute, &second, &consumed) != 6) {
    return false;
  }
  const char *suffix = value + consumed;
  if (*suffix == '.') {
    suffix++;
    if (!isdigit(static_cast<unsigned char>(*suffix))) return false;
    while (isdigit(static_cast<unsigned char>(*suffix))) suffix++;
  }
  if (suffix[0] != 'Z' || suffix[1] != '\0') return false;
  return makeEpoch(year, month, day, hour, minute, second, epoch);
}

bool parsePublicPoolDocument(JsonVariantConst root,
                             PoolStatsSnapshot &snapshot) {
  if (!root["bestDifficulty"].is<double>() ||
      !root["workersCount"].is<int>() || !root["workers"].is<JsonArrayConst>()) {
    return false;
  }

  const double bestDifficulty = root["bestDifficulty"].as<double>();
  const int workersCount = root["workersCount"].as<int>();
  if (!validMetric(bestDifficulty) || workersCount < 0) return false;

  double totalHashRate = 0.0;
  for (JsonObjectConst worker : root["workers"].as<JsonArrayConst>()) {
    if (!worker["hashRate"].is<double>()) return false;
    const double hashRate = worker["hashRate"].as<double>();
    if (!validMetric(hashRate)) return false;
    totalHashRate += hashRate;
  }
  if (!validMetric(totalHashRate)) return false;

  formatPoolNumber(bestDifficulty, snapshot.bestDifficulty,
                   sizeof(snapshot.bestDifficulty));
  snprintf(snapshot.workersCount, sizeof(snapshot.workersCount), "%d",
           workersCount);
  formatPoolNumber(totalHashRate, snapshot.totalHashRate,
                   sizeof(snapshot.totalHashRate));
  return true;
}

bool parseHeliosSnapshotDocument(JsonVariantConst root, uint64_t serverEpoch,
                                 PoolStatsSnapshot &snapshot) {
  if (!root["stats"].is<JsonArrayConst>() ||
      root["stats"].as<JsonArrayConst>().size() == 0 ||
      !root["workers"].is<JsonArrayConst>()) {
    return false;
  }

  JsonArrayConst stats = root["stats"].as<JsonArrayConst>();
  JsonObjectConst latest = stats[0].as<JsonObjectConst>();
  if (!latest["bestEver"].is<double>() ||
      !latest["hashrate5m"].is<double>()) {
    return false;
  }
  const double bestEver = latest["bestEver"].as<double>();
  const double hashRate5m = latest["hashrate5m"].as<double>();
  if (!validMetric(bestEver) || !validMetric(hashRate5m)) return false;

  uint32_t activeWorkers = 0;
  for (JsonObjectConst worker : root["workers"].as<JsonArrayConst>()) {
    const char *lastUpdate = worker["lastUpdate"].as<const char *>();
    uint64_t workerEpoch = 0;
    if (!parseIso8601Utc(lastUpdate, workerEpoch)) return false;

    if (workerEpoch <= serverEpoch &&
        serverEpoch - workerEpoch <= HELIOS_ACTIVE_WINDOW_SECONDS) {
      activeWorkers++;
    }
  }

  formatPoolNumber(bestEver, snapshot.bestDifficulty,
                   sizeof(snapshot.bestDifficulty));
  snprintf(snapshot.workersCount, sizeof(snapshot.workersCount), "%lu",
           static_cast<unsigned long>(activeWorkers));
  formatPoolNumber(hashRate5m, snapshot.totalHashRate,
                   sizeof(snapshot.totalHashRate));
  return true;
}
