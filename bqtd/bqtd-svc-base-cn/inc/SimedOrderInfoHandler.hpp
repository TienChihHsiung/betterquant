/*!
 * \file SimedOrderInfoHandler.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/01
 *
 * \brief
 */

#pragma once

#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq {
struct OrderInfo;
using OrderInfoSPTr = std::shared_ptr<OrderInfo>;
struct SimedTDInfo;
using SimedTDInfoSPtr = std::shared_ptr<SimedTDInfo>;
}  // namespace bq

namespace bq::db::symbolInfo {
struct FieldGroupOfAll;
using Record = FieldGroupOfAll;
using RecordSPtr = std::shared_ptr<Record>;
}  // namespace bq::db::symbolInfo

namespace bq::td::svc {

class TDSvcOfCN;

class SimedOrderInfoHandler {
 public:
  SimedOrderInfoHandler(const SimedOrderInfoHandler&) = delete;
  SimedOrderInfoHandler& operator=(const SimedOrderInfoHandler&) = delete;
  SimedOrderInfoHandler(const SimedOrderInfoHandler&&) = delete;
  SimedOrderInfoHandler& operator=(const SimedOrderInfoHandler&&) = delete;

  explicit SimedOrderInfoHandler(TDSvcOfCN* tdSvc);

 public:
#ifdef SIMED_MODE
  void simOnOrder(OrderInfoSPTr& ordReq);
#endif

 private:
  void simOnOrder(OrderInfoSPTr& ordReq, const SimedTDInfoSPtr& simedTDInfo);

  db::symbolInfo::RecordSPtr simOnOrderConfirmedByExch(
      OrderInfoSPTr& ordReq, const SimedTDInfoSPtr& simedTDInfo);
  void simOnOrderFilled(OrderInfoSPTr& ordReq,
                        const SimedTDInfoSPtr& simedTDInfo,
                        const db::symbolInfo::RecordSPtr& symbolInfo);
  void simOnOrderPartialFilled(OrderInfoSPTr& ordReq,
                               const SimedTDInfoSPtr& simedTDInfo,
                               const db::symbolInfo::RecordSPtr& symbolInfo);
  void simOnOrderFailed(OrderInfoSPTr& ordReq,
                        const SimedTDInfoSPtr& simedTDInfo);
  void simOnOrderOfUnknownOrderStatus(OrderInfoSPTr& ordReq,
                                      const SimedTDInfoSPtr& simedTDInfo);

 public:
  void simOnCancelOrder(OrderInfoSPTr& ordReq);

 private:
  TDSvcOfCN* tdSvc_;

  std::string defaultFeeCurrency_;
  Decimal feeRatioOfMaker_;
  Decimal feeRatioOfTaker_;
  std::uint32_t milliSecIntervalOfSimOrderStatus_{0};
};

}  // namespace bq::td::svc
