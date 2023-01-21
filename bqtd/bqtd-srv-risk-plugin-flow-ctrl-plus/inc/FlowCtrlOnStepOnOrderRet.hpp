/*!
 * \file FlowCtrlOnStepOnOrderRet.hpp
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

class FlowCtrlOnStepOnOrderRet : public FlowCtrlOnStepBase {
 public:
  FlowCtrlOnStepOnOrderRet(const FlowCtrlOnStepOnOrderRet&) = delete;
  FlowCtrlOnStepOnOrderRet& operator=(const FlowCtrlOnStepOnOrderRet&) = delete;
  FlowCtrlOnStepOnOrderRet(const FlowCtrlOnStepOnOrderRet&&) = delete;
  FlowCtrlOnStepOnOrderRet& operator=(const FlowCtrlOnStepOnOrderRet&&) =
      delete;

  using FlowCtrlOnStepBase::FlowCtrlOnStepBase;

 private:
  int doHandleFlowCtrl(const OrderInfoSPtr& orderInfo) final;
};

using FlowCtrlOnStepOnOrderRetSPtr = std::shared_ptr<FlowCtrlOnStepOnOrderRet>;

}  // namespace bq::td::srv
