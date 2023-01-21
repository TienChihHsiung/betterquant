/*!
 * \file TDSrvRiskPluginFlowCtrlPlus.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "TDSrvRiskPlugin.hpp"
#include "util/Pch.hpp"

namespace bq {
struct OrderInfo;
using OrderInfoSPtr = std::shared_ptr<OrderInfo>;
struct FlowCtrlTargetState;
using FlowCtrlTargetStateSPtr = std::shared_ptr<FlowCtrlTargetState>;
}  // namespace bq

namespace bq::td::srv {

class FlowCtrlOnStepOnCancelOrder;
using FlowCtrlOnStepOnCancelOrderSPtr =
    std::shared_ptr<FlowCtrlOnStepOnCancelOrder>;
class FlowCtrlOnStepOnCancelOrderRet;
using FlowCtrlOnStepOnCancelOrderRetSPtr =
    std::shared_ptr<FlowCtrlOnStepOnCancelOrderRet>;
class FlowCtrlOnStepOnOrder;
using FlowCtrlOnStepOnOrderSPtr = std::shared_ptr<FlowCtrlOnStepOnOrder>;
class FlowCtrlOnStepOnOrderRet;
using FlowCtrlOnStepOnOrderRetSPtr = std::shared_ptr<FlowCtrlOnStepOnOrderRet>;

class TDSrv;

class BOOST_SYMBOL_VISIBLE TDSrvRiskPluginFlowCtrlPlus
    : public TDSrvRiskPlugin {
 public:
  TDSrvRiskPluginFlowCtrlPlus(const TDSrvRiskPluginFlowCtrlPlus&) = delete;
  TDSrvRiskPluginFlowCtrlPlus& operator=(const TDSrvRiskPluginFlowCtrlPlus&) =
      delete;
  TDSrvRiskPluginFlowCtrlPlus(const TDSrvRiskPluginFlowCtrlPlus&&) = delete;
  TDSrvRiskPluginFlowCtrlPlus& operator=(const TDSrvRiskPluginFlowCtrlPlus&&) =
      delete;

  explicit TDSrvRiskPluginFlowCtrlPlus(TDSrv* tdSrv);

 public:
  FlowCtrlTargetStateSPtr getFlowCtrlTargetState() const {
    return flowCtrlTargetState_;
  }

 private:
  void doOnThreadStart(std::uint32_t threadNo) final;
  void doOnThreadExit(std::uint32_t threadNo) final;

 private:
  int doLoad() final;
  void initFlowCtrlTargetState();

 private:
  void doUnload() final;

 private:
  boost::dll::fs::path getLocation() const final;

 private:
  int doOnOrder(const OrderInfoSPtr& orderInfo) final;
  int doOnCancelOrder(const OrderInfoSPtr& orderInfo) final;

 private:
  int doOnOrderRet(const OrderInfoSPtr& orderInfo) final;
  int doOnCancelOrderRet(const OrderInfoSPtr& orderInfo) final;

 private:
  FlowCtrlTargetStateSPtr flowCtrlTargetState_{nullptr};

  FlowCtrlOnStepOnOrderSPtr flowCtrlOnStepOnOrder_{nullptr};
  FlowCtrlOnStepOnCancelOrderSPtr flowCtrlOnStepOnCancelOrder_{nullptr};
  FlowCtrlOnStepOnOrderRetSPtr flowCtrlOnStepOnOrderRet_{nullptr};
  FlowCtrlOnStepOnCancelOrderRetSPtr flowCtrlOnStepOnCancelOrderRet_{nullptr};
};

}  // namespace bq::td::srv

inline bq::td::srv::TDSrvRiskPluginFlowCtrlPlus* Create(
    bq::td::srv::TDSrv* tdSrv) {
  return new bq::td::srv::TDSrvRiskPluginFlowCtrlPlus(tdSrv);
}

BOOST_DLL_ALIAS_SECTIONED(Create, CreatePlugin, PlugIn)
