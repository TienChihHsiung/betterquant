/*!
 * \file FlowCtrlOnStepOnCancelOrderRet.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/12
 *
 * \brief
 */

#pragma once

#include "FlowCtrlOnStepBase.hpp"

namespace bq::td::srv {

class FlowCtrlOnStepOnCancelOrderRet : public FlowCtrlOnStepBase {
 public:
  FlowCtrlOnStepOnCancelOrderRet(const FlowCtrlOnStepOnCancelOrderRet&) =
      delete;
  FlowCtrlOnStepOnCancelOrderRet& operator=(
      const FlowCtrlOnStepOnCancelOrderRet&) = delete;
  FlowCtrlOnStepOnCancelOrderRet(const FlowCtrlOnStepOnCancelOrderRet&&) =
      delete;
  FlowCtrlOnStepOnCancelOrderRet& operator=(
      const FlowCtrlOnStepOnCancelOrderRet&&) = delete;

  using FlowCtrlOnStepBase::FlowCtrlOnStepBase;

 private:
  int doHandleFlowCtrl(const OrderInfoSPtr& orderInfo) final;
};

using FlowCtrlOnStepOnCancelOrderRetSPtr =
    std::shared_ptr<FlowCtrlOnStepOnCancelOrderRet>;

}  // namespace bq::td::srv
