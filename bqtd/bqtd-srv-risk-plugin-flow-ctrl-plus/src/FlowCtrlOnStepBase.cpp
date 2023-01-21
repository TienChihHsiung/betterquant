/*!
 * \file FlowCtrlOnStepBase.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#include "FlowCtrlOnStepBase.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlRuleMgr.hpp"
#include "FlowCtrlUtil.hpp"
#include "SHMIPCUtil.hpp"
#include "TDSrv.hpp"
#include "TDSrvRiskPluginFlowCtrlPlus.hpp"
#include "db/DBE.hpp"
#include "def/ConditionUtil.hpp"
#include "def/DataStruOfTD.hpp"
#include "util/Datetime.hpp"
#include "util/Float.hpp"
#include "util/Logger.hpp"
#include "util/Random.hpp"

namespace bq::td::srv {

int FlowCtrlOnStepBase::handleFlowCtrlForTarget(
    const OrderInfoSPtr& orderInfo, FlowCtrlTarget target,
    UpdateStgOfLimitValue updateStgOfLimitValue, Decimal value) {
  const auto& target2FlowCtrlRuleGroup =
      std::ext::tls_get<FlowCtrlRuleMgr>().getTarget2FlowCtrlRuleGroup();

  const auto range = target2FlowCtrlRuleGroup->equal_range(target);

  for (auto iterRange = range.first; iterRange != range.second; ++iterRange) {
    const auto target = iterRange->first;
    const auto& rule = iterRange->second;

    const auto& conditionFieldGroup = rule->conditionFieldGroup_;
    const auto [sCodeOfMake, sMsgOfMake, conditionValueInStrFmt,
                conditionValue] =
        MakeConditioValue(orderInfo, conditionFieldGroup);
    if (sCodeOfMake != 0) {
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), sMsgOfMake);
      continue;
    }

    const auto [sCodeOfMatch, sMsgOfMatch, matchConditionTemplte] =
        MatchConditionTemplate(conditionValue, rule->conditionTemplate_);
    if (sCodeOfMatch != 0) {
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), sMsgOfMatch);
      continue;
    }

    if (matchConditionTemplte == false) continue;

    const auto [triggerRiskCtrl, riskCtrlMsg] = checkIfTriggerFlowCtrl(
        orderInfo, rule, conditionValueInStrFmt, updateStgOfLimitValue, value);

    if (triggerRiskCtrl == false) continue;

    return rule->no_;
  }

  return 0;
}

std::tuple<bool, std::string> FlowCtrlOnStepBase::checkIfTriggerFlowCtrl(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt,
    UpdateStgOfLimitValue updateStgOfLimitValue, Decimal value) {
  switch (rule->limitType_) {
    case FlowCtrlLimitType::NumLimitEachTime:
      return checkIfTriggerFlowCtrlForNumLimitEachTime(
          orderInfo, rule, conditionValueInStrFmt, value);

    case FlowCtrlLimitType::NumLimitTotal:
      return checkIfTriggerFlowCtrlForNumLimitTotal(
          orderInfo, rule, conditionValueInStrFmt, updateStgOfLimitValue,
          value);

    case FlowCtrlLimitType::NumLimitWithinTime:
      return checkIfTriggerFlowCtrlForNumLimitWithInTime(
          orderInfo, rule, conditionValueInStrFmt, updateStgOfLimitValue);

    default:
      return {false, ""};
  }
}

std::tuple<bool, std::string>
FlowCtrlOnStepBase::checkIfTriggerFlowCtrlForNumLimitEachTime(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt, Decimal value) {
  bool triggerRiskCtrl = false;
  std::string riskCtrlMsg;
  if (isDefinitelyGreaterThan(value, rule->limitValue_.value_)) {
    triggerRiskCtrl = true;
    riskCtrlMsg =
        fmt::format("Trigger risk ctrl [{}]. [{} > {}] {}", rule->toStr(),
                    value, rule->limitValue_.value_, orderInfo->toShortStr());
    saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt, riskCtrlMsg);
    L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
  }
  return {triggerRiskCtrl, riskCtrlMsg};
}

std::tuple<bool, std::string>
FlowCtrlOnStepBase::checkIfTriggerFlowCtrlForNumLimitTotal(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt,
    UpdateStgOfLimitValue updateStgOfLimitValue, Decimal value) {
  switch (updateStgOfLimitValue) {
    case UpdateStgOfLimitValue::CompareAndUpdate:
      return numLimitTotalCompareAndUpdate(orderInfo, rule,
                                           conditionValueInStrFmt, value);

    case UpdateStgOfLimitValue::Compare:
      return numLimitTotalCompare(orderInfo, rule, conditionValueInStrFmt);

    case UpdateStgOfLimitValue::Update:
      numLimitTotalUpdate(orderInfo, rule, conditionValueInStrFmt, value);
  }
  return {false, ""};
}

std::tuple<bool, std::string> FlowCtrlOnStepBase::numLimitTotalCompareAndUpdate(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt, Decimal value) {
  bool triggerRiskCtrl = false;
  std::string riskCtrlMsg;

  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    const auto newValue = limitValue.value_ + value;
    if (!isDefinitelyGreaterThan(newValue, rule->limitValue_.value_)) {
      limitValue.value_ = newValue;
      limitValue.mustUpdateToDB_ = true;
    } else {
      triggerRiskCtrl = true;
      riskCtrlMsg = fmt::format(
          "Trigger risk ctrl [{}]. [{} > {}] {}", rule->toStr(), newValue,
          rule->limitValue_.value_, orderInfo->toShortStr());
      saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt,
                                      riskCtrlMsg);
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
    }
  } else {
    if (!isDefinitelyGreaterThan(value, rule->limitValue_.value_)) {
      c2l[conditionValueInStrFmt] = LimitValue();
      auto& limitValue = c2l[conditionValueInStrFmt];
      limitValue.value_ = value;
      limitValue.mustUpdateToDB_ = true;
      if (c2l.size() % 100 == 0) {
        L_W(plugin_->logger(), "Size of risk ctrl list is {}. {}", c2l.size(),
            rule->toStr());
      }
    } else {
      triggerRiskCtrl = true;
      riskCtrlMsg =
          fmt::format("Trigger risk ctrl [{}]. [{} > {}] {}", rule->toStr(),
                      value, rule->limitValue_.value_, orderInfo->toShortStr());
      saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt,
                                      riskCtrlMsg);
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
    }
  }

  return {triggerRiskCtrl, riskCtrlMsg};
}

std::tuple<bool, std::string> FlowCtrlOnStepBase::numLimitTotalCompare(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt) {
  bool triggerRiskCtrl = false;
  std::string riskCtrlMsg;

  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    if (isDefinitelyGreaterThan(limitValue.value_, rule->limitValue_.value_)) {
      triggerRiskCtrl = true;
      riskCtrlMsg = fmt::format(
          "Trigger risk ctrl [{}]. [{} > {}] {}", rule->toStr(),
          limitValue.value_, rule->limitValue_.value_, orderInfo->toShortStr());
      saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt,
                                      riskCtrlMsg);
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
    }
  } else {
  }

  return {triggerRiskCtrl, riskCtrlMsg};
}

void FlowCtrlOnStepBase::numLimitTotalUpdate(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt, Decimal value) {
  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    limitValue.value_ += value;
    limitValue.mustUpdateToDB_ = true;
  } else {
    c2l[conditionValueInStrFmt] = LimitValue();
    auto& limitValue = c2l[conditionValueInStrFmt];
    limitValue.value_ = value;
    limitValue.mustUpdateToDB_ = true;
    if (c2l.size() % 100 == 0) {
      L_W(plugin_->logger(), "Size of risk ctrl list is {}. {}", c2l.size(),
          rule->toStr());
    }
  }
}

std::tuple<bool, std::string>
FlowCtrlOnStepBase::checkIfTriggerFlowCtrlForNumLimitWithInTime(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt,
    UpdateStgOfLimitValue updateStgOfLimitValue) {
  switch (updateStgOfLimitValue) {
    case UpdateStgOfLimitValue::CompareAndUpdate:
      return numLimitWithInTimeCompareAndUpdate(orderInfo, rule,
                                                conditionValueInStrFmt);

    case UpdateStgOfLimitValue::Compare:
      return numLimitWithInTimeCompare(orderInfo, rule, conditionValueInStrFmt);

    case UpdateStgOfLimitValue::Update:
      numLimitWithInTimeUpdate(orderInfo, rule, conditionValueInStrFmt);
  }
  return {false, ""};
}

std::tuple<bool, std::string>
FlowCtrlOnStepBase::numLimitWithInTimeCompareAndUpdate(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt) {
  bool triggerRiskCtrl = false;
  std::string riskCtrlMsg;

  const auto now = GetTotalMSSince1970();
  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    const auto curMSInterval = now - limitValue.tsQue_.front();
    if (curMSInterval < limitValue.msInterval_) {
      triggerRiskCtrl = true;
      riskCtrlMsg = fmt::format(
          "Trigger risk ctrl [{}]. [{} < {}] {}", rule->toStr(), curMSInterval,
          limitValue.msInterval_, orderInfo->toShortStr());
      saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt,
                                      riskCtrlMsg);
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
    } else {
      limitValue.tsQue_.push(now);
      limitValue.mustUpdateToDB_ = true;
    }
  } else {
    LimitValue limitValue = rule->limitValue_;
    limitValue.tsQue_.push(now);
    limitValue.mustUpdateToDB_ = true;
    c2l[conditionValueInStrFmt] = std::move(limitValue);
    if (c2l.size() % 100 == 0) {
      L_W(plugin_->logger(), "Size of risk ctrl list is {}. {}", c2l.size(),
          rule->toStr());
    }
  }

  return {triggerRiskCtrl, riskCtrlMsg};
}

std::tuple<bool, std::string> FlowCtrlOnStepBase::numLimitWithInTimeCompare(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt) {
  bool triggerRiskCtrl = false;
  std::string riskCtrlMsg;

  const auto now = GetTotalMSSince1970();
  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    const auto curMSInterval = now - limitValue.tsQue_.front();
    if (curMSInterval < limitValue.msInterval_) {
      triggerRiskCtrl = true;
      riskCtrlMsg = fmt::format(
          "Trigger risk ctrl [{}]. [{} < {}] {}", rule->toStr(), curMSInterval,
          limitValue.msInterval_, orderInfo->toShortStr());
      saveFlowCtrlRuleTriggerInfoToDB(rule, conditionValueInStrFmt,
                                      riskCtrlMsg);
      L_W(plugin_->logger(), "[{}] {}", plugin_->name(), riskCtrlMsg);
    }
  } else {
  }

  return {triggerRiskCtrl, riskCtrlMsg};
}

void FlowCtrlOnStepBase::numLimitWithInTimeUpdate(
    const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
    const std::string& conditionValueInStrFmt) {
  const auto now = GetTotalMSSince1970();
  auto& c2l = rule->conditionValue2LimitValueGroup_;
  const auto iter = c2l.find(conditionValueInStrFmt);
  if (iter != std::end(c2l)) {
    auto& limitValue = iter->second;
    limitValue.tsQue_.push(now);
    limitValue.mustUpdateToDB_ = true;
  } else {
    LimitValue limitValue = rule->limitValue_;
    limitValue.tsQue_.push(now);
    limitValue.mustUpdateToDB_ = true;
    c2l[conditionValueInStrFmt] = std::move(limitValue);
    if (c2l.size() % 100 == 0) {
      L_W(plugin_->logger(), "Size of risk ctrl list is {}. {}", c2l.size(),
          rule->toStr());
    }
  }
}

void FlowCtrlOnStepBase::saveFlowCtrlRuleTriggerInfoToDB(
    const FlowCtrlRuleSPtr& rule, const std::string& conditionValue,
    const std::string& details) {
  const auto identity = GET_RAND_STR();
  const auto sql = rule->getSqlOfInsert(conditionValue, details);
  const auto [ret, execRet] =
      plugin_->getTDSrv()->getDBEng()->asyncExec(identity, sql);
  if (ret != 0) {
    LOG_W("Insert flow ctrl rule trigger info to db failed. [{}]", sql);
  }
}

}  // namespace bq::td::srv
