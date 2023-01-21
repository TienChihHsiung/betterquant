/*!
 * \file MDSvcOfXTPUtil.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "MDSvcOfXTPUtil.hpp"

namespace bq::md::svc::xtp {

std::tuple<int, XTP_PROTOCOL_TYPE> GetXTPProtocolType(
    std::string protocolType) {
  boost::to_lower(protocolType);
  if (protocolType == "tcp") {
    return {0, XTP_PROTOCOL_TCP};
  } else if (protocolType == "udp") {
    return {0, XTP_PROTOCOL_UDP};
  } else {
    return {-1, XTP_PROTOCOL_TCP};
  }
}

MarketCode GetMarketCode(XTP_EXCHANGE_TYPE exchangeType) {
  switch (exchangeType) {
    case XTP_EXCHANGE_SH:
      return MarketCode::SSE;
    case XTP_EXCHANGE_SZ:
      return MarketCode::SZSE;
    default:
      return MarketCode::Others;
  }
}

XTP_EXCHANGE_TYPE GetExchMarketCode(MarketCode marketCode) {
  switch (marketCode) {
    case MarketCode::SSE:
      return XTP_EXCHANGE_SH;
    case MarketCode::SZSE:
      return XTP_EXCHANGE_SZ;
    default:
      return XTP_EXCHANGE_UNKNOWN;
  }
}

SymbolType GetSymbolType(XTP_SECURITY_TYPE exchSymbolType) {
  switch (exchSymbolType) {
    case XTP_SECURITY_MAIN_BOARD:
      return SymbolType::CN_MainBoard;

    case XTP_SECURITY_SECOND_BOARD:
      return SymbolType::CN_SecondBoard;

    case XTP_SECURITY_STARTUP_BOARD:
      return SymbolType::CN_StartupBoard;

    case XTP_SECURITY_INDEX:
      return SymbolType::CN_Index;

    case XTP_SECURITY_TECH_BOARD:
      return SymbolType::CN_TechBoard;

    case XTP_SECURITY_STATE_BOND:
      return SymbolType::CN_StateBond;

    case XTP_SECURITY_ENTERPRICE_BOND:
      return SymbolType::CN_EnterpriseBond;

    case XTP_SECURITY_COMPANEY_BOND:
      return SymbolType::CN_CompanyBond;

    case XTP_SECURITY_CONVERTABLE_BOND:
      return SymbolType::CN_ConvertableBond;

    case XTP_SECURITY_NATIONAL_BOND_REVERSE_REPO:
      return SymbolType::CN_NationalBondReverseRepo;

    case XTP_SECURITY_ETF_SINGLE_MARKET_STOCK:
      return SymbolType::CN_ETF_SingleMarketBond;

    case XTP_SECURITY_ETF_INTER_MARKET_STOCK:
      return SymbolType::CN_ETF_InterMarketStock;

    case XTP_SECURITY_ETF_CROSS_BORDER_STOCK:
      // 跨境股票 ETF
      return SymbolType::CN_ETF_CrossBorderStock;

    case XTP_SECURITY_ETF_SINGLE_MARKET_BOND:
      return SymbolType::CN_ETF_SingleMarketBond;

    case XTP_SECURITY_ETF_GOLD:
      return SymbolType::CN_ETF_Gold;

    case XTP_SECURITY_STRUCTURED_FUND_CHILD:
      return SymbolType::CN_StructuredFundChild;

    case XTP_SECURITY_SZSE_RECREATION_FUND:
      return SymbolType::CN_SZSE_RecreationFund;

    case XTP_SECURITY_STOCK_OPTION:
      return SymbolType::CN_StockOption;

    case XTP_SECURITY_ETF_OPTION:
      return SymbolType::CN_ETF_Option;

    case XTP_SECURITY_ALLOTMENT:
      return SymbolType::CN_Allotment;

    case XTP_SECURITY_MONETARY_FUND_SHCR:
      return SymbolType::CN_MonetaryFundSHTR;

    case XTP_SECURITY_MONETARY_FUND_SHTR:
      return SymbolType::CN_MonetaryFundSHTR;

    case XTP_SECURITY_MONETARY_FUND_SZ:
      return SymbolType::CN_MonetaryFundSZ;

    default:
      return SymbolType::Others;
  }
}

SymbolState GetSymbolState(XTP_SECURITY_STATUS exchSymbolState) {
  switch (exchSymbolState) {
    case XTP_SECURITY_STATUS_ST:
      return CN_StatusST;

    case XTP_SECURITY_STATUS_N_IPO:
      return CN_StatusNIPO;

    case XTP_SECURITY_STATUS_COMMON:
      return CN_StatusCommon;

    case XTP_SECURITY_STATUS_RESUME:
      return CN_StatusResume;

    case XTP_SECURITY_STATUS_DELISTING:
      return CN_StatusDelisting;

    default:
      return SymbolState::Others;
  }
}

Side GetSideFromTrades(MarketCode marketCode, char trade_flag) {
  if (marketCode == MarketCode::SSE) {
    if (trade_flag == 'B') {
      return Side::Bid;
    } else if (trade_flag == 'S') {
      return Side::Ask;
    } else {
      return Side::Others;
    }
  } else if (marketCode == MarketCode::SZSE) {
    return Side::Others;
  } else {
    return Side::Others;
  }
}

Side GetSideFromOrders(MarketCode marketCode, char side) {
  if (marketCode == MarketCode::SSE) {
    if (side == 'B') {
      return Side::Bid;
    } else if (side == 'S') {
      return Side::Ask;
    } else {
      return Side::Others;
    }
  } else if (marketCode == MarketCode::SZSE) {
    if (side == '1') {
      return Side::Bid;
    } else if (side == '2') {
      return Side::Ask;
    } else {
      return Side::Others;
    }
  } else {
    return Side::Others;
  }
}

}  // namespace bq::md::svc::xtp
