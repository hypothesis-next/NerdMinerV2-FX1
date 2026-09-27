#pragma once
#include <ctype.h>

// Current official response puts the compact user before large histories.
// Fail closed if that framing changes; never scan an unbounded response.
template <typename ReadByte>
bool enterHeliosUserObject(ReadByte read) {
  auto next = [&read]() {
    int c;
    do { c = read(); } while (c >= 0 && isspace(c));
    return c;
  };
  return next() == '{' && next() == '"' && read() == 'u' &&
      read() == 's' && read() == 'e' && read() == 'r' &&
      read() == '"' && next() == ':';
}
