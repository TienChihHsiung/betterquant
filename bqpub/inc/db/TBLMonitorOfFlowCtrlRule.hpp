/*!
 * \file TBLMonitorOfFlowCtrlRule.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#pragma once

#include "db/TBLFlowCtrlRule.hpp"
#include "db/TBLMonitor.hpp"
#include "def/BQDef.hpp"
#include "def/StatusCode.hpp"
#include "util/Pch.hpp"

namespace bq::db {

using RecFlowCtrlRuleGroup =
    std::map<std::string, bq::db::flowCtrlRule::RecordSPtr>;

class TBLMonitorOfFlowCtrlRule : public TBLMonitor<TBLFlowCtrlRule> {
 public:
  using TBLMonitor<TBLFlowCtrlRule>::TBLMonitor;

 public:
  const RecFlowCtrlRuleGroup& getFlowCtrlRuleGroup() const {
    return flowCtrlRuleGroup_;
  }

 private:
  void initNecessaryDataStructures(
      const TBLRecSetSPtr<TBLFlowCtrlRule>& tblRecOfAll) final {
    {
      for (const auto& tblRec : *tblRecOfAll) {
        const auto rec = tblRec.second->getRecWithAllFields();
        flowCtrlRuleGroup_.emplace(rec->getKey(), rec);
      }
    }
  }

 private:
  RecFlowCtrlRuleGroup flowCtrlRuleGroup_;
};

}  // namespace bq::db
