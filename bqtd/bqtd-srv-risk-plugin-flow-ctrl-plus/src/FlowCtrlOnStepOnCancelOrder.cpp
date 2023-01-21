/*!
 * \file FlowCtrlOnStepOnCancelOrder.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#include "FlowCtrlOnStepOnCancelOrder.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlUtil.hpp"
#include "TDSrvRiskPluginFlowCtrlPlus.hpp"
#include "def/DataStruOfTD.hpp"

namespace bq::td::srv {

int FlowCtrlOnStepOnCancelOrder::doHandleFlowCtrl(
    const OrderInfoSPtr& orderInfo) {
  int statusCode = 0;

  if (plugin_->getFlowCtrlTargetState()->cancelOrderTimesTotal_) {
    statusCode = handleFlowCtrlForTarget(orderInfo,
                                         FlowCtrlTarget::CancelOrderTimesTotal,
                                         UpdateStgOfLimitValue::Update, 1);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  if (plugin_->getFlowCtrlTargetState()->cancelOrderTimesWithinTime_) {
    statusCode = handleFlowCtrlForTarget(
        orderInfo, FlowCtrlTarget::CancelOrderTimesWithinTime,
        UpdateStgOfLimitValue::Update, 1);
    if (statusCode != 0) {
      return statusCode;
    }
  }

  return 0;
}

}  // namespace bq::td::srv
