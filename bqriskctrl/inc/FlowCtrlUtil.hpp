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

#include "FlowCtrlConst.hpp"
#include "FlowCtrlDef.hpp"
#include "util/Pch.hpp"

namespace bq {

std::tuple<int, std::string, FlowCtrlLimitType> GetLimitType(
    FlowCtrlTarget target);

std::tuple<int, std::string, LimitValue> MakeLimitValue(
    FlowCtrlLimitType limitType, const std::string& LimitValueInStrFmt);

}  // namespace bq
