/*!
 * \file TestMain.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "util/File.hpp"
#include "util/String.hpp"

using namespace bq;

class global_event : public testing::Environment {
 public:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

TEST(test, test1) {
  std::string str;
  str = bq::ReplaceSubStrBetween2Str("abcdefghi", "123", "abc", "ghi");
  EXPECT_TRUE(str == "abc123ghi");

  str = bq::ReplaceSubStrBetween2Str("abcdefghi", "123", "", "def");
  EXPECT_TRUE(str == "123defghi");

  str = bq::ReplaceSubStrBetween2Str("abcdefghi", "123", "de", "i");
  EXPECT_TRUE(str == "abcde123i");

  str = bq::ReplaceSubStrBetween2Str("abcdefghi", "12", "d", "f");
  EXPECT_TRUE(str == "abcd12fghi");

  str = bq::ReplaceSubStrBetween2Str("abcdefghi", "12", "cd", "fg");
  EXPECT_TRUE(str == "abcd12fghi");
}

TEST(test, testBase64) {
  const auto origStr = "Hello world!";
  const auto encodeStr = Base64Encode(origStr);
  const auto decodeStr = Base64Decode(encodeStr);
  EXPECT_TRUE(origStr == decodeStr);
}

TEST(test, testFile) {
  std::vector<std::string> lineGroup;
  lineGroup.emplace_back("aaa");
  lineGroup.emplace_back("bbb");
  const auto filename = "/tmp/lineGroup.dat";
  SaveDataToFile(filename, lineGroup);
  const auto [statusCode, lg] = LoadFileContToVec(filename);
  EXPECT_TRUE(lg.size() == 2);
  EXPECT_TRUE(lg[0] == "aaa");
  EXPECT_TRUE(lg[1] == "bbb");
}

int main(int argc, char** argv) {
  testing::AddGlobalTestEnvironment(new global_event);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
