/*!
 * \file PosSnapshotImpl.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/Def.hpp"
#include "def/Pnl.hpp"
#include "def/PosInfo.hpp"

namespace bq {

struct Pnl;
using PnlSPtr = std::shared_ptr<Pnl>;

class PosSnapshotImpl;
using PosSnapshotImplSPtr = std::shared_ptr<PosSnapshotImpl>;

class PosSnapshotImpl {
  inline static std::map<std::string, int> FieldName2NoInPosInfo{
      {"productId", 0},   {"userId", 1}, {"acctId", 2},     {"stgId", 3},
      {"stgInstId", 4},   {"algoId", 5}, {"marketCode", 6}, {"symbolType", 7},
      {"symbolCode", 8},  {"side", 9},   {"posSide", 10},   {"parValue", 11},
      {"feeCurrency", 12}};

 public:
  PosSnapshotImpl(const PosSnapshotImpl&) = delete;
  PosSnapshotImpl& operator=(const PosSnapshotImpl&) = delete;
  PosSnapshotImpl(const PosSnapshotImpl&&) = delete;
  PosSnapshotImpl& operator=(const PosSnapshotImpl&&) = delete;

  explicit PosSnapshotImpl(
      const std::map<std::string, PosInfoSPtr>& posInfoDetail,
      const MarketDataCacheSPtr& marketDataCache);

 public:
  const std::map<std::string, PosInfoSPtr>& getPosInfoDetail() const;

  std::tuple<int, PnlSPtr> queryPnl(
      const std::string& queryCond, const std::string& quoteCurrencyForCalc,
      const std::string& quoteCurrencyForConv,
      const std::string& origQuoteCurrencyOfUBasedContract = "USDT");

  std::tuple<int, Key2PnlGroupSPtr> queryPnlGroupBy(
      const std::string& groupCond, const std::string& quoteCurrencyForCalc,
      const std::string& quoteCurrencyForConv,
      const std::string& origQuoteCurrencyOfUBasedContract = "USDT");

 public:
  std::tuple<int, PosInfoGroupSPtr> queryPosInfoGroup(
      const std::string& queryCond);

  std::tuple<int, Key2PosInfoBundleSPtr> queryPosInfoGroupBy(
      const std::string& groupCond);

 private:
  std::map<std::string, PosInfoSPtr> posInfoDetail_;
  MarketDataCacheSPtr marketDataCache_{nullptr};

  std::map<std::string, Key2PnlGroupSPtr> cond2Key2PnlGroup_;
  std::map<std::string, Key2PosInfoBundleSPtr> cond2Key2PosInfoBundle_;
};

}  // namespace bq
