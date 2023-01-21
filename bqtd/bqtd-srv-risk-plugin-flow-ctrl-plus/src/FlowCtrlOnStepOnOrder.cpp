/*!
 * \file FlowCtrlOnStepOnOrder.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#include "FlowCtrlOnStepOnOrder.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlUtil.hpp"
#include "TDSrvRiskPluginFlowCtrlPlus.hpp"
#include "def/DataStruOfTD.hpp"

namespace bq::td::srv {

int FlowCtrlOnStepOnOrder::doHandleFlowCtrl(const OrderInfoSPtr& orderInfo) {
  int statusCode = 0;

  const auto orderAmt = orderInfo->orderSize_ * orderInfo->orderPrice_;

  if (plugin_->getFlowCtrlTargetState()->orderSizeEachTime_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::OrderSizeEachTime,
        UpdateStgOfLimitValue::CompareAndUpdate, orderInfo->orderSize_);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->orderSizeTotal_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::OrderSizeTotal,
        UpdateStgOfLimitValue::CompareAndUpdate, orderInfo->orderSize_);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->orderAmtEachTime_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::OrderAmtEachTime,
        UpdateStgOfLimitValue::CompareAndUpdate, orderAmt);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->orderAmtTotal_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::OrderAmtTotal,
        UpdateStgOfLimitValue::CompareAndUpdate, orderAmt);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->orderTimesTotal_) {
    statusCode =
        handleFlowCtrlForTarget(orderInfo, FlowCtrlTarget::OrderTimesTotal,
                                UpdateStgOfLimitValue::CompareAndUpdate, 1);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->orderTimesWithinTime_) {
    statusCode =
        handleFlowCtrlForTarget(orderInfo, FlowCtrlTarget::OrderTimesWithinTime,
                                UpdateStgOfLimitValue::CompareAndUpdate, 1);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->rejectOrderTimesTotal_) {
    statusCode = handleFlowCtrlForTarget(orderInfo,
                                         FlowCtrlTarget::RejectOrderTimesTotal,
                                         UpdateStgOfLimitValue::Compare);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->rejectOrderTimesWithinTime_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::RejectOrderTimesWithinTime,
        UpdateStgOfLimitValue::Compare);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->dealSizeTotal_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::DealSizeTotal,
        UpdateStgOfLimitValue::CompareAndUpdate, orderInfo->orderSize_);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  return 0;
}

}  // namespace bq::td::srv
