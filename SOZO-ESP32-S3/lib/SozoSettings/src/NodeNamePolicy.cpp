#include "NodeNamePolicy.h"

#include <cstdint>

namespace {

bool decodeUtf8CodePoint(const char *value, const size_t length, size_t &index,
                         uint32_t &codePoint) {
  if (value == nullptr || index >= length) return false;
  const uint8_t first = static_cast<uint8_t>(value[index]);
  size_t continuationCount = 0U;

  if (first <= 0x7fU) {
    codePoint = first;
  } else if (first >= 0xc2U && first <= 0xdfU) {
    continuationCount = 1U;
    codePoint = first & 0x1fU;
  } else if (first >= 0xe0U && first <= 0xefU) {
    continuationCount = 2U;
    codePoint = first & 0x0fU;
  } else if (first >= 0xf0U && first <= 0xf4U) {
    continuationCount = 3U;
    codePoint = first & 0x07U;
  } else {
    return false;
  }

  if (index + continuationCount >= length) return false;
  for (size_t offset = 1U; offset <= continuationCount; ++offset) {
    const uint8_t continuation =
        static_cast<uint8_t>(value[index + offset]);
    if ((continuation & 0xc0U) != 0x80U) return false;
    codePoint = (codePoint << 6U) | (continuation & 0x3fU);
  }

  if ((continuationCount == 2U && codePoint < 0x800U) ||
      (continuationCount == 3U && codePoint < 0x10000U) ||
      (codePoint >= 0xd800U && codePoint <= 0xdfffU) ||
      codePoint > 0x10ffffU) {
    return false;
  }
  index += continuationCount + 1U;
  return true;
}

bool isEdgeWhitespace(const uint32_t codePoint) {
  return (codePoint >= 0x09U && codePoint <= 0x0dU) ||
         codePoint == 0x20U || codePoint == 0x00a0U ||
         codePoint == 0x1680U ||
         (codePoint >= 0x2000U && codePoint <= 0x200aU) ||
         codePoint == 0x202fU || codePoint == 0x205fU ||
         codePoint == 0x3000U || codePoint == 0xfeffU;
}

bool isForbiddenCodePoint(const uint32_t codePoint) {
  return codePoint < 0x20U ||
         (codePoint >= 0x7fU && codePoint <= 0x9fU) ||
         codePoint == 0x2028U || codePoint == 0x2029U ||
         (codePoint >= 0x202aU && codePoint <= 0x202eU) ||
         (codePoint >= 0x2066U && codePoint <= 0x2069U);
}

}  // namespace

namespace sozo {

bool inspectNodeName(const char *value, const size_t length,
                     NodeNameSpan &span) {
  span = {};
  if (value == nullptr) return length == 0U;

  size_t index = 0U;
  size_t firstContentByte = length;
  size_t lastContentEnd = 0U;
  while (index < length) {
    const size_t codePointStart = index;
    uint32_t codePoint = 0U;
    if (!decodeUtf8CodePoint(value, length, index, codePoint)) return false;
    if (!isEdgeWhitespace(codePoint)) {
      if (firstContentByte == length) firstContentByte = codePointStart;
      lastContentEnd = index;
    }
  }

  if (firstContentByte == length) return true;
  if (lastContentEnd - firstContentByte > kMaxNodeNameBytes) return false;

  index = firstContentByte;
  size_t codePointCount = 0U;
  while (index < lastContentEnd) {
    uint32_t codePoint = 0U;
    if (!decodeUtf8CodePoint(value, lastContentEnd, index, codePoint) ||
        isForbiddenCodePoint(codePoint) ||
        ++codePointCount > kMaxNodeNameCodePoints) {
      return false;
    }
  }

  span.begin = firstContentByte;
  span.end = lastContentEnd;
  return true;
}

}  // namespace sozo
