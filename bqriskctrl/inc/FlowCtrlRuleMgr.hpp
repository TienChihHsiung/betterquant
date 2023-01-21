/*!
 * \file FLowCtrlRuleTable.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/11
 *
 * \brief
 */

#pragma once

#include "FlowCtrlConst.hpp"
#include "FlowCtrlDef.hpp"
#include "FlowCtrlUtil.hpp"
#include "util/Pch.hpp"

// clang-format off
// clang-format on

namespace bq::db {
class DBEng;
using DBEngSPtr = std::shared_ptr<DBEng>;
}  // namespace bq::db

namespace bq {

class FlowCtrlRuleMgr {
 public:
  FlowCtrlRuleMgr(const FlowCtrlRuleMgr&) = delete;
  FlowCtrlRuleMgr& operator=(const FlowCtrlRuleMgr&) = delete;
  FlowCtrlRuleMgr(const FlowCtrlRuleMgr&&) = delete;
  FlowCtrlRuleMgr& operator=(const FlowCtrlRuleMgr&&) = delete;

  FlowCtrlRuleMgr();

  std::tuple<int, std::string> init(
      const RecFlowCtrlRuleGroup& recFlowCtrlRuleGroup);

  std::tuple<int, std::string> loadFromDB(const db::DBEngSPtr& dbEng);
  std::tuple<int, std::string> saveToDB(const db::DBEngSPtr& dbEng);

  Target2FlowCtrlRuleGroupSPtr getTarget2FlowCtrlRuleGroup() const {
    return target2FlowCtrlRuleGroup_;
  }

  No2FlowCtrlRuleGroupSPtr getNo2FlowCtrlRuleGroup() const {
    return no2FlowCtrlRuleGroup_;
  }

 private:
  std::tuple<int, std::string> initTarget2FlowCtrlRuleGroup(
      const RecFlowCtrlRuleGroup& recFlowCtrlRuleGroup);

 private:
  Target2FlowCtrlRuleGroupSPtr target2FlowCtrlRuleGroup_{nullptr};
  No2FlowCtrlRuleGroupSPtr no2FlowCtrlRuleGroup_{nullptr};
};

using FlowCtrlRuleMgrSPtr = std::shared_ptr<FlowCtrlRuleMgr>;

}  // namespace bq
