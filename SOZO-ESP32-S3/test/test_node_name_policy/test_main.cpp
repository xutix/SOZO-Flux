#include <NodeNamePolicy.h>

#include <string>

#include "../../../SOZO-Common/test/TestHarness.h"

namespace {

bool inspect(const std::string &value, sozo::NodeNameSpan &span) {
  return sozo::inspectNodeName(value.data(), value.size(), span);
}

void test_accepts_unicode_and_reports_the_trimmed_span() {
  const std::string ideographicSpace = "\xe3\x80\x80";
  const std::string name =
      ideographicSpace + "\xe5\xb7\xa5\xe4\xbd\x9c\xe5\x8f\xb0" +
      "\xf0\x9f\x98\x80" + ideographicSpace;
  sozo::NodeNameSpan span{};

  CHECK_TRUE(inspect(name, span));
  CHECK_EQ(ideographicSpace.size(), span.begin);
  CHECK_EQ(name.size() - ideographicSpace.size(), span.end);
  CHECK_EQ(std::string("\xe5\xb7\xa5\xe4\xbd\x9c\xe5\x8f\xb0"
                       "\xf0\x9f\x98\x80"),
           name.substr(span.begin, span.end - span.begin));
}

void test_empty_and_unicode_whitespace_restore_the_default() {
  for (const std::string &value : {
           std::string(), std::string(" \t\r\n"),
           std::string("\xc2\xa0\xe3\x80\x80"),
           std::string("\xef\xbb\xbf")}) {
    sozo::NodeNameSpan span{99U, 99U};
    CHECK_TRUE(inspect(value, span));
    CHECK_EQ(0U, span.begin);
    CHECK_EQ(0U, span.end);
  }
}

void test_enforces_code_point_and_byte_limits() {
  sozo::NodeNameSpan span{};
  CHECK_TRUE(inspect(std::string(16U, 'a'), span));
  CHECK_TRUE(!inspect(std::string(17U, 'a'), span));

  const std::string emoji = "\xf0\x9f\x98\x80";
  std::string sixteenEmoji;
  for (size_t index = 0U; index < 16U; ++index) sixteenEmoji += emoji;
  CHECK_EQ(64U, sixteenEmoji.size());
  CHECK_TRUE(inspect(sixteenEmoji, span));
  CHECK_TRUE(!inspect(sixteenEmoji + emoji, span));
}

void test_rejects_malformed_utf8_and_invisible_controls() {
  sozo::NodeNameSpan span{};
  for (const std::string &value : {
           std::string("\xc0\xaf", 2U),
           std::string("\xe2\x82", 2U),
           std::string("\xed\xa0\x80", 3U),
           std::string("\xf4\x90\x80\x80", 4U),
           std::string("A\tB", 3U),
           std::string("\xc2\x85", 2U),
           std::string("\xe2\x80\xa8", 3U),
           std::string("\xe2\x80\xae", 3U)}) {
    CHECK_TRUE(!inspect(value, span));
  }
}

}  // namespace

int runNodeNamePolicyTests() {
  test_accepts_unicode_and_reports_the_trimmed_span();
  test_empty_and_unicode_whitespace_restore_the_default();
  test_enforces_code_point_and_byte_limits();
  test_rejects_malformed_utf8_and_invisible_controls();
  return sozo::test::finish("node-name policy tests");
}

#ifdef ARDUINO
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(200U);
  runNodeNamePolicyTests();
  Serial.printf("Node-name policy failures: %d\n", sozo::test::failures);
}

void loop() {}
#else
int main(int, char **) { return runNodeNamePolicyTests(); }
#endif
