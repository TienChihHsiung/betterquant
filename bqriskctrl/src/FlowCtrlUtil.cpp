/*!
 * \file FlowCtrlUtil.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/11
 *
 * \brief
 */

#include "FlowCtrlUtil.hpp"

#include "def/BQConst.hpp"
#include "def/ConditionConst.hpp"
#include "def/ConditionUtil.hpp"
#include "def/DataStruOfTD.hpp"
#include "def/Def.hpp"
#include "def/Field.hpp"

namespace bq {

std::tuple<int, std::string, FlowCtrlLimitType> GetLimitType(
    FlowCtrlTarget target) {
  switch (target) {
    case FlowCtrlTarget::OrderSizeEachTime:
      return {0, "", FlowCtrlLimitType::NumLimitEachTime};
    case FlowCtrlTarget::OrderSizeTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};

    case FlowCtrlTarget::OrderAmtEachTime:
      return {0, "", FlowCtrlLimitType::NumLimitEachTime};
    case FlowCtrlTarget::OrderAmtTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};

    case FlowCtrlTarget::OrderTimesTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};
    case FlowCtrlTarget::OrderTimesWithinTime:
      return {0, "", FlowCtrlLimitType::NumLimitWithinTime};

    case FlowCtrlTarget::CancelOrderTimesTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};
    case FlowCtrlTarget::CancelOrderTimesWithinTime:
      return {0, "", FlowCtrlLimitType::NumLimitWithinTime};

    case FlowCtrlTarget::RejectOrderTimesTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};
    case FlowCtrlTarget::RejectOrderTimesWithinTime:
      return {0, "", FlowCtrlLimitType::NumLimitWithinTime};

    case FlowCtrlTarget::DealSizeTotal:
      return {0, "", FlowCtrlLimitType::NumLimitTotal};

    default: {
      const auto statusMsg =
          fmt::format("Get limit type failed because of invalid target {}.",
                      magic_enum::enum_name(target));
      return {-1, statusMsg, FlowCtrlLimitType::Others};
    }
  }
}

std::tuple<int, std::string, LimitValue> MakeLimitValue(
    FlowCtrlLimitType limitType, const std::string& limitValueInStrFmt) {
  LimitValue limitValue;

  switch (limitType) {
    case FlowCtrlLimitType::NumLimitEachTime:
    case FlowCtrlLimitType::NumLimitTotal: {
      const auto v = CONV_OPT(Decimal, limitValueInStrFmt);
      if (v == boost::none) {
        const auto statusMsg = fmt::format(
            "Make risk ctrl value of limit failed "
            "because of invalid limit value {}.",
            limitValueInStrFmt);
        return {-1, statusMsg, limitValue};
      }
      limitValue.value_ = v.value();
    } break;

    case FlowCtrlLimitType::NumLimitWithinTime: {
      std::vector<std::string> fieldGroup;
      boost::split(fieldGroup, limitValueInStrFmt,
                   boost::is_any_of(SEP_OF_LIMIT_VALUE));

      if (fieldGroup.size() != 2 || !boost::iends_with(fieldGroup[1], "ms")) {
        const auto statusMsg = fmt::format(
            "Make risk ctrl value of limit failed "
            "because of invalid limit value {}.",
            limitValueInStrFmt);
        return {-1, statusMsg, limitValue};
      }

      const auto tmp = boost::erase_tail_copy(fieldGroup[1], 2);
      auto v = CONV_OPT(std::uint32_t, tmp);
      if (v == boost::none) {
        const auto statusMsg = fmt::format(
            "Make risk ctrl value of limit failed "
            "because of invalid limit value {}.",
            limitValueInStrFmt);
        return {-1, statusMsg, limitValue};
      }
      limitValue.msInterval_ = v.value();

      v = CONV_OPT(std::uint32_t, fieldGroup[0]);
      if (v == boost::none) {
        const auto statusMsg = fmt::format(
            "Make risk ctrl value of limit failed "
            "because of invalid limit value {}.",
            limitValueInStrFmt);
        return {-1, statusMsg, limitValue};
      }
      const auto queueSize = v.value();
      if (queueSize > MAX_TS_QUEUE_SIZE) {
        const auto statusMsg = fmt::format(
            "Make risk ctrl value of limit failed "
            "because of size of tsQue {} greater than {}.",
            queueSize, MAX_TS_QUEUE_SIZE);
        return {-1, statusMsg, limitValue};
      }

      limitValue.tsQue_ = FixedSizeQueue<std::uint64_t>(
          boost::circular_buffer<std::uint64_t>(queueSize));
      limitValue.reset(queueSize);

    } break;

    default:
      const auto statusMsg = fmt::format(
          "Make risk ctrl value of limit failed "
          "because of invalid limit type {}.",
          magic_enum::enum_name(limitType));
      return {-1, statusMsg, limitValue};
  }

  return {0, "", limitValue};
}

}  // namespace bq
