/*!
 * \file FlowCtrlOnStepBase.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#pragma once

#include "def/BQDef.hpp"
#include "util/Pch.hpp"

namespace bq {
struct OrderInfo;
using OrderInfoSPtr = std::shared_ptr<OrderInfo>;
enum class FlowCtrlTarget : uint8_t;
struct FlowCtrlRule;
using FlowCtrlRuleSPtr = std::shared_ptr<FlowCtrlRule>;
}  // namespace bq

namespace bq::td::srv {

class TDSrvRiskPluginFlowCtrlPlus;

enum class UpdateStgOfLimitValue { CompareAndUpdate = 1, Compare, Update };

class FlowCtrlOnStepBase {
 public:
  FlowCtrlOnStepBase(const FlowCtrlOnStepBase&) = delete;
  FlowCtrlOnStepBase& operator=(const FlowCtrlOnStepBase&) = delete;
  FlowCtrlOnStepBase(const FlowCtrlOnStepBase&&) = delete;
  FlowCtrlOnStepBase& operator=(const FlowCtrlOnStepBase&&) = delete;

  FlowCtrlOnStepBase(TDSrvRiskPluginFlowCtrlPlus* plugin) : plugin_(plugin) {}

  int handleFlowCtrl(const OrderInfoSPtr& orderInfo) {
    return doHandleFlowCtrl(orderInfo);
  }

 private:
  virtual int doHandleFlowCtrl(const OrderInfoSPtr& orderInfo) = 0;

 protected:
  int handleFlowCtrlForTarget(const OrderInfoSPtr& orderInfo,
                              FlowCtrlTarget target,
                              UpdateStgOfLimitValue updateStgOfLimitValue,
                              Decimal value = 0);

 private:
  std::tuple<bool, std::string> checkIfTriggerFlowCtrl(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt,
      UpdateStgOfLimitValue updateStgOfLimitValue, Decimal value);

 private:
  std::tuple<bool, std::string> checkIfTriggerFlowCtrlForNumLimitEachTime(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt, Decimal value);

 private:
  std::tuple<bool, std::string> checkIfTriggerFlowCtrlForNumLimitTotal(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt,
      UpdateStgOfLimitValue updateStgOfLimitValue, Decimal value);

  std::tuple<bool, std::string> numLimitTotalCompareAndUpdate(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt, Decimal value);

  std::tuple<bool, std::string> numLimitTotalCompare(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt);

  void numLimitTotalUpdate(const OrderInfoSPtr& orderInfo,
                           const FlowCtrlRuleSPtr& rule,
                           const std::string& conditionValueInStrFmt,
                           Decimal value);

 private:
  std::tuple<bool, std::string> checkIfTriggerFlowCtrlForNumLimitWithInTime(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt,
      UpdateStgOfLimitValue updateStgOfLimitValue);

  std::tuple<bool, std::string> numLimitWithInTimeCompareAndUpdate(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt);

  std::tuple<bool, std::string> numLimitWithInTimeCompare(
      const OrderInfoSPtr& orderInfo, const FlowCtrlRuleSPtr& rule,
      const std::string& conditionValueInStrFmt);

  void numLimitWithInTimeUpdate(const OrderInfoSPtr& orderInfo,
                                const FlowCtrlRuleSPtr& rule,
                                const std::string& conditionValueInStrFmt);

  void saveFlowCtrlRuleTriggerInfoToDB(const FlowCtrlRuleSPtr& rule,
                                       const std::string& conditionValue,
                                       const std::string& details);

 protected:
  TDSrvRiskPluginFlowCtrlPlus* plugin_{nullptr};
};

using FlowCtrlOnStepBaseSPtr = std::shared_ptr<FlowCtrlOnStepBase>;

}  // namespace bq::td::srv
