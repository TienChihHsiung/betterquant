/*!
 * \file SymbolInfo.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "def/SymbolInfo.hpp"

#include "def/BQConst.hpp"
#include "util/Logger.hpp"

namespace bq {

std::string SymbolInfo::toStr() const {
  std::string ret;
  ret = fmt::format("{} {} {}", GetMarketName(marketCode_),
                    magic_enum::enum_name(symbolType_), symbolCode_);
  return ret;
}

std::string SymbolInfoGroup2Str(
    const std::vector<SymbolInfoSPtr> symbolInfoGroup) {
  std::string ret;
  for (const auto symbolInfo : symbolInfoGroup) {
    ret = ret + symbolInfo->toStr() + "; ";
  }
  if (ret.size() > 1) {
    ret = ret.substr(0, ret.length() - 2);
  }
  return ret;
}

MarketCode2SymbolInfoGroupSPtr MakeMarketCode2SymbolInfoGroup(
    const TopicGroup& topicGroup) {
  auto ret = std::make_shared<MarketCode2SymbolInfoGroup>();
  for (const auto& topic : topicGroup) {
    std::vector<std::string> fieldGroup;
    boost::split(fieldGroup, topic, boost::is_any_of(SEP_OF_TOPIC));
    if (fieldGroup.size() < 5) {
      const auto fmt =
          "Make marketCode2SymbolInfoGroup failed because of invalid topic {}.";
      LOG_W(fmt, topic);
      continue;
    }

    const auto marketCode = GetMarketCode(fieldGroup[1]);
    const auto symbolTypeValue =
        magic_enum::enum_cast<SymbolType>(fieldGroup[2]);
    if (!symbolTypeValue.has_value()) {
      const auto fmt =
          "Make marketCode2SymbolInfoGroup failed because of invalid topic {}.";
      LOG_W(fmt, topic);
      continue;
    }
    const auto symbolType = symbolTypeValue.value();
    const auto symbolCode = fieldGroup[3];
    const auto symbolInfo =
        std::make_shared<SymbolInfo>(marketCode, symbolType, symbolCode);
    (*ret)[marketCode].emplace_back(symbolInfo);
  }
  return ret;
}

}  // namespace bq
