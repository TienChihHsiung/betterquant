/*!
 * \file TDSvcOfXTP.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/01
 *
 * \brief
 */

#include "TDGatewayOfXTP.hpp"

#include "BQTraderSpi.hpp"
#include "Config.hpp"
#include "OrdMgr.hpp"
#include "SHMIPC.hpp"
#include "TDSvcOfCN.hpp"
#include "TDSvcOfXTPUtil.hpp"
#include "def/OrderInfo.hpp"
#include "def/StatusCode.hpp"
#include "def/SyncTask.hpp"
#include "util/ClientId2OrderId.hpp"
#include "util/ExternalStatusCodeCache.hpp"
#include "util/Logger.hpp"

namespace bq::td::svc::xtp {

int TDGatewayOfXTP::doInit() {
  const auto apiInfo = Config::get_const_instance().getApiInfo();
#ifdef _NDEBUG
  XTP_LOG_LEVEL logLevel = XTP_LOG_LEVEL_INFO;
#else
  XTP_LOG_LEVEL logLevel = XTP_LOG_LEVEL_DEBUG;
#endif
  api_ = TraderApi::CreateTraderApi(apiInfo->clientId_,
                                    apiInfo->loggerPath_.c_str(), logLevel);

  api_->SetHeartBeatInterval(apiInfo->secIntervalOfHeartBeat_);

  api_->SubscribePublicTopic(XTP_TERT_QUICK);

  api_->SetSoftwareKey(apiInfo->softwareKey_.c_str());

  api_->SetSoftwareVersion("1.0.0");

  auto spi = new BQTraderSpi(tdSvc_, api_);
  if (!spi) {
    LOG_W("Create spi failed.");
    return -1;
  }

  api_->RegisterSpi(spi);

  sessionId_ = api_->Login(
      apiInfo->traderServerIP_.c_str(), apiInfo->traderServerPort_,
      apiInfo->traderUsername_.c_str(), apiInfo->traderPassword_.c_str(),
      XTP_PROTOCOL_TCP, apiInfo->localIP_.c_str());
  if (0 == sessionId_) {
    XTPRI* errorInfo = api_->GetApiLastError();
    LOG_E("Login to trader server {}:{} failed. [{} - {}]",
          apiInfo->traderServerIP_.c_str(), apiInfo->traderServerPort_,
          errorInfo->error_id, errorInfo->error_msg);
    return -1;
  }

  const auto tradingDay = api_->GetTradingDay();
  const auto filename = tdSvc_->getFilenameOfClientId2OrderId(tradingDay);
  tdSvc_->getClientId2OrderId()->load(filename);

  tdSvc_->setTradingDay(tradingDay);
  LOG_I("Login to trader server {}:{} success. [tradingDay: {}; sessionId: {}]",
        apiInfo->traderServerIP_.c_str(), apiInfo->traderServerPort_,
        tradingDay, sessionId_);

  return 0;
}

std::tuple<int, ExchOrderId> TDGatewayOfXTP::doOrder(
    const OrderInfoSPtr& orderInfo) {
  XTPOrderInsertInfo order;

  memset(&order, 0, sizeof(XTPOrderInsertInfo));
  order.order_client_id = tdSvc_->getClientId2OrderId()->getClientId();
  tdSvc_->getClientId2OrderId()->add(order.order_client_id,
                                     orderInfo->orderId_);
  strncpy(order.ticker, orderInfo->exchSymbolCode_, XTP_TICKER_LEN);
  order.market = GetExchMarketCode(orderInfo->marketCode_);
  order.price = orderInfo->orderPrice_;
  order.quantity = orderInfo->orderSize_;
  order.price_type = XTP_PRICE_LIMIT;
  order.side = GetExchSide(orderInfo->side_);
  order.position_effect = XTP_POSITION_EFFECT_INIT;
  order.business_type = XTP_BUSINESS_TYPE_CASH;

  const auto exchOrderId = api_->InsertOrder(&order, sessionId_);
  if (exchOrderId == 0) {
    const auto apiInfo = Config::get_const_instance().getApiInfo();
    const auto errorInfo = api_->GetApiLastError();
    const auto externalStatusCode = fmt::format("{}", errorInfo->error_id);
    const auto externalStatusMsg = errorInfo->error_msg;
    const auto statusCode =
        tdSvc_->getExternalStatusCodeCache()->getAndSetStatusCodeIfNotExists(
            apiInfo->apiName_, 0, externalStatusCode, externalStatusMsg, -1);
    tdSvc_->getClientId2OrderId()->remove(order.order_client_id);
    LOG_W("Insert order {} failed. {} - {}", orderInfo->orderId_,
          externalStatusCode, externalStatusMsg);
    return {statusCode, exchOrderId};
  }

  LOG_I("Send req of insert order {} of clientId {} success. ",
        orderInfo->orderId_, order.order_client_id);
  return {0, exchOrderId};
}

int TDGatewayOfXTP::doCancelOrder(const OrderInfoSPtr& orderInfo) {
  const auto exchCancelOrderId =
      api_->CancelOrder(orderInfo->exchOrderId_, sessionId_);
  if (exchCancelOrderId == 0) {
    const auto apiInfo = Config::get_const_instance().getApiInfo();
    const auto errorInfo = api_->GetApiLastError();
    const auto externalStatusCode = fmt::format("{}", errorInfo->error_id);
    const auto externalStatusMsg = errorInfo->error_msg;
    orderInfo->statusCode_ =
        tdSvc_->getExternalStatusCodeCache()->getAndSetStatusCodeIfNotExists(
            apiInfo->apiName_, 0, externalStatusCode, externalStatusMsg, -1);
    LOG_W("Cancel order {} failed. {} - {}", orderInfo->orderId_,
          externalStatusCode, externalStatusMsg);
    return orderInfo->statusCode_;
  }

  LOG_I("Send req of cancel order {} success. ", orderInfo->orderId_);
  return 0;
}

void TDGatewayOfXTP::doSyncUnclosedOrderInfo(SHMIPCAsyncTaskSPtr& asyncTask) {
  const auto orderInfoInOrdMgr = std::any_cast<OrderInfoSPtr>(asyncTask->arg_);
  if (orderInfoInOrdMgr->exchOrderId_ == 0) {
    orderInfoInOrdMgr->orderStatus_ = OrderStatus::Failed;
    orderInfoInOrdMgr->statusCode_ = SCODE_TD_SVC_ORDER_NOT_SENT_TO_RMT_SRV;

    const auto [isTheOrderInfoUpdated, orderInfoUpdated] =
        tdSvc_->getOrdMgr()->updateByOrderInfoFromExch(
            orderInfoInOrdMgr, tdSvc_->getNextNoUsedToCalcPos(),
            DeepClone::True, LockFunc::True, tdSvc_->getFeeInfoCache());
    if (isTheOrderInfoUpdated == IsSomeFieldOfOrderUpdated::False) {
      return;
    }

    tdSvc_->getSHMCliOfTDSrv()->asyncSendMsgWithZeroCopy(
        [&](void* shmBuf) {
          InitMsgBody(shmBuf, *orderInfoUpdated);
          LOG_I("Send order ret. {}",
                static_cast<OrderInfo*>(shmBuf)->toShortStr());
        },
        MSG_ID_ON_ORDER_RET, sizeof(OrderInfo));

    tdSvc_->cacheSyncTaskGroup(MSG_ID_ON_ORDER_RET, orderInfoUpdated,
                               SyncToRiskMgr::True, SyncToDB::True);
  }

  const auto ret = api_->QueryOrderByXTPIDEx(orderInfoInOrdMgr->exchOrderId_,
                                             sessionId_, ++requestId_);
  if (ret != 0) {
    const auto errorInfo = api_->GetApiLastError();
    const auto externalStatusCode = errorInfo->error_id;
    const auto externalStatusMsg = errorInfo->error_msg;
    LOG_W("Sync unclosed order info failed. {} - {} {}", externalStatusCode,
          externalStatusMsg, orderInfoInOrdMgr->toShortStr());
    return;
  }
  LOG_I("Send req of query unclosed order info success. {} ",
        orderInfoInOrdMgr->toShortStr());
  return;
}

void TDGatewayOfXTP::doSyncAssetsSnapshot() {
  if (const auto ret = api_->QueryPosition(nullptr, sessionId_, ++requestId_);
      ret != 0) {
    const auto errorInfo = api_->GetApiLastError();
    const auto externalStatusCode = errorInfo->error_id;
    const auto externalStatusMsg = errorInfo->error_msg;
    LOG_W("Query position info failed. {} - {}", externalStatusCode,
          externalStatusMsg);
    return;
  }
  LOG_I("Send req of query position info success. ");

  if (const auto ret = api_->QueryAsset(sessionId_, ++requestId_); ret != 0) {
    const auto errorInfo = api_->GetApiLastError();
    const auto externalStatusCode = errorInfo->error_id;
    const auto externalStatusMsg = errorInfo->error_msg;
    LOG_W("Query position info failed. {} - {}", externalStatusCode,
          externalStatusMsg);
    return;
  }
  LOG_I("Send req of query assets info success. ");

  return;
}

void TDGatewayOfXTP::doTestOrder() {}

void TDGatewayOfXTP::doTestCancelOrder() {}

}  // namespace bq::td::svc::xtp
