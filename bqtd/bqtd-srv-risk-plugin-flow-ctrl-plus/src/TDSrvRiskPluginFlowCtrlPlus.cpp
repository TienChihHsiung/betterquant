/*!
 * \file TDSrvRiskPluginFlowCtrlPlus.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "TDSrvRiskPluginFlowCtrlPlus.hpp"

#include "AssetsMgr.hpp"
#include "FlowCtrlConst.hpp"
#include "FlowCtrlDef.hpp"
#include "FlowCtrlOnStepOnCancelOrder.hpp"
#include "FlowCtrlOnStepOnCancelOrderRet.hpp"
#include "FlowCtrlOnStepOnOrder.hpp"
#include "FlowCtrlOnStepOnOrderRet.hpp"
#include "FlowCtrlRuleMgr.hpp"
#include "FlowCtrlUtil.hpp"
#include "OrdMgr.hpp"
#include "PosMgr.hpp"
#include "TDSrv.hpp"
#include "db/TBLMonitorOfFlowCtrlRule.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "def/StatusCode.hpp"
#include "util/FlowCtrlSvc.hpp"
#include "util/Logger.hpp"
#include "util/Random.hpp"
#include "util/StdExt.hpp"

namespace bq::td::srv {

void TDSrvRiskPluginFlowCtrlPlus::doOnThreadStart(std::uint32_t threadNo) {
  const auto& tbl = getTDSrv()->getTBLMonitorOfFlowCtrlRule();
  const auto& recSet = tbl->getFlowCtrlRuleGroup();

  const auto [sCodeOfInit, sMsgOfInit] =
      std::ext::tls_get<FlowCtrlRuleMgr>().init(recSet);
  if (sCodeOfInit != 0) {
    L_W(logger(), "[{}] {}", name(), sMsgOfInit);
  } else {
    L_I(logger(), "Init flow ctrl rule mgr {}.",
        static_cast<void*>(&std::ext::tls_get<FlowCtrlRuleMgr>()));
  }

  L_I(logger(), "[{}] Load risk of flow ctrl data from db.", name());
  const auto [sCodeOfLoad, sMsgOfLoad] =
      std::ext::tls_get<FlowCtrlRuleMgr>().loadFromDB(getTDSrv()->getDBEng());
  if (sCodeOfLoad != 0) {
    L_W(logger(), "[{}] {}", name(), sMsgOfLoad);
  }
}

void TDSrvRiskPluginFlowCtrlPlus::doOnThreadExit(std::uint32_t threadNo) {
  L_I(logger(), "[{}] Save risk of flow ctrl data to db.", name());
  const auto [statusCode, statusMsg] =
      std::ext::tls_get<FlowCtrlRuleMgr>().saveToDB(getTDSrv()->getDBEng());
  if (statusCode != 0) {
    L_W(logger(), "[{}] {}", name(), statusMsg);
  }
}

int TDSrvRiskPluginFlowCtrlPlus::doLoad() {
  initFlowCtrlTargetState();

  flowCtrlOnStepOnOrder_ = std::make_shared<FlowCtrlOnStepOnOrder>(this);
  flowCtrlOnStepOnCancelOrder_ =
      std::make_shared<FlowCtrlOnStepOnCancelOrder>(this);
  flowCtrlOnStepOnOrderRet_ = std::make_shared<FlowCtrlOnStepOnOrderRet>(this);
  flowCtrlOnStepOnCancelOrderRet_ =
      std::make_shared<FlowCtrlOnStepOnCancelOrderRet>(this);

  return 0;
}

void TDSrvRiskPluginFlowCtrlPlus::initFlowCtrlTargetState() {
  flowCtrlTargetState_ = std::make_shared<FlowCtrlTargetState>();

  flowCtrlTargetState_->orderSizeEachTime_ =
      node_["orderSizeEachTime"].as<bool>(true);

  flowCtrlTargetState_->orderSizeTotal_ =  //
      node_["orderSizeTotal"].as<bool>(true);

  flowCtrlTargetState_->orderAmtEachTime_ =
      node_["orderAmtEachTime"].as<bool>(true);

  flowCtrlTargetState_->orderAmtTotal_ =  //
      node_["orderAmtTotal"].as<bool>(true);

  flowCtrlTargetState_->orderTimesTotal_ =  //
      node_["orderTimesTotal"].as<bool>(true);

  flowCtrlTargetState_->orderTimesWithinTime_ =
      node_["orderTimesWithinTime"].as<bool>(true);

  flowCtrlTargetState_->cancelOrderTimesTotal_ =
      node_["cancelOrderTimesTotal"].as<bool>(true);

  flowCtrlTargetState_->cancelOrderTimesWithinTime_ =
      node_["cancelOrderTimesWithinTime"].as<bool>(true);

  flowCtrlTargetState_->rejectOrderTimesTotal_ =
      node_["rejectOrderTimesTotal"].as<bool>(true);

  flowCtrlTargetState_->rejectOrderTimesWithinTime_ =
      node_["rejectOrderTimesWithinTime"].as<bool>(true);

  flowCtrlTargetState_->rejectOrderTimesWithinTime_ =
      node_["dealSizeTotal"].as<bool>(true);
}

void TDSrvRiskPluginFlowCtrlPlus::doUnload() {}

boost::dll::fs::path TDSrvRiskPluginFlowCtrlPlus::getLocation() const {
  return boost::dll::this_line_location();
}

TDSrvRiskPluginFlowCtrlPlus::TDSrvRiskPluginFlowCtrlPlus(TDSrv* tdSrv)
    : TDSrvRiskPlugin(tdSrv) {}

int TDSrvRiskPluginFlowCtrlPlus::doOnOrder(const OrderInfoSPtr& orderInfo) {
  L_I(logger(), "[{}] On order req.", name());
  const auto ret = flowCtrlOnStepOnOrder_->handleFlowCtrl(orderInfo);
  return ret;
}

int TDSrvRiskPluginFlowCtrlPlus::doOnCancelOrder(
    const OrderInfoSPtr& orderInfo) {
  L_I(logger(), "[{}] On cancel order req.", name());
  const auto ret = flowCtrlOnStepOnCancelOrder_->handleFlowCtrl(orderInfo);
  return ret;
}

int TDSrvRiskPluginFlowCtrlPlus::doOnOrderRet(const OrderInfoSPtr& orderInfo) {
  L_I(logger(), "[{}] On order ret.", name());
  const auto ret = flowCtrlOnStepOnOrderRet_->handleFlowCtrl(orderInfo);
  return ret;
}

int TDSrvRiskPluginFlowCtrlPlus::doOnCancelOrderRet(
    const OrderInfoSPtr& orderInfo) {
  L_I(logger(), "[{}] On cancel order ret.", name());
  const auto ret = flowCtrlOnStepOnCancelOrderRet_->handleFlowCtrl(orderInfo);
  return ret;
}

}  // namespace bq::td::srv
