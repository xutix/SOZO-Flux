#pragma once

#include <cstddef>

namespace sozo {

constexpr size_t kMaxNodeNameCodePoints = 16U;
constexpr size_t kMaxNodeNameBytes = 64U;

struct NodeNameSpan {
  size_t begin{0U};
  size_t end{0U};
};

// Validates one UTF-8 display name and returns the byte span after trimming
// edge whitespace. An empty span is valid and means "use the default name".
bool inspectNodeName(const char *value, size_t length, NodeNameSpan &span);

}  // namespace sozo
