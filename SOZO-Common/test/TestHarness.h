#pragma once

#include <cstdio>
#include <cstring>

namespace sozo::test {

inline int failures = 0;

inline void check(const bool condition, const char *expression,
                  const int line) {
  if (condition) return;
  std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
  ++failures;
}

inline int finish(const char *suiteName) {
  if (failures == 0) std::printf("PASS %s\n", suiteName);
  return failures == 0 ? 0 : 1;
}

}  // namespace sozo::test

#define CHECK_TRUE(expression) \
  ::sozo::test::check((expression), #expression, __LINE__)
#define CHECK_EQ(expected, actual) \
  ::sozo::test::check((expected) == (actual), \
                      #expected " == " #actual, __LINE__)
#define CHECK_MEMORY_EQ(expected, actual, length) \
  ::sozo::test::check(std::memcmp((expected), (actual), (length)) == 0, \
                      #expected " memory equals " #actual, __LINE__)
