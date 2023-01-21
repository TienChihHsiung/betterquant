/*!
 * \file FLowCtrlRuleTable.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/11
 *
 * \brief
 */

#include "FlowCtrlRuleMgr.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlDef.hpp"
#include "db/DBE.hpp"
#include "db/TBLFlowCtrlRule.hpp"
#include "db/TBLFlowCtrlRuleCache.hpp"
#include "db/TBLRecSetMaker.hpp"
#include "def/BQConst.hpp"
#include "def/ConditionConst.hpp"
#include "def/ConditionUtil.hpp"
#include "util/BQUtil.hpp"
#include "util/Random.hpp"

namespace bq {

FlowCtrlRuleMgr::FlowCtrlRuleMgr()
    : target2FlowCtrlRuleGroup_(std::make_shared<Target2FlowCtrlRuleGroup>()),
      no2FlowCtrlRuleGroup_(std::make_shared<No2FlowCtrlRuleGroup>()) {}

std::tuple<int, std::string> FlowCtrlRuleMgr::init(
    const RecFlowCtrlRuleGroup& recFlowCtrlRuleGroup) {
  if (const auto [statusCode, statusMsg] =
          initTarget2FlowCtrlRuleGroup(recFlowCtrlRuleGroup);
      statusCode != 0) {
    return {statusCode, statusMsg};
  }
  return {0, ""};
}

std::tuple<int, std::string> FlowCtrlRuleMgr::initTarget2FlowCtrlRuleGroup(
    const RecFlowCtrlRuleGroup& recFlowCtrlRuleGroup) {
  for (const auto& rec : recFlowCtrlRuleGroup) {
    auto rule = std::make_shared<FlowCtrlRule>();

    rule->no_ = rec.second->no;
    rule->name_ = rec.second->name;

    const auto target =
        magic_enum::enum_cast<FlowCtrlTarget>(rec.second->target);
    if (target.has_value()) {
      rule->target_ = target.value();
    } else {
      const auto statusMsg = fmt::format(
          "Init flow ctrl rule mgr failed "
          "because of invalid target {} in db.",
          rec.second->target);
      return {-1, statusMsg};
    }

    const auto step = magic_enum::enum_cast<FlowCtrlStep>(rec.second->step);
    if (step.has_value()) {
      rule->step_ = step.value();
    } else {
      const auto statusMsg = fmt::format(
          "Init flow ctrl rule mgr failed "
          "because of invalid step {} in db.",
          rec.second->step);
      return {-1, statusMsg};
    }

    std::vector<std::string> actionInStrFmtGroup;
    rule->action_ = rec.second->action;
    boost::split(actionInStrFmtGroup, rule->action_,
                 boost::is_any_of(SEP_OF_COND_AND));
    if (actionInStrFmtGroup.empty()) {
      const auto statusMsg = fmt::format(
          "Init flow ctrl rule mgr failed because of empty action in db.");
      return {-1, statusMsg};
    }
    for (const auto& actionInStrFmt : actionInStrFmtGroup) {
      const auto action = magic_enum::enum_cast<FlowCtrlAction>(actionInStrFmt);
      if (action.has_value()) {
        rule->actionGroup_.emplace_back(action.value());
      } else {
        const auto statusMsg = fmt::format(
            "Init flow ctrl rule mgr failed "
            "because of invalid action {} in db.",
            rule->action_);
        return {-1, statusMsg};
      }
    }

    rule->condition_ = rec.second->condition;
    if (rule->step_ == FlowCtrlStep::InTDSrv) {
      rule->condition_ =
          AddRequiredFields(rule->condition_, ORDER_INFO_OFFLOAD_GRANULARITY);
    }

    int statusCode = 0;
    std::string statusMsg;

    std::tie(statusCode, statusMsg, rule->conditionFieldGroup_) =
        MakeConditionFieldGroup(rule->condition_);
    if (statusCode != 0) {
      return {statusCode, statusMsg};
    }

    std::tie(statusCode, statusMsg, rule->conditionTemplate_) =
        MakeConditionTemplate(rule->condition_);
    if (statusCode != 0) {
      return {statusCode, statusMsg};
    }

    std::tie(statusCode, statusMsg, rule->limitType_) =
        GetLimitType(rule->target_);
    if (statusCode != 0) {
      return {statusCode, statusMsg};
    }

    std::tie(statusCode, statusMsg, rule->limitValue_) =
        MakeLimitValue(rule->limitType_, rec.second->limitValue);
    if (statusCode != 0) {
      return {statusCode, statusMsg};
    }

    target2FlowCtrlRuleGroup_->emplace(rule->target_, rule);
    no2FlowCtrlRuleGroup_->emplace(rule->no_, rule);
  }

  return {0, ""};
}

std::tuple<int, std::string> FlowCtrlRuleMgr::loadFromDB(
    const db::DBEngSPtr& dbEng) {
  const auto sql =
      fmt::format("SELECT * FROM {};", TBLFlowCtrlRuleCache::TableName);
  const auto [statusCode, tblRecSet] =
      db::TBLRecSetMaker<TBLFlowCtrlRuleCache>::ExecSql(dbEng, sql);
  if (statusCode != 0) {
    const auto statusMsg = fmt::format("Query flow ctrl rule from db failed.");
    return {statusCode, statusMsg};
  }

  for (const auto& tblRec : *tblRecSet) {
    const auto rec = tblRec.second->getRecWithAllFields();
    const auto target =
        magic_enum::enum_cast<FlowCtrlTarget>(rec->target).value();
    const auto range = target2FlowCtrlRuleGroup_->equal_range(target);
    for (auto iterRange = range.first; iterRange != range.second; ++iterRange) {
      const auto conditionValueInStrFmt = rec->conditionValue;
      LimitValue limitValue;
      const auto [statusCode, statusMsg] = limitValue.fromStr(rec->data);
      if (statusCode != 0) {
        return {statusCode, statusMsg};
      }
      const auto& rule = iterRange->second;
      rule->conditionValue2LimitValueGroup_[conditionValueInStrFmt] =
          limitValue;
    }
  }

  return {0, ""};
}

std::tuple<int, std::string> FlowCtrlRuleMgr::saveToDB(
    const db::DBEngSPtr& dbEng) {
  std::vector<std::string> sqlGroup;

  for (const auto& target2FlowCtrlRule : *target2FlowCtrlRuleGroup_) {
    const auto target = target2FlowCtrlRule.first;
    const auto& rule = target2FlowCtrlRule.second;

    for (const auto& conditionValue2LimitValue :
         rule->conditionValue2LimitValueGroup_) {
      const auto& limitValue = conditionValue2LimitValue.second;
      if (limitValue.mustUpdateToDB_ == false) {
        continue;
      }
      const auto& conditionValueInStrFmt = conditionValue2LimitValue.first;

      // clang-format off
      const auto sql = fmt::format(
          "REPLACE INTO `BetterQuant`.`flowCtrlRuleCache` ("
          "`no`,"
          "`name`,"
          "`step`,"
          "`target`,"
          "`condition`,"
          "`conditionValue`,"
          "`data`"
          ")"
          "VALUES"
          "("
          " {} ,"  // no
          "'{}',"  // name
          "'{}',"  // step
          "'{}',"  // target
          "'{}',"  // condition
          "'{}',"  // conditioValue 
          "'{}'"   // data
          "); ",
        rule->no_,
        rule->name_,
        magic_enum::enum_name(rule->step_),
        magic_enum::enum_name(rule->target_),
        rule->condition_,
        conditionValueInStrFmt,
        limitValue.toStr()
      );
      // clang-format on

      sqlGroup.emplace_back(sql);
    }
  }

  for (const auto& sql : sqlGroup) {
    const auto identity = GET_RAND_STR();
    const auto [ret, execRet] = dbEng->asyncExec(identity, sql);
    if (ret != 0) {
      const auto statusMsg = fmt::format(
          "Insert flow ctrl rule trigger info to db failed. [{}]", sql);
      return {-1, statusMsg};
    }
  }

  return {0, ""};
}

}  // namespace bq
