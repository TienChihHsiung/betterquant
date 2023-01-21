/*!
 * \file BQConstIF.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "util/PchBase.hpp"

namespace bq {

enum class MarketCode : std::uint16_t {
  Okex = 1,
  Binance = 2,
  Coinbase = 3,
  Kraken = 4,

  SSE = 101,
  SZSE,
  SHFE,
  ZCE,
  DCE,
  CFFEX,

  Others = UINT16_MAX
};
std::string GetMarketName(MarketCode marketCode);
MarketCode GetMarketCode(const std::string& marketCodeName);

enum class SymbolType : std::uint8_t {
  Spot = 1,
  Futures = 2,
  CFutures = 3,
  Perp = 4,
  CPerp = 5,
  Option = 6,
  index = 7,

  CN_MainBoard = 21,
  CN_SecondBoard = 22,
  CN_StartupBoard = 23,
  CN_Index = 24,
  CN_TechBoard = 25,
  CN_StateBond = 26,
  CN_EnterpriseBond = 27,
  CN_CompanyBond = 28,
  CN_ConvertableBond = 29,
  CN_NationalBondReverseRepo = 30,
  CN_ETF_SingleMarketStock = 31,
  CN_ETF_InterMarketStock = 32,
  CN_ETF_CrossBorderStock = 33,
  CN_ETF_SingleMarketBond = 34,
  CN_ETF_Gold = 35,
  CN_StructuredFundChild = 36,
  CN_SZSE_RecreationFund = 37,
  CN_StockOption = 38,
  CN_ETF_Option = 39,

  CN_Allotment = 40,

  CN_MoneyMonetaryFundSHCR = 41,
  CN_MonetaryFundSHTR = 42,
  CN_MonetaryFundSZ = 43,

  Others = UINT8_MAX - 1
};

enum class BusinessType : std::uint8_t { Normal = 1, Others = UINT8_MAX - 1 };

enum class Side : std::uint8_t { Bid = 1, Ask = 2, Others = UINT8_MAX - 1 };

enum class PosSide : std::uint8_t {
  Long = 1,
  Short = 2,
  Both = 3,
  Others = UINT8_MAX - 1
};

enum class LiquidityDirection : std::uint8_t {
  Taker = 1,
  Maker = 2,
  Others = UINT8_MAX - 1
};

enum class OrderType : std::uint8_t { Limit = 1, Others = UINT8_MAX - 1 };

enum class OrderTypeExtra : std::uint8_t {
  Normal = 1,
  MakeOnly = 2,
  Ioc = 3,
  Fok = 4,
  Others = UINT8_MAX - 1
};

enum class MDType : std::uint8_t {
  Trades = 1,
  Orders = 2,
  Books = 3,
  Tickers = 4,
  Candle = 5,

  Others = UINT8_MAX - 1
};

enum class OrderStatus {
  Created = 1,
  ConfirmedInLocal = 3,
  Pending = 5,
  ConfirmedByExch = 10,
  PartialFilled = 20,
  Filled = 100,
  Canceled = 101,
  PartialFilledCanceled = 105,
  Failed = 110,
  Others = 127
};

enum SymbolState : std::uint8_t {
  Preopen = 1,
  Online = 2,
  Suspend = 3,
  Settlement = 4,

  CN_StatusST = 21,
  CN_StatusNIPO = 22,
  CN_StatusCommon = 23,
  CN_StatusResume = 24,
  CN_StatusDelisting = 25,

  Others = UINT8_MAX - 1
};

const static std::string CN_OFFICIAL_CURRENCY = "CNY";

constexpr static std::uint32_t MAX_DEPTH_LEVEL = 400;
constexpr static std::uint32_t MAX_DEPTH_LEVEL_OF_CN = 10;
constexpr static std::uint32_t MAX_DEPTH_LEVEL_IN_TICKER = 10;

const static std::string SEP_OF_DEPTH_FIELDS = ",";
const static std::string SEP_OF_DEPTH_REC = ";";

constexpr static std::uint16_t MAX_SYMBOL_CODE_LEN = 32;
constexpr static std::uint16_t MAX_SIMED_TD_INFO = 1024;
constexpr static std::uint16_t MAX_CURRENCY_LEN = 16;
constexpr static std::uint16_t MAX_TRADE_ID_LEN = 32;

constexpr static std::uint16_t MAX_TRADE_NO_LEN = 32;
constexpr static std::uint16_t MAX_ORDER_NO_LEN = 32;
constexpr static std::uint16_t MAX_ORDER_ID_LEN = 16;
constexpr static std::uint16_t MAX_TRADING_DAY_LEN = 9;
constexpr static std::uint16_t MAX_ASSETS_NAME_LEN = 32;

const static std::string TOPIC_PREFIX_OF_MARKET_DATA = "MD";
const static std::string TOPIC_PREFIX_OF_TRADE_DATA = "TD";
const static std::string SEP_OF_TOPIC = "@";

const static std::string SEP_OF_SYMBOL_SPOT = "-";
const static std::string SEP_OF_SYMBOL_FUTURES = "-";
const static std::string SEP_OF_SYMBOL_PERP = "-";

const static std::string SEP_OF_REC_IDENTITY = "/";
const static std::string SEP_OF_COND_AND = "&";

const static std::uint64_t UNDEFINED_FIELD_MIN_TS = 946684800000000;
const static std::uint64_t UNDEFINED_FIELD_MAX_TS = 1893456000000000;
const static std::string UNDEFINED_FIELD_MIN_DATETIME =
    "2000-01-01 00:00:00.000000";
const static std::string UNDEFINED_FIELD_MAX_DATETIME =
    "2030-01-01 00:00:00.000000";

}  // namespace bq
