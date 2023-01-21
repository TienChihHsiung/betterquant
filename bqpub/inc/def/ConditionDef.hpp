/*!
 * \file FlowCtrlDef.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#pragma once

#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/ConditionConst.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq {

using ConditionFieldGroup = std::vector<std::string>;

using ConditionTemplate = std::map<std::string, std::string>;

using ConditionValue = std::map<std::string, std::string>;

}  // namespace bq
