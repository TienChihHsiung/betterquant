/*!
 * \file FlowCtrlUtil.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#pragma once

#include "def/ConditionConst.hpp"
#include "def/ConditionDef.hpp"
#include "util/Pch.hpp"

namespace bq {

struct OrderInfo;
using OrderInfoSPtr = std::shared_ptr<OrderInfo>;

std::tuple<int, std::string, ConditionFieldGroup> MakeConditionFieldGroup(
    const std::string& conditionInStrFmt);

std::tuple<int, std::string, ConditionTemplate> MakeConditionTemplate(
    const std::string& conditionInStrFmt);

std::tuple<int, std::string, std::string, ConditionValue> MakeConditioValue(
    const OrderInfoSPtr& orderInfo,
    const ConditionFieldGroup& conditionFieldGroup);

std::string MakeConditioFieldInfoInStrFmt(
    const OrderInfo* orderInfo, const ConditionFieldGroup& conditionFieldGroup);

std::tuple<int, std::string, bool> MatchConditionTemplate(
    const ConditionValue& conditionValue,
    const ConditionTemplate& conditionTemplate);

std::string ExtractFieldName(const std::string& cond);

std::string AddRequiredFields(const std::string& targetStr,
                              const std::string& requiredFields);

}  // namespace bq
