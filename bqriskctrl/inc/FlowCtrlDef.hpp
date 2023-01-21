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

#include "FlowCtrlConst.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/ConditionDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq::db::flowCtrlRule {
struct FieldGroupOfAll;
using Record = FieldGroupOfAll;
using RecordSPtr = std::shared_ptr<Record>;
}  // namespace bq::db::flowCtrlRule

namespace bq::db {
using RecFlowCtrlRuleGroup =
    std::map<std::string, bq::db::flowCtrlRule::RecordSPtr>;
}

namespace bq {

template <typename T>
using FixedSizeQueue = std::queue<T, boost::circular_buffer<T>>;

struct LimitValue {
 public:
  LimitValue() = default;
  LimitValue(Decimal value) : value_(value) {}

  union {
    Decimal value_{0};
    std::uint32_t msInterval_;
  };
  FixedSizeQueue<std::uint64_t> tsQue_;

  bool mustUpdateToDB_{false};

 public:
  void reset(std::uint32_t queueSize) {
    for (std::uint32_t i = 0; i < queueSize; ++i) {
      tsQue_.push(UNDEFINED_FIELD_MIN_TS / 1000);
    }
  }

  std::string toStr() const {
    std::string ret;
    if (tsQue_.empty()) {
      ret = fmt::format("{}{}", value_, ";");
    } else {
      ret = fmt::format("{}{}", msInterval_, ";");
      auto tmpQue = tsQue_;
      while (!tmpQue.empty()) {
        ret.append(std::to_string(tmpQue.front())).append(",");
        tmpQue.pop();
      }
      if (boost::ends_with(ret, ",") || boost::ends_with(ret, ";")) {
        ret.pop_back();
      }
    }
    return ret;
  }

  std::tuple<int, std::string> fromStr(const std::string& str) {
    std::vector<std::string> recSet;
    boost::split(recSet, str, boost::is_any_of(";"));
    if (recSet.size() != 2) {
      const auto statusMsg =
          fmt::format("Invalid str fmt of limit value. {}", str);
      return {-1, statusMsg};
    }

    const auto valueInStrFmt = recSet[0];
    const auto tsQueInStrFmt = recSet[1];
    if (tsQueInStrFmt.empty()) {
      value_ = CONV(Decimal, recSet[0]);
    } else {
      msInterval_ = CONV(std::uint32_t, valueInStrFmt);
      std::vector<std::string> fieldGroup;
      boost::split(fieldGroup, tsQueInStrFmt, boost::is_any_of(","));
      tsQue_ = FixedSizeQueue<std::uint64_t>(
          boost::circular_buffer<std::uint64_t>(fieldGroup.size()));
      for (const auto& field : fieldGroup) {
        tsQue_.push(CONV(std::uint64_t, field));
      }
    }

    return {0, ""};
  }
};

using ConditionValue2LimitValueGroup = std::map<std::string, LimitValue>;

struct FlowCtrlRule {
  std::uint32_t no_;
  std::string name_;

  FlowCtrlStep step_;
  FlowCtrlTarget target_;

  std::string condition_;
  ConditionFieldGroup conditionFieldGroup_;
  ConditionTemplate conditionTemplate_;
  ConditionValue2LimitValueGroup conditionValue2LimitValueGroup_;

  FlowCtrlLimitType limitType_;
  LimitValue limitValue_;

  std::string action_;
  std::vector<FlowCtrlAction> actionGroup_;

  bool contains(FlowCtrlAction action) {
    const auto iter =
        std::find(std::begin(actionGroup_), std::end(actionGroup_), action);
    if (iter != std::end(actionGroup_)) return true;
    return false;
  }

  std::string toStr() {
    if (!str_.empty()) return str_;
    const auto name = name_.empty() ? R"("")" : name_;
    str_ = fmt::format("no: {}; name: {}; step: {}; target: {}; condition: {}",
                       no_, name, magic_enum::enum_name(step_),
                       magic_enum::enum_name(target_), condition_);
    return str_;
  }

  // clang-format off
  std::string getSqlOfInsert(const std::string& conditionValue, const std::string& details) const {
    const auto sql = fmt::format(
        "INSERT INTO `BetterQuant`.`flowCtrlRuleTriggerInfo` ("
        "`no`,"
        "`name`,"
        "`step`,"
        "`target`,"
        "`condition`,"
        "`conditionValue`,"
        "`limitType`,"
        "`action`,"
        "`details`"
        ")"
        "VALUES"
        "("
        " {} ,"  // no
        "'{}',"  // name
        "'{}',"  // step
        "'{}',"  // target
        "'{}',"  // condition
        "'{}',"  // conditioValue 
        "'{}',"  // limitType
        "'{}',"  // action 
        "'{}'"  // details
        "); ",
      no_,
      name_,
      magic_enum::enum_name(step_),
      magic_enum::enum_name(target_),
      condition_,
      conditionValue,
      magic_enum::enum_name(limitType_),
      action_,
      details
    );

    return sql;
  }
  // clang-format on

  std::string toJson();

 private:
  std::string str_;
  std::string jsonStr_;
};
using FLowCtrlRuleSPtr = std::shared_ptr<FlowCtrlRule>;
using Target2FlowCtrlRuleGroup =
    std::multimap<FlowCtrlTarget, FLowCtrlRuleSPtr>;
using Target2FlowCtrlRuleGroupSPtr = std::shared_ptr<Target2FlowCtrlRuleGroup>;

using No2FlowCtrlRuleGroup = std::map<std::uint32_t, FLowCtrlRuleSPtr>;
using No2FlowCtrlRuleGroupSPtr = std::shared_ptr<No2FlowCtrlRuleGroup>;

using RecFlowCtrlRuleGroup =
    std::map<std::string, bq::db::flowCtrlRule::RecordSPtr>;

struct FlowCtrlTargetState {
  bool orderSizeEachTime_{false};
  bool orderSizeTotal_{false};

  bool orderAmtEachTime_{false};
  bool orderAmtTotal_{false};

  bool orderTimesTotal_{false};
  bool orderTimesWithinTime_{false};

  bool cancelOrderTimesTotal_{false};
  bool cancelOrderTimesWithinTime_{false};

  bool rejectOrderTimesTotal_{false};
  bool rejectOrderTimesWithinTime_{false};

  bool dealSizeTotal_{false};
};
using FlowCtrlTargetStateSPtr = std::shared_ptr<FlowCtrlTargetState>;

struct OrderInfo;
using OrderInfoSPtr = std::shared_ptr<OrderInfo>;

}  // namespace bq
