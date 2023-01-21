/*!
 * \file FlowCtrlConst.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#pragma once

#include "util/Pch.hpp"

namespace bq {

enum class FlowCtrlStep : std::uint8_t {
  BeforeTDSrv = 1,
  InTDSrv,
  Others = UINT8_MAX - 1
};

enum class FlowCtrlTarget : std::uint8_t {
  OrderSizeEachTime,
  OrderSizeTotal,

  OrderAmtEachTime,
  OrderAmtTotal,

  OrderTimesTotal,
  OrderTimesWithinTime,

  CancelOrderTimesTotal,
  CancelOrderTimesWithinTime,

  RejectOrderTimesTotal,
  RejectOrderTimesWithinTime,

  DealSizeTotal,

  Others = UINT8_MAX - 1
};

enum class FlowCtrlAction : std::uint8_t {
  RejectOrder = 1,
  PubTopic,

  Others = UINT8_MAX - 1
};

enum class FlowCtrlLimitType : std::uint8_t {
  NumLimitEachTime = 1,
  NumLimitTotal,
  NumLimitWithinTime,
  Others = UINT8_MAX - 1
};

const static std::string SEP_OF_LIMIT_VALUE = "/";
const static std::uint32_t MAX_TS_QUEUE_SIZE = 1000;

}  // namespace bq
