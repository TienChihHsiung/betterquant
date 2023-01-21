/*!
 * \file BQDefIF.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/BQConstIF.hpp"
#include "util/PchBase.hpp"

namespace bq {

template <typename Task>
struct AsyncTask;

struct SHMIPCTask;
using SHMIPCTaskSPtr = std::shared_ptr<SHMIPCTask>;

using SHMIPCAsyncTask = AsyncTask<SHMIPCTaskSPtr>;
using SHMIPCAsyncTaskSPtr = std::shared_ptr<SHMIPCAsyncTask>;

using Decimal = double;

using ProductId = std::uint16_t;

using StgId = std::uint16_t;
using StgInstId = std::uint16_t;

using AlgoId = std::uint32_t;

using UserId = std::uint16_t;
using AcctId = std::uint16_t;

using OrderId = std::uint64_t;
using ExchOrderId = std::uint64_t;

struct MDHeader {
  std::uint64_t exchTs_;
  std::uint64_t localTs_;
  MarketCode marketCode_;
  SymbolType symbolType_;
  char symbolCode_[MAX_SYMBOL_CODE_LEN];
  MDType mdType_;
  std::string toStr() const;
  std::string getTopicPrefix() const;
  std::string toJson() const;
};

using TopicGroup = std::set<std::string>;
using TopicGroupSPtr = std::shared_ptr<TopicGroup>;

}  // namespace bq
