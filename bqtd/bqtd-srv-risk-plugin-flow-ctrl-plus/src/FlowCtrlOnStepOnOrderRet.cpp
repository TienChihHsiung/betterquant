/*!
 * \file FlowCtrlOnStepOnOrderRet.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#include "FlowCtrlOnStepOnOrderRet.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlUtil.hpp"
#include "TDSrvRiskPluginFlowCtrlPlus.hpp"
#include "def/DataStruOfTD.hpp"
#include "def/StatusCode.hpp"

namespace bq::td::srv {

int FlowCtrlOnStepOnOrderRet::doHandleFlowCtrl(const OrderInfoSPtr& orderInfo) {
  if (plugin_->getFlowCtrlTargetState()->rejectOrderTimesTotal_) {
    if (orderInfo->statusCode_ == SCODE_EXTERNAL_SYS_ORDER_REJECTED) {
      const auto statusCode = handleFlowCtrlForTarget(
          orderInfo, FlowCtrlTarget::RejectOrderTimesTotal,
          UpdateStgOfLimitValue::Update);
      if (statusCode != 0) {
        return statusCode;
      }
    }
  }

  if (plugin_->getFlowCtrlTargetState()->rejectOrderTimesWithinTime_) {
    if (orderInfo->statusCode_ == SCODE_EXTERNAL_SYS_ORDER_REJECTED) {
      const auto statusCode = handleFlowCtrlForTarget(
          orderInfo, FlowCtrlTarget::RejectOrderTimesWithinTime,
          UpdateStgOfLimitValue::Update);
      if (statusCode != 0) {
        return statusCode;
      }
    }
  }

  if (plugin_->getFlowCtrlTargetState()->dealSizeTotal_) {
    if (orderInfo->orderStatus_ > OrderStatus::Filled) {
      Decimal updateNum = 0;
      if (orderInfo->orderStatus_ != OrderStatus::PartialFilledCanceled) {
        updateNum = -1 * orderInfo->orderSize_;
      } else {
        updateNum =
            -1 * (orderInfo->orderSize_ - std::fabs(orderInfo->dealSize_));
      }
      const auto statusCode =
          handleFlowCtrlForTarget(orderInfo, FlowCtrlTarget::DealSizeTotal,
                                  UpdateStgOfLimitValue::Update, updateNum);
      if (statusCode != 0) {
        return statusCode;
      }
    }
  }

  return 0;
}

}  // namespace bq::td::srv
