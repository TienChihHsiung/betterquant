/*!
 * \file BQTraderSpi.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/31
 *
 * \brief
 */

#include "BQTraderSpi.hpp"

#include "Config.hpp"
#include "OrdMgr.hpp"
#include "SHMIPC.hpp"
#include "TDGatewayOfXTP.hpp"
#include "TDSvcOfCN.hpp"
#include "TDSvcOfXTPUtil.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/DataStruOfTD.hpp"
#include "def/Def.hpp"
#include "def/StatusCode.hpp"
#include "def/SyncTask.hpp"
#include "util/ClientId2OrderId.hpp"
#include "util/Datetime.hpp"
#include "util/ExternalStatusCodeCache.hpp"
#include "util/FeeInfoCache.hpp"
#include "util/Float.hpp"
#include "util/Logger.hpp"

using namespace std;

namespace bq::td::svc::xtp {

BQTraderSpi::BQTraderSpi(TDSvcOfCN* tdSvc, TraderApi* api)
    : tdSvc_(tdSvc), api_(api) {}

BQTraderSpi::~BQTraderSpi() {}

void BQTraderSpi::OnDisconnected(uint64_t sessionId, int reason) {
  LOG_W("Disconnect from trader server. session id {} reason {}", sessionId,
        reason);

  const auto apiInfo = Config::get_const_instance().getApiInfo();

  std::uint64_t tmpSessionId = 0;
  while (tmpSessionId == 0) {
    tmpSessionId = api_->Login(
        apiInfo->traderServerIP_.c_str(), apiInfo->traderServerPort_,
        apiInfo->traderUsername_.c_str(), apiInfo->traderPassword_.c_str(),
        XTP_PROTOCOL_TCP, apiInfo->localIP_.c_str());
    if (tmpSessionId == 0) {
      XTPRI* errorInfo = api_->GetApiLastError();
      LOG_W("Login to server failed. [{} - {}]", errorInfo->error_id,
            errorInfo->error_msg);

      std::this_thread::sleep_for(
          std::chrono::seconds(apiInfo->secIntervalOfReconnect_));
    }
  };

  const auto tdGateway =
      std::dynamic_pointer_cast<TDGatewayOfXTP>(tdSvc_->getTDGateway());
  tdGateway->setSessionId(tmpSessionId);

  tdSvc_->setTradingDay(api_->GetTradingDay());
  LOG_I("Login to trader server {}:{} success. [tradingDay: {}; sessionId: {}]",
        apiInfo->traderServerIP_.c_str(), apiInfo->traderServerPort_,
        tdSvc_->getTradingDay(), tmpSessionId);
}

void BQTraderSpi::OnOrderEvent(XTPOrderInfo* orderInfo, XTPRI* errorInfo,
                               uint64_t sessionId) {
  auto orderInfoFromExch = std::make_shared<OrderInfo>();
  orderInfoFromExch->orderId_ =
      tdSvc_->getClientId2OrderId()->getOrderId(orderInfo->order_client_id);
  if (orderInfoFromExch->orderId_ == 0) {
    LOG_W(
        "Handle rtn of order failed "
        "beause of can't find client id {} in client id info.",
        orderInfo->order_client_id);
    return;
  }

  switch (orderInfo->order_status) {
    case XTP_ORDER_STATUS_NOTRADEQUEUEING: {
      orderInfoFromExch->orderStatus_ = OrderStatus::ConfirmedByExch;
      orderInfoFromExch->exchOrderId_ = orderInfo->order_xtp_id;
      LOG_I("Recv rtn order {} of order id {}. ",
            magic_enum::enum_name(orderInfoFromExch->orderStatus_),
            orderInfoFromExch->orderId_);
      break;
    }

    case XTP_ORDER_STATUS_ALLTRADED: {
      LOG_T("Recv rtn order {} of order id {}. ",
            magic_enum::enum_name(OrderStatus::Filled),
            orderInfoFromExch->orderId_);
      return;
    }

    case XTP_ORDER_STATUS_PARTTRADEDNOTQUEUEING: {
      orderInfoFromExch->orderStatus_ = OrderStatus::PartialFilledCanceled;
      orderInfoFromExch->exchOrderId_ = orderInfo->order_xtp_id;
      orderInfoFromExch->dealSize_ = orderInfo->qty_traded;
      const auto dealAmt = orderInfo->trade_amount;
      if (!isApproximatelyZero(orderInfoFromExch->dealSize_)) {
        orderInfoFromExch->avgDealPrice_ =
            dealAmt / orderInfoFromExch->dealSize_;
      } else {
        LOG_W("Recv invalid dealSize {} in rtn order of order id {}. {}",
              orderInfoFromExch->dealSize_, orderInfoFromExch->orderId_,
              orderInfoFromExch->toShortStr());
        return;
      }
      tdSvc_->getClientId2OrderId()->remove(orderInfo->order_client_id);
      LOG_I("Recv rtn order {} of order id {}. {}",
            magic_enum::enum_name(orderInfoFromExch->orderStatus_),
            orderInfoFromExch->orderId_, orderInfoFromExch->toShortStr());
      break;
    }

    case XTP_ORDER_STATUS_CANCELED: {
      orderInfoFromExch->orderStatus_ = OrderStatus::Canceled;
      orderInfoFromExch->exchOrderId_ = orderInfo->order_xtp_id;
      tdSvc_->getClientId2OrderId()->remove(orderInfo->order_client_id);
      LOG_I("Recv rtn order {} of order id {}. ",
            magic_enum::enum_name(orderInfoFromExch->orderStatus_),
            orderInfoFromExch->orderId_);
      break;
    }

    case XTP_ORDER_STATUS_REJECTED: {
      orderInfoFromExch->orderStatus_ = OrderStatus::Failed;
      orderInfoFromExch->exchOrderId_ = orderInfo->order_xtp_id;
      tdSvc_->getClientId2OrderId()->remove(orderInfo->order_client_id);
      if (errorInfo && (errorInfo->error_id > 0)) {
        const auto apiInfo = Config::get_const_instance().getApiInfo();

        const auto externalStatusCode = fmt::format("{}", errorInfo->error_id);
        const auto externalStatusMsg = errorInfo->error_msg;
        orderInfoFromExch->statusCode_ =
            tdSvc_->getExternalStatusCodeCache()
                ->getAndSetStatusCodeIfNotExists(apiInfo->apiName_, 0,
                                                 externalStatusCode,
                                                 externalStatusMsg, -1);
        LOG_W("Recv rtn order {} of of order id {}. [{} - {}] {} - {}",
              magic_enum::enum_name(orderInfoFromExch->orderStatus_),
              orderInfoFromExch->orderId_, orderInfoFromExch->statusCode_,
              GetStatusMsg(orderInfoFromExch->statusCode_), externalStatusCode,
              externalStatusMsg);
      }
      break;
    }

    default:
      LOG_W("Recv unknown order status {}. {}", orderInfo->order_status,
            orderInfoFromExch->toShortStr());
      return;
  }

  const auto [isTheOrderInfoUpdated, orderInfoUpdated] =
      tdSvc_->getOrdMgr()->updateByOrderInfoFromExch(
          orderInfoFromExch, tdSvc_->getNextNoUsedToCalcPos(), DeepClone::True,
          LockFunc::True, tdSvc_->getFeeInfoCache());
  if (isTheOrderInfoUpdated == IsSomeFieldOfOrderUpdated::False) {
    return;
  }

  tdSvc_->getSHMCliOfTDSrv()->asyncSendMsgWithZeroCopy(
      [&](void* shmBuf) {
        InitMsgBody(shmBuf, *orderInfoUpdated);
#ifndef OPT_LOG
        LOG_I("Send order ret. {}",
              static_cast<OrderInfo*>(shmBuf)->toShortStr());
#endif
      },
      MSG_ID_ON_ORDER_RET, sizeof(OrderInfo));

  tdSvc_->cacheSyncTaskGroup(MSG_ID_ON_ORDER_RET, orderInfoUpdated,
                             SyncToRiskMgr::True, SyncToDB::True);
}

void BQTraderSpi::OnTradeEvent(XTPTradeReport* tradeInfo, uint64_t sessionId) {
  auto orderInfoFromExch = std::make_shared<OrderInfo>();
  orderInfoFromExch->orderId_ =
      tdSvc_->getClientId2OrderId()->getOrderId(tradeInfo->order_client_id);
  if (orderInfoFromExch->orderId_ == 0) {
    LOG_I(
        "Handle rtn of trade failed "
        "beause of can't find client id {} in client id info.",
        tradeInfo->order_client_id);
    return;
  }

  orderInfoFromExch->exchOrderId_ = tradeInfo->order_xtp_id;
  orderInfoFromExch->lastDealPrice_ = tradeInfo->price;
  orderInfoFromExch->lastDealSize_ = tradeInfo->quantity;
  strncpy(orderInfoFromExch->lastTradeId_, tradeInfo->exec_id,
          sizeof(orderInfoFromExch->lastTradeId_) - 1);
  orderInfoFromExch->lastDealTime_ =
      ConvertDatetimeToTs(tradeInfo->trade_time) - USEC_DIFF_OF_CN;

  LOG_I("Recv rtn trade. {}", orderInfoFromExch->toShortStr());

  const auto [isTheOrderInfoUpdated, orderInfoUpdated] =
      tdSvc_->getOrdMgr()->updateByOrderInfoFromExch(
          orderInfoFromExch, tdSvc_->getNextNoUsedToCalcPos(), DeepClone::True,
          LockFunc::True, tdSvc_->getFeeInfoCache());
  if (isTheOrderInfoUpdated == IsSomeFieldOfOrderUpdated::False) {
    return;
  }

  tdSvc_->getSHMCliOfTDSrv()->asyncSendMsgWithZeroCopy(
      [&](void* shmBuf) {
        InitMsgBody(shmBuf, *orderInfoUpdated);
#ifndef OPT_LOG
        LOG_I("Send order ret. {}",
              static_cast<OrderInfo*>(shmBuf)->toShortStr());
#endif
      },
      MSG_ID_ON_ORDER_RET, sizeof(OrderInfo));

  tdSvc_->cacheSyncTaskGroup(MSG_ID_ON_ORDER_RET, orderInfoUpdated,
                             SyncToRiskMgr::True, SyncToDB::True);
}

void BQTraderSpi::OnQueryOrderEx(XTPOrderInfoEx* orderInfo, XTPRI* errorInfo,
                                 int requestId, bool isLast,
                                 uint64_t sessionId) {
  auto orderInfoFromExch = std::make_shared<OrderInfo>();
  orderInfoFromExch->orderId_ =
      tdSvc_->getClientId2OrderId()->getOrderId(orderInfo->order_client_id);
  if (orderInfoFromExch->orderId_ == 0) {
    LOG_W(
        "Handle rtn of order failed "
        "beause of can't find client id {} in client id info.",
        orderInfo->order_client_id);
    return;
  }

  orderInfoFromExch->exchOrderId_ = orderInfo->order_xtp_id;
  orderInfoFromExch->dealSize_ = orderInfo->qty_traded;
  const auto dealAmt = orderInfo->trade_amount;
  if (!isApproximatelyZero(orderInfoFromExch->dealSize_)) {
    orderInfoFromExch->avgDealPrice_ = dealAmt / orderInfoFromExch->dealSize_;
  }
  orderInfoFromExch->orderStatus_ = GetOrderStatus(orderInfo->order_status);
  if (orderInfoFromExch->orderStatus_ == OrderStatus::Others) {
    LOG_W(
        "Handle rtn of order of order id {} failed "
        "beause of receive invalid order status {}.",
        orderInfoFromExch->orderId_, orderInfo->order_status);
    return;
  }

  LOG_I("Query order info. {}", orderInfoFromExch->toShortStr());

  const auto [isTheOrderInfoUpdated, orderInfoUpdated] =
      tdSvc_->getOrdMgr()->updateByOrderInfoFromExch(
          orderInfoFromExch, tdSvc_->getNextNoUsedToCalcPos(), DeepClone::True,
          LockFunc::True, tdSvc_->getFeeInfoCache());
  if (isTheOrderInfoUpdated == IsSomeFieldOfOrderUpdated::False) {
    return;
  }

  tdSvc_->getSHMCliOfTDSrv()->asyncSendMsgWithZeroCopy(
      [&](void* shmBuf) {
        InitMsgBody(shmBuf, *orderInfoUpdated);
#ifndef OPT_LOG
        LOG_I("Send order ret. {}",
              static_cast<OrderInfo*>(shmBuf)->toShortStr());
#endif
      },
      MSG_ID_ON_ORDER_RET, sizeof(OrderInfo));

  tdSvc_->cacheSyncTaskGroup(MSG_ID_ON_ORDER_RET, orderInfoUpdated,
                             SyncToRiskMgr::True, SyncToDB::True);
}

void BQTraderSpi::OnCancelOrderError(XTPOrderCancelInfo* cancelInfo,
                                     XTPRI* errorInfo, uint64_t sessionId) {
  if (errorInfo && (errorInfo->error_id > 0)) {
    const auto apiInfo = Config::get_const_instance().getApiInfo();
    const auto [statusCode, orderInfo] =
        tdSvc_->getOrdMgr()->getOrderInfoByExchOrderId(
            cancelInfo->order_xtp_id, DeepClone::True, LockFunc::True);
    if (statusCode != 0) {
      LOG_W(
          "Handle cancel order ret failed because of "
          "can't find order info of exchOrderId {} in ordmgr.",
          cancelInfo->order_xtp_id);
      return;
    }
    orderInfo->exchOrderId_ = cancelInfo->order_xtp_id;
    const auto externalStatusCode = fmt::format("{}", errorInfo->error_id);
    const auto externalStatusMsg = errorInfo->error_msg;
    orderInfo->statusCode_ =
        tdSvc_->getExternalStatusCodeCache()->getAndSetStatusCodeIfNotExists(
            apiInfo->apiName_, 0, externalStatusCode, externalStatusMsg, -1);

    tdSvc_->getSHMCliOfTDSrv()->asyncSendMsgWithZeroCopy(
        [&](void* shmBuf) {
          InitMsgBody(shmBuf, *orderInfo);
#ifndef OPT_LOG
          LOG_I("Send cancel order ret. {}",
                static_cast<OrderInfo*>(shmBuf)->toShortStr());
#endif
        },
        MSG_ID_ON_CANCEL_ORDER_RET, sizeof(OrderInfo));

    // not sync to db
    tdSvc_->cacheSyncTaskGroup(MSG_ID_ON_CANCEL_ORDER_RET, orderInfo,
                               SyncToRiskMgr::True, SyncToDB::False);
    LOG_W("Cancel order failed. [{} - {}] {}", errorInfo->error_id,
          errorInfo->error_msg, orderInfo->toShortStr());
  }
}

void BQTraderSpi::OnQueryPosition(XTPQueryStkPositionRsp* pos, XTPRI* errorInfo,
                                  int requestId, bool isLast,
                                  uint64_t sessionId) {
  if (errorInfo && (errorInfo->error_id > 0)) {
    LOG_W("Query position info failed. [{} - {}]", errorInfo->error_id,
          errorInfo->error_msg);
    return;
  }
  LOG_I("symbolCode: {}; totalPos: {}; PrePos: {}; sellablePos: {}",
        pos->ticker, pos->total_qty, pos->yesterday_position,
        pos->sellable_qty);
}

void BQTraderSpi::OnQueryAsset(XTPQueryAssetRsp* asset, XTPRI* errorInfo,
                               int requestId, bool isLast, uint64_t sessionId) {
  if (errorInfo && (errorInfo->error_id > 0)) {
    LOG_W("Query position info failed. [{} - {}]", errorInfo->error_id,
          errorInfo->error_msg);
    return;
  }
  LOG_I("total: {}; available: {}; securityAsset: {}", asset->total_asset,
        asset->buying_power, asset->security_asset);
}

}  // namespace bq::td::svc::xtp
