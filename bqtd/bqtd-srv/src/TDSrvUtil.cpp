/*!
 * \file TDSrvUtil.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/14
 *
 * \brief
 */

#include "TDSrvUtil.hpp"

#include "FlowCtrlDef.hpp"
#include "def/DataStruOfTD.hpp"
#include "util/Datetime.hpp"

namespace bq {

std::string MakeTopicDataOfTriggerRiskCtrl(const FlowCtrlRuleSPtr& rule,
                                           const OrderInfoSPtr& orderInfo) {
  const auto jsonStrOfRule = rule->toJson();
  const auto jsonStrOfOrder = orderInfo->toJson();
  const auto topicData =
      fmt::format(R"({{"triggerTime":{},"rule":{},"orderInfo":{}}})",
                  GetTotalUSSince1970(), jsonStrOfRule, jsonStrOfOrder);
  return topicData;
}

}  // namespace bq
