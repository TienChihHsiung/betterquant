/*!
 * \file BQTraderSpi.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */
#pragma once

#include "API.hpp"
#include "util/Pch.hpp"

using namespace XTP::API;

namespace bq::td::svc {
class TDSvcOfCN;
}

namespace bq::td::svc::xtp {

class BQTraderSpi : public TraderSpi {
 public:
  BQTraderSpi(TDSvcOfCN *tdSvc, TraderApi *api);
  ~BQTraderSpi();

  void OnDisconnected(uint64_t sessionId, int reason) final;

  void OnOrderEvent(XTPOrderInfo *orderInfo, XTPRI *errorInfo,
                    uint64_t sessionId) final;

  void OnTradeEvent(XTPTradeReport *tradeInfo, uint64_t sessionId) final;

  void OnCancelOrderError(XTPOrderCancelInfo *cancelInfo, XTPRI *errorInfo,
                          uint64_t sessionId) final;

  void OnQueryOrderEx(XTPOrderInfoEx *orderInfo, XTPRI *errorInfo,
                      int requestId, bool isLast, uint64_t sessionId) final;

  void OnQueryPosition(XTPQueryStkPositionRsp *pos, XTPRI *errorInfo,
                       int requestId, bool isLast, uint64_t sessionId) final;

  void OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *errorInfo, int requestId,
                    bool isLast, uint64_t sessionId) final;

 private:
  TDSvcOfCN *tdSvc_{nullptr};
  TraderApi *api_{nullptr};
};

}  // namespace bq::td::svc::xtp
