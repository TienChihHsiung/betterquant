/*!
 * \file MDSvcOfXTP.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "MDSvcOfXTP.hpp"

#include "API.hpp"
#include "BQQuoteSpi.hpp"
#include "Config.hpp"
#include "MDStorageSvc.hpp"
#include "MDSvcOfXTPUtil.hpp"
#include "RawMDHandlerOfXTP.hpp"
#include "SHMIPCConst.hpp"
#include "SHMSrv.hpp"
#include "SHMSrvMsgHandler.hpp"
#include "db/DBE.hpp"
#include "db/DBEng.hpp"
#include "db/DBEngConst.hpp"
#include "tdeng/TDEngConnpool.hpp"
#include "tdeng/TDEngConst.hpp"
#include "tdeng/TDEngParam.hpp"
#include "util/Logger.hpp"
#include "util/MarketDataCond.hpp"
#include "util/String.hpp"

using namespace std;

namespace bq::md::svc::xtp {

MDSvcOfXTP::~MDSvcOfXTP() {
  if (api_) {
    api_->Release();
    api_ = nullptr;
  }
}

int MDSvcOfXTP::beforeInit() {
  const auto raMDHandler = std::make_shared<RawMDHandlerOfXTP>(this);
  setRawMDHandler(raMDHandler);

  return 0;
}

int MDSvcOfXTP::doRun() { return 0; }

int MDSvcOfXTP::startGateway() {
  if (api_) {
    api_->Release();
    api_ = nullptr;
  }

#ifdef _NDEBUG
  XTP_LOG_LEVEL logLevel = XTP_LOG_LEVEL_INFO;
#else
  XTP_LOG_LEVEL logLevel = XTP_LOG_LEVEL_DEBUG;
#endif

  const auto apiInfo = Config::get_const_instance().getApiInfo();
  api_ = XTP::API::QuoteApi::CreateQuoteApi(
      apiInfo->clientId_, apiInfo->loggerPath_.c_str(), logLevel);
  api_->SetHeartBeatInterval(apiInfo->secIntervalOfHeartBeat_);

  const auto spi = new BQQuoteSpi(this, api_);
  api_->RegisterSpi(spi);

  const auto [statusCode, protocolType] =
      GetXTPProtocolType(apiInfo->protocolType_);
  if (statusCode != 0) {
    LOG_E(
        "Start gateway of {} failed "
        "because of invalid protocol type {} in config.",
        apiInfo->apiName_, apiInfo->protocolType_);
    return statusCode;
  }

  const auto ret = api_->Login(
      apiInfo->quoteServerIP_.c_str(), apiInfo->quoteServerPort_,
      apiInfo->quoteUsername_.c_str(), apiInfo->quotePassword_.c_str(),
      protocolType,
      apiInfo->localIP_.empty() ? apiInfo->localIP_.c_str() : nullptr);
  if (0 != ret) {
    XTPRI* error_info = api_->GetApiLastError();
    LOG_W("Login to server error. [{} - {}]", error_info->error_id,
          error_info->error_msg);
    return ret;
  }
  setTradingDay(api_->GetTradingDay());
  LOG_I("Login to server {}:{} success. [tradingDay: {}]",
        apiInfo->quoteServerIP_, apiInfo->quoteServerPort_, getTradingDay());

  api_->QueryAllTickersFullInfo(XTP_EXCHANGE_SH);
  return 0;
}

void MDSvcOfXTP::stopGateway() {
  if (api_) {
    api_->Release();
    api_ = nullptr;
  }
}

void MDSvcOfXTP::doSub(const MarketDataCondGroup& marketDataCondGroup) {
  using KeyType = std::tuple<XTP_EXCHANGE_TYPE, MDType>;
  using ValType = std::vector<MarketDataCondSPtr>;

  std::map<KeyType, ValType> m;

  for (const auto& marketDataCond : marketDataCondGroup) {
    const auto exchMarketCode = GetExchMarketCode(marketDataCond->marketCode_);
    const auto key = std::make_tuple(exchMarketCode, marketDataCond->mdType_);
    m[key].emplace_back(marketDataCond);
  }

  for (const auto& rec : m) {
    const auto [exchMarketCode, mdType] = rec.first;
    const auto& mdCondGroup = rec.second;

    const auto tickerCount = mdCondGroup.size();

    char** ppInstrumentID = new char*[tickerCount];
    for (std::size_t i = 0; i < tickerCount; i++) {
      ppInstrumentID[i] = new char[XTP_TICKER_LEN];
    }

    for (std::size_t i = 0; i < mdCondGroup.size(); ++i) {
      strncpy(ppInstrumentID[i], mdCondGroup[i]->symbolCode_.c_str(),
              sizeof(ppInstrumentID[i]));
    }

    switch (mdType) {
      case MDType::Tickers:
        api_->SubscribeMarketData(ppInstrumentID, tickerCount, exchMarketCode);
        break;
      case MDType::Trades:
      case MDType::Orders:
        api_->SubscribeTickByTick(ppInstrumentID, tickerCount, exchMarketCode);
        break;
      case MDType::Books:
        api_->SubscribeOrderBook(ppInstrumentID, tickerCount, exchMarketCode);
        break;
      default:
        break;
    }

    for (std::size_t i = 0; i < tickerCount; i++) {
      delete[] ppInstrumentID[i];
      ppInstrumentID[i] = NULL;
    }
    delete[] ppInstrumentID;
    ppInstrumentID = NULL;
  }
}

void MDSvcOfXTP::doUnSub(const MarketDataCondGroup& marketDataCondGroup) {
  using KeyType = std::tuple<XTP_EXCHANGE_TYPE, MDType>;
  using ValType = std::vector<MarketDataCondSPtr>;

  std::map<KeyType, ValType> m;

  for (const auto& marketDataCond : marketDataCondGroup) {
    const auto exchMarketCode = GetExchMarketCode(marketDataCond->marketCode_);
    const auto key = std::make_tuple(exchMarketCode, marketDataCond->mdType_);
    m[key].emplace_back(marketDataCond);
  }

  for (const auto& rec : m) {
    const auto [exchMarketCode, mdType] = rec.first;
    const auto& mdCondGroup = rec.second;

    const auto tickerCount = mdCondGroup.size();

    char** ppInstrumentID = new char*[tickerCount];
    for (std::size_t i = 0; i < tickerCount; i++) {
      ppInstrumentID[i] = new char[XTP_TICKER_LEN];
    }

    for (std::size_t i = 0; i < mdCondGroup.size(); ++i) {
      strncpy(ppInstrumentID[i], mdCondGroup[i]->symbolCode_.c_str(),
              sizeof(ppInstrumentID[i]));
    }

    switch (mdType) {
      case MDType::Tickers:
        api_->UnSubscribeMarketData(ppInstrumentID, tickerCount,
                                    exchMarketCode);
        break;
      case MDType::Trades:
      case MDType::Orders:
        api_->UnSubscribeTickByTick(ppInstrumentID, tickerCount,
                                    exchMarketCode);
        break;
      case MDType::Books:
        api_->UnSubscribeOrderBook(ppInstrumentID, tickerCount, exchMarketCode);
        break;
      default:
        break;
    }

    for (std::size_t i = 0; i < tickerCount; i++) {
      delete[] ppInstrumentID[i];
      ppInstrumentID[i] = NULL;
    }
    delete[] ppInstrumentID;
    ppInstrumentID = NULL;
  }
}

}  // namespace bq::md::svc::xtp
