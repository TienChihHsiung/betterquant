/*!
 * \file FlowCtrlUtil.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/11
 *
 * \brief
 */

#include "def/ConditionUtil.hpp"

#include "def/BQConst.hpp"
#include "def/DataStruOfTD.hpp"
#include "def/Def.hpp"
#include "def/Field.hpp"

namespace bq {

std::tuple<int, std::string, ConditionFieldGroup> MakeConditionFieldGroup(
    const std::string& conditionInStrFmt) {
  ConditionFieldGroup conditionFieldGroup;

  std::vector<std::string> recGroup;
  boost::split(recGroup, conditionInStrFmt, boost::is_any_of(SEP_OF_COND_AND));

  for (const auto& rec : recGroup) {
    std::vector<std::string> fieldGroup;
    boost::split(fieldGroup, rec, boost::is_any_of(SEP_OF_COND_FIELD));
    if (fieldGroup.size() != 1 && fieldGroup.size() != 2) {
      const auto statusMsg = fmt::format(
          "Make condition field group failed "
          "because of invalid field num {} in condition {}.",
          rec, conditionInStrFmt);
      return {-1, statusMsg, conditionFieldGroup};
    }
    conditionFieldGroup.emplace_back(fieldGroup[0]);
  }

  return {0, "", conditionFieldGroup};
}

std::tuple<int, std::string, ConditionTemplate> MakeConditionTemplate(
    const std::string& conditionInStrFmt) {
  ConditionTemplate conditionTemplate;

  std::vector<std::string> recGroup;
  boost::split(recGroup, conditionInStrFmt, boost::is_any_of(SEP_OF_COND_AND));

  for (const auto& rec : recGroup) {
    std::vector<std::string> fieldGroup;
    boost::split(fieldGroup, rec, boost::is_any_of(SEP_OF_COND_FIELD));
    if (fieldGroup.size() != 2) {
      const auto statusMsg = fmt::format(
          "Make condition field group failed "
          "because of invalid field num {} in condition {}.",
          rec, conditionInStrFmt);
      return {-1, statusMsg, conditionTemplate};
    }
    conditionTemplate.emplace(std::make_pair(fieldGroup[0], fieldGroup[1]));
  }

  return {0, "", conditionTemplate};
}

std::tuple<int, std::string, std::string, ConditionValue> MakeConditioValue(
    const OrderInfoSPtr& orderInfo,
    const ConditionFieldGroup& conditionFieldGroup) {
  std::string conditionValueInStrFmt;
  ConditionValue conditionValue;

  for (const auto& fieldName : conditionFieldGroup) {
    if (fieldName == FIELD_ACCT_ID) {
      const auto acctId = fmt::format("{}", orderInfo->acctId_);
      conditionValueInStrFmt.append(FIELD_ACCT_ID)
          .append(SEP_OF_COND_FIELD)
          .append(acctId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_ACCT_ID] = acctId;

    } else if (fieldName == FIELD_MARKET_CODE) {
      const auto marketCode = GetMarketName(orderInfo->marketCode_);
      conditionValueInStrFmt.append(FIELD_MARKET_CODE)
          .append(SEP_OF_COND_FIELD)
          .append(marketCode)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_MARKET_CODE] = marketCode;

    } else if (fieldName == FIELD_SYMBOL_CODE) {
      conditionValueInStrFmt.append(FIELD_SYMBOL_CODE)
          .append(SEP_OF_COND_FIELD)
          .append(orderInfo->symbolCode_)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_SYMBOL_CODE] = orderInfo->symbolCode_;

    } else if (fieldName == FIELD_PRODUCT_ID) {
      const auto productId = fmt::format("{}", orderInfo->productId_);
      conditionValueInStrFmt.append(FIELD_PRODUCT_ID)
          .append(SEP_OF_COND_FIELD)
          .append(productId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_PRODUCT_ID] = productId;

    } else if (fieldName == FIELD_USER_ID) {
      const auto userId = fmt::format("{}", orderInfo->userId_);
      conditionValueInStrFmt.append(FIELD_USER_ID)
          .append(SEP_OF_COND_FIELD)
          .append(userId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_USER_ID] = userId;

    } else if (fieldName == FIELD_STG_ID) {
      const auto stgId = fmt::format("{}", orderInfo->stgId_);
      conditionValueInStrFmt.append(FIELD_STG_ID)
          .append(SEP_OF_COND_FIELD)
          .append(stgId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_STG_ID] = stgId;

    } else if (fieldName == FIELD_STG_INST_ID) {
      const auto stgInstId = fmt::format("{}", orderInfo->stgInstId_);
      conditionValueInStrFmt.append(FIELD_STG_INST_ID)
          .append(SEP_OF_COND_FIELD)
          .append(stgInstId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_STG_INST_ID] = stgInstId;

    } else if (fieldName == FIELD_ALGO_ID) {
      const auto algoId = fmt::format("{}", orderInfo->algoId_);
      conditionValueInStrFmt.append(FIELD_ALGO_ID)
          .append(SEP_OF_COND_FIELD)
          .append(algoId)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_ALGO_ID] = algoId;

    } else if (fieldName == FIELD_SYMBOL_TYPE) {
      const auto symbolType =
          fmt::format("{}", magic_enum::enum_name(orderInfo->symbolType_));
      conditionValueInStrFmt.append(FIELD_SYMBOL_TYPE)
          .append(SEP_OF_COND_FIELD)
          .append(symbolType)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_SYMBOL_TYPE] = symbolType;

    } else if (fieldName == FIELD_SIDE) {
      const auto side =
          fmt::format("{}", magic_enum::enum_name(orderInfo->side_));
      conditionValueInStrFmt.append(FIELD_SIDE)
          .append(SEP_OF_COND_FIELD)
          .append(side)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_SIDE] = side;

    } else if (fieldName == FIELD_POS_SIDE) {
      const auto posSide =
          fmt::format("{}", magic_enum::enum_name(orderInfo->posSide_));
      conditionValueInStrFmt.append(FIELD_POS_SIDE)
          .append(SEP_OF_COND_FIELD)
          .append(posSide)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_POS_SIDE] = posSide;

    } else if (fieldName == FIELD_PAR_VALUE) {
      const auto parValue = fmt::format("{}", orderInfo->parValue_);
      conditionValueInStrFmt.append(FIELD_PAR_VALUE)
          .append(SEP_OF_COND_FIELD)
          .append(parValue)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_PAR_VALUE] = parValue;

    } else if (fieldName == FIELD_ORDER_TYPE) {
      const auto orderType =
          fmt::format("{}", magic_enum::enum_name(orderInfo->orderType_));
      conditionValueInStrFmt.append(FIELD_ORDER_TYPE)
          .append(SEP_OF_COND_FIELD)
          .append(orderType)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_ORDER_TYPE] = orderType;

    } else if (fieldName == FIELD_ORDER_TYPE_EXTRA) {
      const auto orderTypeExtra =
          fmt::format("{}", magic_enum::enum_name(orderInfo->orderTypeExtra_));
      conditionValueInStrFmt.append(FIELD_ORDER_TYPE_EXTRA)
          .append(SEP_OF_COND_FIELD)
          .append(orderTypeExtra)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_ORDER_TYPE_EXTRA] = orderTypeExtra;

    } else if (fieldName == FIELD_FEE_CURRENCY) {
      conditionValueInStrFmt.append(FIELD_FEE_CURRENCY)
          .append(SEP_OF_COND_FIELD)
          .append(orderInfo->feeCurrency_)
          .append(SEP_OF_COND_AND);
      conditionValue[FIELD_FEE_CURRENCY] = orderInfo->feeCurrency_;

    } else {
      const auto statusMsg = fmt::format(
          "Make condition value failed "
          "because of invalid field name {} in condition field Group.",
          fieldName);
      return {-1, statusMsg, conditionValueInStrFmt, conditionValue};
    }
  }
  if (!conditionValueInStrFmt.empty()) conditionValueInStrFmt.pop_back();

  return {0, "", std::move(conditionValueInStrFmt), std::move(conditionValue)};
}

std::string MakeConditioFieldInfoInStrFmt(
    const OrderInfo* orderInfo,
    const ConditionFieldGroup& conditionFieldGroup) {
  std::string conditionValueInStrFmt;

  for (const auto& fieldName : conditionFieldGroup) {
    if (fieldName == FIELD_ACCT_ID) {
      const auto acctId = fmt::format("{}", orderInfo->acctId_);
      conditionValueInStrFmt.append(FIELD_ACCT_ID)
          .append(SEP_OF_COND_FIELD)
          .append(acctId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_MARKET_CODE) {
      const auto marketCode = GetMarketName(orderInfo->marketCode_);
      conditionValueInStrFmt.append(FIELD_MARKET_CODE)
          .append(SEP_OF_COND_FIELD)
          .append(marketCode)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_SYMBOL_CODE) {
      conditionValueInStrFmt.append(FIELD_SYMBOL_CODE)
          .append(SEP_OF_COND_FIELD)
          .append(orderInfo->symbolCode_)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_PRODUCT_ID) {
      const auto productId = fmt::format("{}", orderInfo->productId_);
      conditionValueInStrFmt.append(FIELD_PRODUCT_ID)
          .append(SEP_OF_COND_FIELD)
          .append(productId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_USER_ID) {
      const auto userId = fmt::format("{}", orderInfo->userId_);
      conditionValueInStrFmt.append(FIELD_USER_ID)
          .append(SEP_OF_COND_FIELD)
          .append(userId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_STG_ID) {
      const auto stgId = fmt::format("{}", orderInfo->stgId_);
      conditionValueInStrFmt.append(FIELD_STG_ID)
          .append(SEP_OF_COND_FIELD)
          .append(stgId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_STG_INST_ID) {
      const auto stgInstId = fmt::format("{}", orderInfo->stgInstId_);
      conditionValueInStrFmt.append(FIELD_STG_INST_ID)
          .append(SEP_OF_COND_FIELD)
          .append(stgInstId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_ALGO_ID) {
      const auto algoId = fmt::format("{}", orderInfo->algoId_);
      conditionValueInStrFmt.append(FIELD_ALGO_ID)
          .append(SEP_OF_COND_FIELD)
          .append(algoId)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_SYMBOL_TYPE) {
      const auto symbolType =
          fmt::format("{}", magic_enum::enum_name(orderInfo->symbolType_));
      conditionValueInStrFmt.append(FIELD_SYMBOL_TYPE)
          .append(SEP_OF_COND_FIELD)
          .append(symbolType)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_SIDE) {
      const auto side =
          fmt::format("{}", magic_enum::enum_name(orderInfo->side_));
      conditionValueInStrFmt.append(FIELD_SIDE)
          .append(SEP_OF_COND_FIELD)
          .append(side)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_POS_SIDE) {
      const auto posSide =
          fmt::format("{}", magic_enum::enum_name(orderInfo->posSide_));
      conditionValueInStrFmt.append(FIELD_POS_SIDE)
          .append(SEP_OF_COND_FIELD)
          .append(posSide)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_PAR_VALUE) {
      const auto parValue = fmt::format("{}", orderInfo->parValue_);
      conditionValueInStrFmt.append(FIELD_PAR_VALUE)
          .append(SEP_OF_COND_FIELD)
          .append(parValue)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_ORDER_TYPE) {
      const auto orderType =
          fmt::format("{}", magic_enum::enum_name(orderInfo->orderType_));
      conditionValueInStrFmt.append(FIELD_ORDER_TYPE)
          .append(SEP_OF_COND_FIELD)
          .append(orderType)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_ORDER_TYPE_EXTRA) {
      const auto orderTypeExtra =
          fmt::format("{}", magic_enum::enum_name(orderInfo->orderTypeExtra_));
      conditionValueInStrFmt.append(FIELD_ORDER_TYPE_EXTRA)
          .append(SEP_OF_COND_FIELD)
          .append(orderTypeExtra)
          .append(SEP_OF_COND_AND);

    } else if (fieldName == FIELD_FEE_CURRENCY) {
      conditionValueInStrFmt.append(FIELD_FEE_CURRENCY)
          .append(SEP_OF_COND_FIELD)
          .append(orderInfo->feeCurrency_)
          .append(SEP_OF_COND_AND);

    } else {
      const auto statusMsg = fmt::format(
          "Make condition value failed "
          "because of invalid field name {} in condition field Group.",
          fieldName);
      return "";
    }
  }
  if (!conditionValueInStrFmt.empty()) conditionValueInStrFmt.pop_back();

  return conditionValueInStrFmt;
}

std::tuple<int, std::string, bool> MatchConditionTemplate(
    const ConditionValue& conditionValue,
    const ConditionTemplate& conditionTemplate) {
  if (conditionValue.size() != conditionTemplate.size()) {
    const auto statusMsg = fmt::format(
        "Make condition template failed because of "
        "condition value size {} not equal to condition template size {}.",
        conditionValue.size(), conditionTemplate.size());
    return {-1, statusMsg, false};
  }

  auto iterCV = std::begin(conditionValue);
  auto iterCT = std::begin(conditionTemplate);

  for (; iterCV != std::end(conditionValue); ++iterCV, ++iterCT) {
    const auto& cvKey = iterCV->first;
    const auto& ctKey = iterCT->first;

    if (cvKey != ctKey) {
      const auto statusMsg = fmt::format(
          "Make condition template failed because of "
          "condition value key {} not equal to condition template key {}.",
          cvKey, ctKey);
      return {-1, statusMsg, false};
    }

    const auto& cvValue = iterCV->second;
    const auto& ctValue = iterCT->second;

    if (ctValue.empty() || ctValue == "*") {
      continue;
    }

    if (cvValue != ctValue) {
      return {0, "", false};
    }
  }

  return {0, "", true};
}

std::string ExtractFieldName(const std::string& cond) {
  std::vector<std::string> fieldName2ValueGroup;
  boost::split(fieldName2ValueGroup, cond, boost::is_any_of(SEP_OF_COND_AND));
  if (fieldName2ValueGroup.empty()) {
    return "";
  }

  std::string ret;
  for (const auto& rec : fieldName2ValueGroup) {
    std::vector<std::string> fieldName2Value;
    boost::split(fieldName2Value, rec, boost::is_any_of("="));
    if (fieldName2Value.size() != 2) {
      return "";
    }
    ret = ret + fieldName2Value[0] + SEP_OF_COND_AND;
  }
  if (!ret.empty()) ret.pop_back();

  return ret;
}

std::string AddRequiredFields(const std::string& targetStr,
                              const std::string& requiredFields) {
  std::string ret = targetStr;
  const auto [statusCode, statusMsg, conditionFieldGroup] =
      MakeConditionFieldGroup(requiredFields);
  for (const auto& field : conditionFieldGroup) {
    if (targetStr.find(field) == std::string::npos) {
      ret.append(SEP_OF_COND_AND).append(field).append("=");
    }
  }
  if (boost::starts_with(ret, SEP_OF_COND_AND)) ret.erase(0, 1);
  return ret;
}

}  // namespace bq
