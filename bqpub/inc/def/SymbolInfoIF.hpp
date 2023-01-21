/*!
 * \file SymbolInfoIF.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/BQConstIF.hpp"
#include "def/BQDefIF.hpp"
#include "util/PchBase.hpp"

namespace bq {

struct SymbolInfo;
using SymbolInfoSPtr = std::shared_ptr<SymbolInfo>;

struct SymbolInfo {
  SymbolInfo(MarketCode marketCode, SymbolType symbolType,
             const std::string& symbolCode)
      : marketCode_(marketCode),
        symbolType_(symbolType),
        symbolCode_(symbolCode) {}
  MarketCode marketCode_;
  SymbolType symbolType_;
  std::string symbolCode_;

  std::string toStr() const;
};

using SymbolInfoGroup = std::vector<SymbolInfoSPtr>;
using MarketCode2SymbolInfoGroup = std::map<MarketCode, SymbolInfoGroup>;
using MarketCode2SymbolInfoGroupSPtr =
    std::shared_ptr<MarketCode2SymbolInfoGroup>;

MarketCode2SymbolInfoGroupSPtr MakeMarketCode2SymbolInfoGroup(
    const TopicGroup& topicGroup);

}  // namespace bq
