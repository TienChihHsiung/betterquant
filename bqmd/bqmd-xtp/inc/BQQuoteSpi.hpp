/*!
 * \file BQQuoteSpi.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */
#pragma once

#include "util/Pch.hpp"
#include "xtp_quote_api.h"

using namespace XTP::API;

namespace bq::md::svc {
class MDSvcOfCN;
}

namespace bq::md::svc::xtp {

class BQQuoteSpi : public QuoteSpi {
 public:
  explicit BQQuoteSpi(MDSvcOfCN *mdSvc, QuoteApi *api);
  virtual ~BQQuoteSpi();

  void OnDisconnected(int reason) final;

  void OnQueryAllTickersFullInfo(XTPQFI *ticker_info, XTPRI *error_info,
                                 bool is_last) final;

  void OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last);
  void OnSubscribeAllMarketData(XTP_EXCHANGE_TYPE exchange_id,
                                XTPRI *error_info) final;

  void OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last) final;
  void OnUnSubscribeAllMarketData(XTP_EXCHANGE_TYPE exchange_id,
                                  XTPRI *error_info) final;

  void OnSubOrderBook(XTPST *ticker, XTPRI *error_info, bool is_last) final;
  void OnSubscribeAllOrderBook(XTP_EXCHANGE_TYPE exchange_id,
                               XTPRI *error_info) final;

  void OnUnSubOrderBook(XTPST *ticker, XTPRI *error_info, bool is_last) final;
  void OnUnSubscribeAllOrderBook(XTP_EXCHANGE_TYPE exchange_id,
                                 XTPRI *error_info) final;

  void OnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last) final;
  void OnSubscribeAllTickByTick(XTP_EXCHANGE_TYPE exchange_id,
                                XTPRI *error_info) final;

  void OnUnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last) final;
  void OnUnSubscribeAllTickByTick(XTP_EXCHANGE_TYPE exchange_id,
                                  XTPRI *error_info) final;

  void OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[],
                         int32_t bid1_count, int32_t max_bid1_count,
                         int64_t ask1_qty[], int32_t ask1_count,
                         int32_t max_ask1_count);

  void OnOrderBook(XTPOB *order_book) final;

  void OnTickByTick(XTPTBT *tbt_data) final;

 private:
  void subscribeMarketData();

 private:
  MDSvcOfCN *mdSvc_{nullptr};
  QuoteApi *api_{nullptr};
};

}  // namespace bq::md::svc::xtp
