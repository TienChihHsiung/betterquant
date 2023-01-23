/*!
 * \file BQQuoteSpi.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "BQQuoteSpi.hpp"

#include "Config.hpp"
#include "MDSvcOfXTP.hpp"
#include "MDSvcOfXTPUtil.hpp"
#include "RawMDHandler.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "def/RawMD.hpp"
#include "util/Logger.hpp"

namespace bq::md::svc::xtp {

BQQuoteSpi::BQQuoteSpi(MDSvcOfCN *mdSvc, QuoteApi *api)
    : mdSvc_(mdSvc), api_(api) {}

BQQuoteSpi::~BQQuoteSpi() {}

void BQQuoteSpi::OnDisconnected(int reason) {
  LOG_W("Disconnect from quote server. [reason: {}]", reason);

  const auto apiInfo = Config::get_const_instance().getApiInfo();

  int ret = -1;
  while (0 != ret) {
    const auto [statusCode, protocolType] =
        GetXTPProtocolType(apiInfo->protocolType_);
    ret = api_->Login(
        apiInfo->quoteServerIP_.c_str(), apiInfo->quoteServerPort_,
        apiInfo->quoteUsername_.c_str(), apiInfo->quotePassword_.c_str(),
        protocolType,
        apiInfo->localIP_.empty() ? apiInfo->localIP_.c_str() : nullptr);
    if (0 != ret) {
      XTPRI *error_info = api_->GetApiLastError();
      LOG_W("Login to server error. [{} - {}]", error_info->error_id,
            error_info->error_msg);

      std::this_thread::sleep_for(std::chrono::seconds(
          std::chrono::seconds(apiInfo->secIntervalOfReconnect_)));
    }
  }

  mdSvc_->setTradingDay(api_->GetTradingDay());
  LOG_I("Login to server {}:{} success. [tradingDay: {}]",
        apiInfo->quoteServerIP_, apiInfo->quoteServerPort_,
        mdSvc_->getTradingDay());

  subscribeMarketData();
}

void BQQuoteSpi::OnQueryAllTickersFullInfo(XTPQFI *ticker_info,
                                           XTPRI *error_info, bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Query all tickers full info failed. [{} - {}]", error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_D("Query tickers full info success. [{} - {}]", ticker_info->ticker,
        ticker_info->ticker_name);

  if (CONFIG["enableSymbolTableMaint"].as<bool>(false)) {
    auto rawMD = mdSvc_->getRawMDHandler()->makeRawMD(
        MsgType::NewSymbol, ticker_info, sizeof(XTPQFI));
    mdSvc_->getRawMDHandler()->dispatch(rawMD);
  }

  if (is_last == true) {
    if (ticker_info->exchange_id == XTP_EXCHANGE_SH) {
      api_->QueryAllTickersFullInfo(XTP_EXCHANGE_SZ);
    } else {
      subscribeMarketData();
    }
  }
}

void BQQuoteSpi::subscribeMarketData() {
  if (CONFIG["subAllMarketData"].as<bool>(false) == true) {
    api_->SubscribeAllOrderBook(XTP_EXCHANGE_SH);
    api_->SubscribeAllOrderBook(XTP_EXCHANGE_SZ);
    api_->SubscribeAllTickByTick(XTP_EXCHANGE_SH);
    api_->SubscribeAllTickByTick(XTP_EXCHANGE_SZ);
    api_->SubscribeAllMarketData(XTP_EXCHANGE_SH);
    api_->SubscribeAllMarketData(XTP_EXCHANGE_SZ);
  }
  return;
}

void BQQuoteSpi::OnSubMarketData(XTPST *ticker, XTPRI *error_info,
                                 bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub ticker of {} failed. [{} - {}]", ticker->ticker,
          error_info->error_id, error_info->error_msg);
    return;
  }
  LOG_I("Sub ticker of {} success.", ticker->ticker);
}

void BQQuoteSpi::OnSubscribeAllMarketData(XTP_EXCHANGE_TYPE exchange_id,
                                          XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub all tickers of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Sub all tickers of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnUnSubMarketData(XTPST *ticker, XTPRI *error_info,
                                   bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub ticker failed. [{} - {}]", error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub ticker of {} success. ", ticker->ticker);
}

void BQQuoteSpi::OnUnSubscribeAllMarketData(XTP_EXCHANGE_TYPE exchange_id,
                                            XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub all tickers of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub all tickers of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnSubTickByTick(XTPST *ticker, XTPRI *error_info,
                                 bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub tbt of {} failed. [{} - {}]", ticker->ticker,
          error_info->error_id, error_info->error_msg);
    return;
  }
  LOG_I("Sub tbt of {} success.", ticker->ticker);
}

void BQQuoteSpi::OnSubscribeAllTickByTick(XTP_EXCHANGE_TYPE exchange_id,
                                          XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub all tbts of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Sub all tbts of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnUnSubTickByTick(XTPST *ticker, XTPRI *error_info,
                                   bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub tbt failed. [{} - {}]", error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub tbt of {} success. ", ticker->ticker);
}

void BQQuoteSpi::OnUnSubscribeAllTickByTick(XTP_EXCHANGE_TYPE exchange_id,
                                            XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub all tbts of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub all tbts of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnSubOrderBook(XTPST *ticker, XTPRI *error_info,
                                bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub order book of {} failed. [{} - {}]", ticker->ticker,
          error_info->error_id, error_info->error_msg);
    return;
  }
  LOG_I("Sub order book of {} success.", ticker->ticker);
}

void BQQuoteSpi::OnSubscribeAllOrderBook(XTP_EXCHANGE_TYPE exchange_id,
                                         XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Sub all order books of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Sub all order books of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnUnSubOrderBook(XTPST *ticker, XTPRI *error_info,
                                  bool is_last) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub order book failed. [{} - {}]", error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub order book of {} success. ", ticker->ticker);
};

void BQQuoteSpi::OnUnSubscribeAllOrderBook(XTP_EXCHANGE_TYPE exchange_id,
                                           XTPRI *error_info) {
  if (error_info && error_info->error_id != 0) {
    LOG_W("Unsub all order books of exchange {} failed. [{} - {}]",
          GetMarketName(GetMarketCode(exchange_id)), error_info->error_id,
          error_info->error_msg);
    return;
  }
  LOG_I("Unsub all order books of exchange {} success.",
        GetMarketName(GetMarketCode(exchange_id)));
}

void BQQuoteSpi::OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[],
                                   int32_t bid1_count, int32_t max_bid1_count,
                                   int64_t ask1_qty[], int32_t ask1_count,
                                   int32_t max_ask1_count) {
  auto rawMD = mdSvc_->getRawMDHandler()->makeRawMD(MsgType::Tickers,
                                                    market_data, sizeof(XTPMD));
  mdSvc_->getRawMDHandler()->dispatch(rawMD);
}

void BQQuoteSpi::OnTickByTick(XTPTBT *tbt_data) {
  switch (tbt_data->type) {
    case XTP_TBT_TRADE: {
      auto rawMD = mdSvc_->getRawMDHandler()->makeRawMD(
          MsgType::Trades, tbt_data, sizeof(XTPTBT));
      mdSvc_->getRawMDHandler()->dispatch(rawMD);
    } break;
    case XTP_TBT_ENTRUST: {
      auto rawMD = mdSvc_->getRawMDHandler()->makeRawMD(
          MsgType::Orders, tbt_data, sizeof(XTPTBT));
      mdSvc_->getRawMDHandler()->dispatch(rawMD);
    } break;
    default:
      break;
  }
}

void BQQuoteSpi::OnOrderBook(XTPOB *order_book) {
  LOG_I("Recv order book of {}.", order_book->ticker);
  auto rawMD = mdSvc_->getRawMDHandler()->makeRawMD(MsgType::Books, order_book,
                                                    sizeof(XTPOB));
  mdSvc_->getRawMDHandler()->dispatch(rawMD);
}

}  // namespace bq::md::svc::xtp
