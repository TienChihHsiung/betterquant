/*!
 * \file OrderInfoIF.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "SHMHeader.hpp"
#include "def/BQConstIF.hpp"
#include "def/BQDefIF.hpp"

namespace bq {

class FeeInfoCache;
using FeeInfoCacheSPtr = std::shared_ptr<FeeInfoCache>;

enum class IsSomeFieldOfOrderUpdated { True = 1, False = 2 };
enum class IsTheOrderCanBeUsedCalcPos { True = 1, False = 2 };

struct OrderInfo;
using OrderInfoSPtr = std::shared_ptr<OrderInfo>;

struct OrderInfo {
  SHMHeader shmHeader_;

  ProductId productId_{0};
  UserId userId_{0};
  AcctId acctId_{0};

  StgId stgId_{0};
  StgInstId stgInstId_{0};
  AlgoId algoId_{0};

  OrderId orderId_{0};
  ExchOrderId exchOrderId_{0};
  OrderId parentOrderId_{0};

  MarketCode marketCode_{MarketCode::Others};
  SymbolType symbolType_{SymbolType::Others};
  char symbolCode_[MAX_SYMBOL_CODE_LEN];
  char exchSymbolCode_[MAX_SYMBOL_CODE_LEN];

  Side side_{Side::Others};
  PosSide posSide_{PosSide::Both};

  Decimal orderPrice_{0};
  Decimal orderSize_{0};

  std::int32_t parValue_{0};

  OrderType orderType_{OrderType::Limit};
  OrderTypeExtra orderTypeExtra_{OrderTypeExtra::Normal};
  std::uint64_t orderTime_{0};

#ifdef SIMED_MODE
  char simedTDInfo_[MAX_SIMED_TD_INFO];
#endif

  Decimal fee_{0};
  char feeCurrency_[MAX_CURRENCY_LEN];

  Decimal dealSize_{0};
  Decimal avgDealPrice_{0};

  char lastTradeId_[MAX_TRADE_ID_LEN];
  Decimal lastDealPrice_{0};
  Decimal lastDealSize_{0};
  std::uint64_t lastDealTime_{UNDEFINED_FIELD_MIN_TS};

  OrderStatus orderStatus_{OrderStatus::Created};
  std::uint64_t noUsedToCalcPos_{0};
  std::int32_t statusCode_{0};

  IsSomeFieldOfOrderUpdated updateByOrderInfoFromExch(
      const OrderInfoSPtr& newOrderInfo, std::uint64_t noUsedToCalcPos,
      const FeeInfoCacheSPtr& feeInfoCache = nullptr);

  IsTheOrderCanBeUsedCalcPos updateByOrderInfoFromTDGW(
      const OrderInfoSPtr& newOrderInfo);

  bool closed() const { return orderStatus_ >= OrderStatus::Filled; }
  bool notClosed() const { return !closed(); }

  std::string toShortStr() const;
  std::string toJson() const;

  std::string getSqlOfReplace() const;
  std::string getSqlOfUSPOrderInfoUpdate() const;
  Decimal getFeeOfLastTrade() const;

  std::string getPosKey() const;
  std::string getPosKeyOfBid() const;
  std::string getPosKeyOfAsk() const;
};

inline OrderInfoSPtr MakeOrderInfo() { return std::make_shared<OrderInfo>(); }

}  // namespace bq
