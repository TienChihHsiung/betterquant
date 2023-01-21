/*!
 * \file RawTDHandler.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/02
 *
 * \brief
 */

#include "RawTDHandler.hpp"

#include "Config.hpp"
#include "SHMIPCConst.hpp"
#include "SHMIPCMsgId.hpp"
#include "SHMIPCUtil.hpp"
#include "SHMSrv.hpp"
#include "TDSvcOfCN.hpp"
#include "db/TBLRecSetMaker.hpp"
#include "db/TBLSymbolInfo.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/DataStruOfTD.hpp"
#include "def/RawTD.hpp"
#include "util/Datetime.hpp"
#include "util/File.hpp"
#include "util/FlowCtrlSvc.hpp"
#include "util/Literal.hpp"
#include "util/MarketDataCond.hpp"
#include "util/String.hpp"
#include "util/TaskDispatcher.hpp"

namespace bq::td::svc {

RawTDHandler::RawTDHandler(TDSvcOfCN const* tdSvc) : tdSvc_(tdSvc) {}

int RawTDHandler::init() {
  const auto rawTDHandlerParamInStrFmt =
      SetParam(DEFAULT_TASK_DISPATCHER_PARAM,
               CONFIG["rawTDHandlerParam"].as<std::string>());
  const auto [ret, rawTDHandlerParam] =
      MakeTaskDispatcherParam(rawTDHandlerParamInStrFmt);
  if (ret != 0) {
    LOG_E("Init failed. {}", rawTDHandlerParamInStrFmt);
    return ret;
  }

  taskDispatcher_ = std::make_shared<TaskDispatcher<RawTDSPtr>>(
      rawTDHandlerParam,
      [this](const auto& task) { return makeAsyncTask(task); },
      [](auto& asyncTask, auto taskSpecificThreadPoolSize) {
        if (asyncTask->task_->msgType_ == MsgType::Order) {
          return ThreadNo(0);
        } else {
          return ThreadNo(1);
        }
      },
      [this](auto& asyncTask) { handle(asyncTask); });

  taskDispatcher_->init();
  return ret;
}

void RawTDHandler::start() { taskDispatcher_->start(); }
void RawTDHandler::stop() { taskDispatcher_->stop(); }

void RawTDHandler::dispatch(RawTDSPtr& task) {
  taskDispatcher_->dispatch(task);
}

void RawTDHandler::handle(RawTDAsyncTaskSPtr& asyncTask) {
  switch (asyncTask->task_->msgType_) {
    case MsgType::Order:
      break;

    case MsgType::SyncUnclosedOrder:
      break;

    case MsgType::SyncAssetsSnapshot:
      break;

    default:
      assert(1 == 2 && "Entered an impossible code segment");
      break;
  }
}

void RawTDHandler::handleOrder(RawTDAsyncTaskSPtr& asyncTask) {}

void RawTDHandler::handleSyncUnclosedOrder(RawTDAsyncTaskSPtr& asyncTask) {}

void RawTDHandler::handleSyncAssetsSnapshot(RawTDAsyncTaskSPtr& asyncTask) {}

}  // namespace bq::td::svc
