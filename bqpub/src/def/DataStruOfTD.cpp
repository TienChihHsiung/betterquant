/*!
 * \file DataStruOfTD.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "def/DataStruOfTD.hpp"

#include "db/TBLOrderInfo.hpp"
#include "db/TBLStgInstInfo.hpp"
#include "db/TBLTradeInfo.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/DataStruOfStg.hpp"
#include "def/Def.hpp"
#include "def/StatusCode.hpp"
#include "util/Datetime.hpp"
#include "util/FeeInfoCache.hpp"
#include "util/Float.hpp"
#include "util/Logger.hpp"
#include "util/Random.hpp"
#include "util/String.hpp"

namespace bq {

IsSomeFieldOfOrderUpdated OrderInfo::updateByOrderInfoFromExch(
    const OrderInfoSPtr& newOrderInfo, std::uint64_t noUsedToCalcPos,
    const FeeInfoCacheSPtr& feeInfoCache) {
  IsSomeFieldOfOrderUpdated isTheOrderInfoUpdated =
      IsSomeFieldOfOrderUpdated::False;

  if (side_ == Side::Bid) {
    if (newOrderInfo->dealSize_ < 0) newOrderInfo->dealSize_ *= -1;
    if (newOrderInfo->lastDealSize_ < 0) newOrderInfo->lastDealSize_ *= -1;
  } else {  // Side::Ask
    if (newOrderInfo->dealSize_ > 0) newOrderInfo->dealSize_ *= -1;
    if (newOrderInfo->lastDealSize_ > 0) newOrderInfo->lastDealSize_ *= -1;
  }

  noUsedToCalcPos_ = noUsedToCalcPos;

  if (newOrderInfo->orderStatus_ > orderStatus_) {
    if (notClosed()) {
      orderStatus_ = newOrderInfo->orderStatus_;
      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    } else {
      LOG_W("Orders change from one final state {} to another. {}",
            magic_enum::enum_name(orderStatus_), newOrderInfo->toShortStr());
    }
  } else if (newOrderInfo->orderStatus_ == orderStatus_) {
    if (OrderStatus::PartialFilled == newOrderInfo->orderStatus_ &&
        OrderStatus::PartialFilled == orderStatus_) {
    } else {
      LOG_I("The order status {} not changed. {}",
            magic_enum::enum_name(orderStatus_), newOrderInfo->toShortStr());
    }
  } else {
    LOG_I("The order status {} has become older. {}",
          magic_enum::enum_name(orderStatus_), newOrderInfo->toShortStr());
  }

  bool isValidOrderInfo = false;
  if (isDefinitelyGreaterThan(std::fabs(newOrderInfo->dealSize_),
                              std::fabs(dealSize_))) {
    if (isDefinitelyLessThan(std::fabs(newOrderInfo->dealSize_), orderSize_) ||
        isApproximatelyEqual(std::fabs(newOrderInfo->dealSize_), orderSize_)) {
      isValidOrderInfo = true;
    } else {
      LOG_W("Deal size greater {} than order size {}. {}",
            std::fabs(newOrderInfo->dealSize_), orderSize_,
            newOrderInfo->toShortStr());
    }
  } else {
    LOG_I("New deal size {} less than or equal to original deal size {}. {}",
          std::fabs(newOrderInfo->dealSize_), std::fabs(dealSize_),
          newOrderInfo->toShortStr());
  }

  if (isValidOrderInfo) {
    if (!isApproximatelyZero(newOrderInfo->avgDealPrice_)) {
      const auto oldDealSize = dealSize_;
      const auto newDealSize = newOrderInfo->dealSize_;
      const auto oldAvgDealPrice = avgDealPrice_;
      const auto newAvgDealPrice = newOrderInfo->avgDealPrice_;

      dealSize_ = newDealSize;
      avgDealPrice_ = newAvgDealPrice;

      if (!isApproximatelyZero(newOrderInfo->lastDealSize_)) {
        lastDealSize_ = newOrderInfo->lastDealSize_;
      } else {
        lastDealSize_ = newDealSize - oldDealSize;
      }

      if (!isApproximatelyZero(newOrderInfo->lastDealPrice_)) {
        lastDealPrice_ = newOrderInfo->lastDealPrice_;
      } else {
        const auto lastDealAmt =
            newAvgDealPrice * newDealSize - oldAvgDealPrice * oldDealSize;
        if (!isApproximatelyZero(lastDealSize_)) {
          lastDealPrice_ = lastDealAmt / lastDealSize_;
        } else {
          LOG_W("Last deal size {} is equal to 0 when calc last deal price. {}",
                lastDealSize_, newOrderInfo->toShortStr());
        }
      }

      if (!isApproximatelyZero(newOrderInfo->fee_)) {
        fee_ = newOrderInfo->fee_;
      } else {
        if (feeInfoCache != nullptr) {
          const auto dealAmt = std::fabs(newDealSize * newAvgDealPrice);
          fee_ = feeInfoCache->calcFee(this, dealAmt);
        } else {
          if (!isApproximatelyZero(fee_) && !isApproximatelyZero(oldDealSize)) {
            fee_ = newDealSize / oldDealSize * fee_;
          }
        }
      }

      strncpy(lastTradeId_, newOrderInfo->lastTradeId_,
              sizeof(lastTradeId_) - 1);
      if (newOrderInfo->lastDealTime_ != 0) {
        lastDealTime_ = newOrderInfo->lastDealTime_;
      }

      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    }
  }

  if (isApproximatelyZero(newOrderInfo->dealSize_) &&
      isApproximatelyZero(newOrderInfo->avgDealPrice_)) {
    if (!isApproximatelyZero(newOrderInfo->lastDealSize_) &&
        !isApproximatelyZero(newOrderInfo->lastDealPrice_)) {
      const auto oldDealSize = dealSize_;

      const auto dealAmt =
          dealSize_ * avgDealPrice_ +
          newOrderInfo->lastDealSize_ * newOrderInfo->lastDealPrice_;
      dealSize_ += newOrderInfo->lastDealSize_;
      avgDealPrice_ = dealAmt / dealSize_;

      lastDealSize_ = newOrderInfo->lastDealSize_;
      lastDealPrice_ = newOrderInfo->lastDealPrice_;

      if (feeInfoCache != nullptr) {
        fee_ = feeInfoCache->calcFee(this, dealAmt);
      } else {
        if (!isApproximatelyZero(fee_) && !isApproximatelyZero(oldDealSize)) {
          fee_ = dealSize_ / oldDealSize * fee_;
        }
      }

      strncpy(lastTradeId_, newOrderInfo->lastTradeId_,
              sizeof(lastTradeId_) - 1);
      if (newOrderInfo->lastDealTime_ != 0) {
        lastDealTime_ = newOrderInfo->lastDealTime_;
      }

      if (isApproximatelyEqual(std::fabs(dealSize_), orderSize_)) {
        orderStatus_ = OrderStatus::Filled;
        LOG_T(
            "Set order status to {} because of dealSize {} "
            "equal to orderSize {}. oldOrderInfo: {}; newOrderInfo: ",
            magic_enum::enum_name(orderStatus_), std::fabs(dealSize_),
            orderSize_, toShortStr(), newOrderInfo->toShortStr());

      } else if (isDefinitelyLessThan(std::fabs(dealSize_), orderSize_)) {
        orderStatus_ = OrderStatus::PartialFilled;
        LOG_T(
            "Set order status to {} because of dealSize {} "
            "less than orderSize {}. oldOrderInfo: {}; newOrderInfo: {}",
            magic_enum::enum_name(orderStatus_), std::fabs(dealSize_),
            orderSize_, toShortStr(), newOrderInfo->toShortStr());

      } else {
        orderStatus_ = OrderStatus::Filled;
        LOG_W(
            "Find deal size {} is greater than order size {} "
            "when handle rtn trade. {}",
            std::fabs(dealSize_), orderSize_, newOrderInfo->toShortStr());
      }

      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    }
  }

  if (newOrderInfo->exchOrderId_ != 0) {
    if (exchOrderId_ == 0) {
      exchOrderId_ = newOrderInfo->exchOrderId_;
      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    } else {
    }
  } else {
  }

  if (newOrderInfo->feeCurrency_[0] != '\0') {
    if (feeCurrency_[0] == '\0') {
      strncpy(feeCurrency_, newOrderInfo->feeCurrency_,
              sizeof(feeCurrency_) - 1);
      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    }
  }

  if (newOrderInfo->statusCode_ != 0) {
    if (statusCode_ == 0) {
      statusCode_ = newOrderInfo->statusCode_;
      isTheOrderInfoUpdated = IsSomeFieldOfOrderUpdated::True;
    }
  }

  return isTheOrderInfoUpdated;
}

IsTheOrderCanBeUsedCalcPos OrderInfo::updateByOrderInfoFromTDGW(
    const OrderInfoSPtr& newOrderInfo) {
  IsTheOrderCanBeUsedCalcPos isTheOrderCanBeUsedCalcPos =
      IsTheOrderCanBeUsedCalcPos::False;

  if (newOrderInfo->orderStatus_ > orderStatus_) {
    if (notClosed()) {
      orderStatus_ = newOrderInfo->orderStatus_;
    } else {
      LOG_W("Orders change from one final state {} to another. {}",
            magic_enum::enum_name(orderStatus_), newOrderInfo->toShortStr());
    }
  } else if (newOrderInfo->orderStatus_ == orderStatus_) {
    if (OrderStatus::PartialFilled == newOrderInfo->orderStatus_ &&
        OrderStatus::PartialFilled == orderStatus_) {
    } else {
      LOG_W("The order status has not changed and it is not PartialFilled. {}",
            newOrderInfo->toShortStr());
    }
  } else {
    LOG_W("The order status {} has become older. {}",
          magic_enum::enum_name(orderStatus_), newOrderInfo->toShortStr());
  }

  bool isValidOrderInfo = false;
  if (isDefinitelyGreaterThan(std::fabs(newOrderInfo->dealSize_),
                              std::fabs(dealSize_))) {
    if (isDefinitelyLessThan(std::fabs(newOrderInfo->dealSize_), orderSize_) ||
        isApproximatelyEqual(std::fabs(newOrderInfo->dealSize_), orderSize_)) {
      isValidOrderInfo = true;
    } else {
      LOG_W("Deal size {} greater than order size {}. {}",
            std::fabs(newOrderInfo->dealSize_), orderSize_,
            newOrderInfo->toShortStr());
    }
  } else {
    LOG_I("New deal size {} less than or equal to original deal size {}. {}",
          std::fabs(newOrderInfo->dealSize_), std::fabs(dealSize_),
          newOrderInfo->toShortStr());
  }

  if (isValidOrderInfo) {
    if (!isApproximatelyZero(newOrderInfo->avgDealPrice_)) {
      dealSize_ = newOrderInfo->dealSize_;
      avgDealPrice_ = newOrderInfo->avgDealPrice_;

      lastDealSize_ = newOrderInfo->lastDealSize_;
      lastDealPrice_ = newOrderInfo->lastDealPrice_;
      strncpy(lastTradeId_, newOrderInfo->lastTradeId_,
              sizeof(lastTradeId_) - 1);
      lastDealTime_ = newOrderInfo->lastDealTime_;

      fee_ = newOrderInfo->fee_;
      isTheOrderCanBeUsedCalcPos = IsTheOrderCanBeUsedCalcPos::True;
    }
  }

  if (newOrderInfo->exchOrderId_ != 0) {
    if (exchOrderId_ == 0) {
      exchOrderId_ = newOrderInfo->exchOrderId_;
    } else {
    }
  } else {
  }

  if (newOrderInfo->feeCurrency_[0] != '\0') {
    if (feeCurrency_[0] == '\0') {
      strncpy(feeCurrency_, newOrderInfo->feeCurrency_,
              sizeof(feeCurrency_) - 1);
    }
  }

  if (newOrderInfo->statusCode_ != 0) {
    if (statusCode_ == 0) {
      statusCode_ = newOrderInfo->statusCode_;
    }
  }

  return isTheOrderCanBeUsedCalcPos;
}

// clang-format off
std::string OrderInfo::toShortStr() const {
  const auto ret  = fmt::format(
    "["
    "{} "
    "stgInstId: {} "
    "orderId: {} "
    "exchOrderId: {} "
    "{} "
    "{} "
    "{} "
    "{} "
    "{} "
    "price: {} "
    "size: {} "
    "avgDealPrice: {} "
    "dealSize: {} "
    "lastDealPrice: {} "
    "lastDealSize: {} "
    "{} "
    "statusCode: {} "
    "statusMsg: {} "
    "]",
    shmHeader_.toStr(),
    stgInstId_,
    orderId_,
    exchOrderId_,
    GetMarketName(marketCode_),
    magic_enum::enum_name(symbolType_),
    symbolCode_,
    magic_enum::enum_name(side_),
    magic_enum::enum_name(posSide_),
    orderPrice_,
    orderSize_,
    avgDealPrice_,
    dealSize_,
    lastDealPrice_,
    lastDealSize_,
    magic_enum::enum_name(orderStatus_),
    statusCode_,
    GetStatusMsg(statusCode_)
    );
  return ret;
}
// clang-format on

std::string OrderInfo::toJson() const {
  rapidjson::StringBuffer strBuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
  writer.StartObject();

  writer.Key("productId");
  writer.Uint(productId_);
  writer.Key("userId");
  writer.Uint(userId_);
  writer.Key("acctId");
  writer.Uint(acctId_);
  writer.Key("stgId");
  writer.Uint(stgId_);
  writer.Key("stgInstId");
  writer.Uint(stgInstId_);
  writer.Key("algoId");
  writer.Uint(algoId_);
  writer.Key("orderId");
  writer.Uint64(orderId_);
  writer.Key("exchOrderId");
  writer.Uint64(exchOrderId_);
  writer.Key("parentOrderId");
  writer.Uint64(parentOrderId_);
  writer.Key("marketCode");
  writer.Key(GetMarketName(marketCode_).c_str());
  writer.Key("symbolType");
  writer.Key(ENUM_TO_STR(symbolType_).c_str());
  writer.Key("symbolCode");
  writer.Key(symbolCode_);
  writer.Key("exchSymbolCode");
  writer.Key(exchSymbolCode_);
  writer.Key("side");
  writer.Key(ENUM_TO_STR(side_).c_str());
  writer.Key("posSide");
  writer.Key(ENUM_TO_STR(posSide_).c_str());
  writer.Key("orderPrice");
  writer.Double(orderPrice_);
  writer.Key("orderSize");
  writer.Double(orderSize_);
  writer.Key("parValue");
  writer.Uint(parValue_);
  writer.Key("orderType");
  writer.Key(ENUM_TO_STR(orderType_).c_str());
  writer.Key("orderTypeExtra");
  writer.Key(ENUM_TO_STR(orderTypeExtra_).c_str());
  writer.Key("orderTime");
  writer.Uint64(orderTime_);
  writer.Key("fee");
  writer.Double(fee_);
  writer.Key("feeCurrency");
  writer.String(feeCurrency_);
  writer.Key("dealSize");
  writer.Double(dealSize_);
  writer.Key("avgDealPrice");
  writer.Double(avgDealPrice_);
  writer.Key("lastTradeId");
  writer.String(lastTradeId_);
  writer.Key("lastDealPrice");
  writer.Double(lastDealPrice_);
  writer.Key("lastDealSize");
  writer.Double(lastDealSize_);
  writer.Key("lastDealTime");
  writer.Uint64(lastDealTime_);
  writer.Key("orderStatus");
  writer.Key(ENUM_TO_STR(orderStatus_).c_str());
  writer.Key("statusCode");
  writer.Int(statusCode_);

  writer.EndObject();

  return strBuf.GetString();
}

// clang-format off
std::string OrderInfo::getSqlOfReplace() const {
  const auto sql = fmt::format(
    "REPLACE INTO `BetterQuant`.`orderInfo` ("
    "`productId`,"
    "`userId`,"
    "`acctId`,"
    "`stgId`,"
    "`stgInstId`,"
    "`algoId`,"
    "`orderId`,"
    "`exchOrderId`,"
    "`parentOrderId`,"
    "`marketCode`,"
    "`symbolType`,"
    "`symbolCode`,"
    "`exchSymbolCode`,"
    "`side`,"
    "`posSide`,"
    "`orderPrice`,"
    "`orderSize`,"
    "`parValue`,"
    "`orderType`,"
    "`orderTypeExtra`,"
    "`orderTime`,"
    "`fee`,"
    "`feeCurrency`,"
    "`dealSize`,"
    "`avgDealPrice`,"
    "`lastTradeId`,"
    "`lastDealPrice`,"
    "`lastDealSize`,"
    "`lastDealTime`,"
    "`orderStatus`,"
    "`statusCode`,"
    "`statusMsg`"
    ")"
  "VALUES"
  "("
    " {}, "  // productId 
    " {}, "  // userId
    " {}, "  // acctId
    " {}, "  // stgId
    " {}, "  // stgInstId
    " {}, "  // algoId
    "'{}',"  // orderId
    "'{}',"  // exchOrderId
    "'{}',"  // parentOrderId
    "'{}',"  // marketCode
    "'{}',"  // symbolType
    "'{}',"  // symbolCode
    "'{}',"  // exchSymbolCode
    "'{}',"  // side
    "'{}',"  // posSide
    "'{}',"  // orderPrice
    "'{}',"  // orderSize
    "'{}',"  // parValue
    "'{}',"  // orderType
    "'{}',"  // orderTypeExtra
    "'{}',"  // orderTime
    "'{}',"  // fee
    "'{}',"  // feeCurrency
    "'{}',"  // dealSize
    "'{}',"  // avgDealPrice
    "'{}',"  // lastTradeId
    "'{}',"  // lastDealPrice
    "'{}',"  // lastDealSize
    "'{}',"  // lastDealTime
    " {}, "  // orderStatus
    " {}, "  // statusCode
    "'{}' "  // statusMsg
  "); ",
    productId_,
    userId_,
    acctId_,
    stgId_,
    stgInstId_,
    algoId_,
    orderId_,
    exchOrderId_,
    parentOrderId_,
    GetMarketName(marketCode_),
    magic_enum::enum_name(symbolType_),
    symbolCode_,
    exchSymbolCode_,
    magic_enum::enum_name(side_),
    magic_enum::enum_name(posSide_),
    orderPrice_,
    orderSize_,
    parValue_,
    magic_enum::enum_name(orderType_),
    magic_enum::enum_name(orderTypeExtra_),
    ConvertTsToDBTime(orderTime_),
    fee_,
    feeCurrency_,
    dealSize_,
    avgDealPrice_,
    lastTradeId_,
    lastDealPrice_,
    lastDealSize_,
    ConvertTsToDBTime(lastDealTime_),
    magic_enum::enum_integer(orderStatus_),
    statusCode_,
    GetStatusMsg(statusCode_)
  );
  return sql;
}
// clang-format on

// clang-format off
std::string OrderInfo::getSqlOfUSPOrderInfoUpdate() const {
  const auto sql = fmt::format(
  "call `uspOrderInfoUpdate` "
  "("
    " {}, "  // productId 
    " {}, "  // userId
    " {}, "  // acctId
    " {}, "  // stgId
    " {}, "  // stgInstId
    " {}, "  // algoId
    "'{}',"  // orderId
    "'{}',"  // exchOrderId
    "'{}',"  // parentOrderId
    "'{}',"  // marketCode
    "'{}',"  // symbolType
    "'{}',"  // symbolCode
    "'{}',"  // exchSymbolCode
    "'{}',"  // side
    "'{}',"  // posSide
    "'{}',"  // orderPrice
    "'{}',"  // orderSize
    "'{}',"  // parValue
    "'{}',"  // orderType
    "'{}',"  // orderTypeExtra
    "'{}',"  // orderTime
    "'{}',"  // simedTDInfo
    "'{}',"  // fee
    "'{}',"  // feeCurrency
    "'{}',"  // dealSize
    "'{}',"  // avgDealPrice
    "'{}',"  // lastTradeId
    "'{}',"  // lastDealPrice
    "'{}',"  // lastDealSize
    "'{}',"  // lastDealTime
    " {}, "  // orderStatus
    " {}, "  // noUsedToCalcPos
    " {}, "  // statusCode
    "'{}' "  // statusMsg
  "); ",
    productId_,
    userId_,
    acctId_,
    stgId_,
    stgInstId_,
    algoId_,
    orderId_,
    exchOrderId_,
    parentOrderId_,
    GetMarketName(marketCode_),
    magic_enum::enum_name(symbolType_),
    symbolCode_,
    exchSymbolCode_,
    magic_enum::enum_name(side_),
    magic_enum::enum_name(posSide_),
    orderPrice_,
    orderSize_,
    parValue_,
    magic_enum::enum_name(orderType_),
    magic_enum::enum_name(orderTypeExtra_),
    ConvertTsToDBTime(orderTime_),
#ifdef SIMED_MODE
    simedTDInfo_,
#else
    "",
#endif
    fee_,
    feeCurrency_,
    dealSize_,
    avgDealPrice_,
    lastTradeId_,
    lastDealPrice_,
    lastDealSize_,
    ConvertTsToDBTime(lastDealTime_),
    magic_enum::enum_integer(orderStatus_),
    noUsedToCalcPos_,
    statusCode_,
    GetStatusMsg(statusCode_)
  );
  return sql;
}
// clang-format on

Decimal OrderInfo::getFeeOfLastTrade() const {
  if (isApproximatelyZero(dealSize_)) {
    return 0;
  }
  const auto ret = (lastDealSize_ / dealSize_) * fee_;
  return ret;
}

std::string OrderInfo::getPosKey() const {
  const auto ret = fmt::format(
      "{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}", productId_, userId_, acctId_,
      stgId_, stgInstId_, algoId_, GetMarketName(marketCode_),
      magic_enum::enum_name(symbolType_), symbolCode_,
      magic_enum::enum_name(side_), magic_enum::enum_name(posSide_), parValue_,
      feeCurrency_);
  return ret;
}

std::string OrderInfo::getPosKeyOfBid() const {
  const auto ret = fmt::format(
      "{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}", productId_, userId_, acctId_,
      stgId_, stgInstId_, algoId_, GetMarketName(marketCode_),
      magic_enum::enum_name(symbolType_), symbolCode_,
      magic_enum::enum_name(Side::Bid), magic_enum::enum_name(posSide_),
      parValue_, feeCurrency_);
  return ret;
}

std::string OrderInfo::getPosKeyOfAsk() const {
  const auto ret = fmt::format(
      "{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}/{}", productId_, userId_, acctId_,
      stgId_, stgInstId_, algoId_, GetMarketName(marketCode_),
      magic_enum::enum_name(symbolType_), symbolCode_,
      magic_enum::enum_name(Side::Ask), magic_enum::enum_name(posSide_),
      parValue_, feeCurrency_);
  return ret;
}

OrderInfoSPtr MakeOrderInfo(const db::orderInfo::RecordSPtr& recOrderInfo) {
  auto orderInfo = std::make_shared<OrderInfo>();

  orderInfo->productId_ = recOrderInfo->productId;
  orderInfo->userId_ = recOrderInfo->userId;
  orderInfo->acctId_ = recOrderInfo->acctId;
  orderInfo->stgId_ = recOrderInfo->stgId;
  orderInfo->stgInstId_ = recOrderInfo->stgInstId;
  orderInfo->algoId_ = recOrderInfo->algoId;

  if (!recOrderInfo->orderId.empty()) {
    orderInfo->orderId_ = CONV(OrderId, recOrderInfo->orderId);
  }
  if (!recOrderInfo->exchOrderId.empty()) {
    orderInfo->exchOrderId_ = CONV(ExchOrderId, recOrderInfo->exchOrderId);
  }
  if (!recOrderInfo->parentOrderId.empty()) {
    orderInfo->parentOrderId_ = CONV(OrderId, recOrderInfo->parentOrderId);
  }

  orderInfo->marketCode_ = GetMarketCode(recOrderInfo->marketCode);
  orderInfo->symbolType_ =
      magic_enum::enum_cast<SymbolType>(recOrderInfo->symbolType).value();
  strncpy(orderInfo->symbolCode_, recOrderInfo->symbolCode.c_str(),
          sizeof(orderInfo->symbolCode_) - 1);
  strncpy(orderInfo->exchSymbolCode_, recOrderInfo->exchSymbolCode.c_str(),
          sizeof(orderInfo->exchSymbolCode_) - 1);

  orderInfo->side_ = magic_enum::enum_cast<Side>(recOrderInfo->side).value();
  orderInfo->posSide_ =
      magic_enum::enum_cast<PosSide>(recOrderInfo->posSide).value();

  orderInfo->orderPrice_ = CONV(Decimal, recOrderInfo->orderPrice);
  orderInfo->orderSize_ = CONV(Decimal, recOrderInfo->orderSize);

  orderInfo->parValue_ = recOrderInfo->parValue;

  orderInfo->orderType_ =
      magic_enum::enum_cast<OrderType>(recOrderInfo->orderType).value();
  orderInfo->orderTypeExtra_ =
      magic_enum::enum_cast<OrderTypeExtra>(recOrderInfo->orderTypeExtra)
          .value();

  orderInfo->orderTime_ = ConvertDBTimeToTS(recOrderInfo->orderTime);

  orderInfo->fee_ = CONV(Decimal, recOrderInfo->fee);
  strncpy(orderInfo->feeCurrency_, recOrderInfo->feeCurrency.c_str(),
          sizeof(orderInfo->feeCurrency_) - 1);

  orderInfo->dealSize_ = CONV(Decimal, recOrderInfo->dealSize);
  orderInfo->avgDealPrice_ = CONV(Decimal, recOrderInfo->avgDealPrice);

  strncpy(orderInfo->lastTradeId_, recOrderInfo->lastTradeId.c_str(),
          sizeof(orderInfo->lastTradeId_) - 1);
  orderInfo->lastDealPrice_ = CONV(Decimal, recOrderInfo->lastDealPrice);
  orderInfo->lastDealSize_ = CONV(Decimal, recOrderInfo->lastDealSize);
  orderInfo->lastDealTime_ = ConvertDBTimeToTS(recOrderInfo->lastDealTime);

  orderInfo->orderStatus_ =
      magic_enum::enum_cast<OrderStatus>(recOrderInfo->orderStatus).value();

  orderInfo->noUsedToCalcPos_ = recOrderInfo->noUsedToCalcPos;
  orderInfo->statusCode_ = recOrderInfo->statusCode;

  return orderInfo;
}

OrderInfoSPtr MakeOrderInfo(const db::tradeInfo::RecordSPtr& recTradeInfo) {
  auto orderInfo = std::make_shared<OrderInfo>();

  orderInfo->productId_ = recTradeInfo->productId;
  orderInfo->userId_ = recTradeInfo->userId;
  orderInfo->acctId_ = recTradeInfo->acctId;
  orderInfo->stgId_ = recTradeInfo->stgId;
  orderInfo->stgInstId_ = recTradeInfo->stgInstId;
  orderInfo->algoId_ = recTradeInfo->algoId;

  if (!recTradeInfo->orderId.empty()) {
    orderInfo->orderId_ = CONV(OrderId, recTradeInfo->orderId);
  }
  if (!recTradeInfo->exchOrderId.empty()) {
    orderInfo->exchOrderId_ = CONV(ExchOrderId, recTradeInfo->exchOrderId);
  }
  if (!recTradeInfo->parentOrderId.empty()) {
    orderInfo->parentOrderId_ = CONV(OrderId, recTradeInfo->parentOrderId);
  }

  orderInfo->marketCode_ = GetMarketCode(recTradeInfo->marketCode);
  orderInfo->symbolType_ =
      magic_enum::enum_cast<SymbolType>(recTradeInfo->symbolType).value();
  strncpy(orderInfo->symbolCode_, recTradeInfo->symbolCode.c_str(),
          sizeof(orderInfo->symbolCode_) - 1);
  strncpy(orderInfo->exchSymbolCode_, recTradeInfo->exchSymbolCode.c_str(),
          sizeof(orderInfo->exchSymbolCode_) - 1);

  orderInfo->side_ = magic_enum::enum_cast<Side>(recTradeInfo->side).value();
  orderInfo->posSide_ =
      magic_enum::enum_cast<PosSide>(recTradeInfo->posSide).value();

  orderInfo->orderPrice_ = CONV(Decimal, recTradeInfo->orderPrice);
  orderInfo->orderSize_ = CONV(Decimal, recTradeInfo->orderSize);

  orderInfo->parValue_ = recTradeInfo->parValue;

  orderInfo->orderType_ =
      magic_enum::enum_cast<OrderType>(recTradeInfo->orderType).value();
  orderInfo->orderTypeExtra_ =
      magic_enum::enum_cast<OrderTypeExtra>(recTradeInfo->orderTypeExtra)
          .value();

  orderInfo->orderTime_ = ConvertDBTimeToTS(recTradeInfo->orderTime);

  orderInfo->fee_ = CONV(Decimal, recTradeInfo->fee);
  strncpy(orderInfo->feeCurrency_, recTradeInfo->feeCurrency.c_str(),
          sizeof(orderInfo->feeCurrency_) - 1);

  orderInfo->dealSize_ = CONV(Decimal, recTradeInfo->dealSize);
  orderInfo->avgDealPrice_ = CONV(Decimal, recTradeInfo->avgDealPrice);

  strncpy(orderInfo->lastTradeId_, recTradeInfo->lastTradeId.c_str(),
          sizeof(orderInfo->lastTradeId_) - 1);
  orderInfo->lastDealPrice_ = CONV(Decimal, recTradeInfo->lastDealPrice);
  orderInfo->lastDealSize_ = CONV(Decimal, recTradeInfo->lastDealSize);
  orderInfo->lastDealTime_ = ConvertDBTimeToTS(recTradeInfo->lastDealTime);

  orderInfo->orderStatus_ =
      magic_enum::enum_cast<OrderStatus>(recTradeInfo->orderStatus).value();

  orderInfo->noUsedToCalcPos_ = recTradeInfo->noUsedToCalcPos;
  orderInfo->statusCode_ = recTradeInfo->statusCode;

  return orderInfo;
}

OrderInfoSPtr MakeOrderInfo(const StgInstInfoSPtr& stgInstInfo, AcctId acctId,
                            MarketCode marketCode,
                            const std::string& symbolCode, Side side,
                            PosSide posSide, Decimal orderPrice,
                            Decimal orderSize, AlgoId algoId,
                            const std::string& simedTDInfo) {
  const auto now = GetTotalUSSince1970();
  const auto orderInfo = std::make_shared<OrderInfo>();
  orderInfo->orderTime_ = now;

  orderInfo->productId_ = stgInstInfo->productId_;
  orderInfo->userId_ = stgInstInfo->userId_;
  orderInfo->acctId_ = acctId;

  orderInfo->stgId_ = stgInstInfo->stgId_;
  orderInfo->stgInstId_ = stgInstInfo->stgInstId_;
  orderInfo->algoId_ = algoId;

  orderInfo->marketCode_ = marketCode;
  strncpy(orderInfo->symbolCode_, symbolCode.c_str(),
          sizeof(orderInfo->symbolCode_) - 1);

  orderInfo->side_ = side;
  orderInfo->posSide_ = posSide;

  orderInfo->orderPrice_ = orderPrice;
  orderInfo->orderSize_ = orderSize;

  orderInfo->orderType_ = OrderType::Limit;
  orderInfo->orderTypeExtra_ = OrderTypeExtra::Normal;

#ifdef SIMED_MODE
  strncpy(orderInfo->simedTDInfo_, simedTDInfo.c_str(),
          sizeof(orderInfo->simedTDInfo_) - 1);
#endif

  return orderInfo;
}

}  // namespace bq
