/*!
 * \file OrdMgr.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "def/OrderInfo.hpp"
#include "util/PchBase.hpp"

namespace bq::db {
class DBEng;
using DBEngSPtr = std::shared_ptr<DBEng>;
}  // namespace bq::db

namespace bq {

class FeeInfoCache;
using FeeInfoCacheSPtr = std::shared_ptr<FeeInfoCache>;

class OrdMgr {
  struct TagOrderId {};
  struct KeyOrderId : boost::multi_index::composite_key<
                          OrderInfo, MIDX_MEMER(OrderInfo, OrderId, orderId_)> {
  };
  using MIdxOrderId = boost::multi_index::ordered_unique<
      boost::multi_index::tag<TagOrderId>, KeyOrderId,
      boost::multi_index::composite_key_result_less<KeyOrderId::result_type>>;

  struct TagExchOrderId {};
  struct KeyExchOrderId
      : boost::multi_index::composite_key<
            OrderInfo, MIDX_MEMER(OrderInfo, ExchOrderId, exchOrderId_)> {};
  using MIdxExchOrderId = boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<TagExchOrderId>, KeyExchOrderId,
      boost::multi_index::composite_key_result_less<
          KeyExchOrderId::result_type>>;

  struct TagMarketCodeExchOrderId {};
  struct KeyMarketCodeExchOrderId
      : boost::multi_index::composite_key<
            OrderInfo, MIDX_MEMER(OrderInfo, MarketCode, marketCode_),
            MIDX_MEMER(OrderInfo, ExchOrderId, exchOrderId_)> {};
  using MIdxMarketCodeExchOrderId = boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<TagMarketCodeExchOrderId>,
      KeyMarketCodeExchOrderId,
      boost::multi_index::composite_key_result_less<
          KeyMarketCodeExchOrderId::result_type>>;

  using OrderInfoGroup = boost::multi_index::multi_index_container<
      OrderInfoSPtr,
      boost::multi_index::indexed_by<MIdxOrderId, MIdxExchOrderId,
                                     MIdxMarketCodeExchOrderId>>;
  using OrderInfoGroupSPtr = std::shared_ptr<OrderInfoGroup>;

 public:
  OrdMgr(const OrdMgr&) = delete;
  OrdMgr& operator=(const OrdMgr&) = delete;
  OrdMgr(const OrdMgr&&) = delete;
  OrdMgr& operator=(const OrdMgr&&) = delete;

  OrdMgr();

 public:
  int init(const YAML::Node& node, const db::DBEngSPtr& dbEng,
           const std::string& sql);

 private:
  int initOrderInfoGroup(const std::string& sql);

 public:
  int add(const OrderInfoSPtr& orderInfo, DeepClone deepClone,
          LockFunc lockFunc = LockFunc::True);

  int remove(OrderId orderId, LockFunc lockFunc = LockFunc::True);

  std::tuple<int, OrderInfoSPtr> getOrderInfo(
      OrderId orderId, DeepClone deepClone, LockFunc lockFunc = LockFunc::True);

  std::tuple<int, OrderInfoSPtr> getOrderInfoByExchOrderId(
      ExchOrderId exchOrderId, DeepClone deepClone,
      LockFunc lockFunc = LockFunc::True);

  std::tuple<int, OrderInfoSPtr> getOrderInfo(
      MarketCode marketCode, ExchOrderId exchOrderId, DeepClone deepClone,
      LockFunc lockFunc = LockFunc::True);

  std::vector<OrderInfoSPtr> getOrderInfoGroup(
      std::uint32_t secAgoTheOrderNeedToBeSynced,
      DeepClone deepClone = DeepClone::True,
      LockFunc lockFunc = LockFunc::True) const;

  std::tuple<IsSomeFieldOfOrderUpdated, OrderInfoSPtr>
  updateByOrderInfoFromExch(const OrderInfoSPtr& orderInfoFromExch,
                            std::uint64_t noUsedToCalcPos, DeepClone deepClone,
                            LockFunc lockFunc = LockFunc::True,
                            const FeeInfoCacheSPtr& feeInfoCache = nullptr);

  std::tuple<IsTheOrderCanBeUsedCalcPos, OrderInfoSPtr>
  updateByOrderInfoFromTDGW(const OrderInfoSPtr& orderInfoFromTDGW,
                            LockFunc lockFunc = LockFunc::True);

  int updateExchOrderId(OrderId orderId, ExchOrderId exchOrderId,
                        LockFunc lockFunc = LockFunc::True);

 private:
  OrderInfoSPtr getOrderInfo(const OrderInfoSPtr& orderInfo,
                             DeepClone deepClone,
                             LockFunc lockFunc = LockFunc::True);

 public:
  YAML::Node& getNode() { return node_; }

 private:
  YAML::Node node_;

  db::DBEngSPtr dbEng_{nullptr};

  OrderInfoGroupSPtr orderInfoGroup_{nullptr};
  mutable std::ext::spin_mutex mtxOrderInfoGroup_;
};

}  // namespace bq
