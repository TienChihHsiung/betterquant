/*!
 * \file FlowCtrlOnStepOnCancelOrderRet.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#include "FlowCtrlOnStepOnCancelOrderRet.hpp"

#include "FlowCtrlConst.hpp"
#include "FlowCtrlUtil.hpp"
#include "TDSrvRiskPluginFlowCtrlPlus.hpp"
#include "def/DataStruOfTD.hpp"

namespace bq::td::srv {

int FlowCtrlOnStepOnCancelOrderRet::doHandleFlowCtrl(
    const OrderInfoSPtr& orderInfo) {
  int statusCode = 0;
  return 0;
}

}  // namespace bq::td::srv
