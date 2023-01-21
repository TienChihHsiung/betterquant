/*!
 * \file BQConst.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "def/BQConst.hpp"

namespace bq {

std::string GetMarketName(MarketCode marketCode) {
  switch (marketCode) {
    case MarketCode::Okex:
      return "Okex";
    case MarketCode::Binance:
      return "Binance";
    case MarketCode::Coinbase:
      return "Coinbase";
    case MarketCode::Kraken:
      return "Kraken";

    case MarketCode::SSE:
      return "SSE";
    case MarketCode::SZSE:
      return "SZSE";
    case MarketCode::SHFE:
      return "SHFE";
    case MarketCode::ZCE:
      return "ZCE";
    case MarketCode::DCE:
      return "DCE";
    case MarketCode::CFFEX:
      return "CFFEX";

    default:
      return "Others";
      break;
  }
  return "Others";
}

MarketCode GetMarketCode(const std::string& marketCodeName) {
  if (marketCodeName == "Okex") {
    return MarketCode::Okex;
  } else if (marketCodeName == "Binance") {
    return MarketCode::Binance;
  } else if (marketCodeName == "Coinbase") {
    return MarketCode::Coinbase;
  } else if (marketCodeName == "Kraken") {
    return MarketCode::Kraken;

  } else if (marketCodeName == "SSE") {
    return MarketCode::SSE;
  } else if (marketCodeName == "SZSE") {
    return MarketCode::SZSE;
  } else if (marketCodeName == "SHFE") {
    return MarketCode::SHFE;
  } else if (marketCodeName == "ZCE") {
    return MarketCode::ZCE;
  } else if (marketCodeName == "DCE") {
    return MarketCode::DCE;
  } else if (marketCodeName == "CFFEX") {
    return MarketCode::CFFEX;

  } else {
    return MarketCode::Others;
  }

  return MarketCode::Others;
}

}  // namespace bq
