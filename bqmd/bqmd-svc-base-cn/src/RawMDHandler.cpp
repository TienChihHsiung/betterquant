/*!
 * \file RawMDHandler.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/11/16
 *
 * \brief
 */

#include "RawMDHandler.hpp"

#include "Config.hpp"
#include "MDStorageSvc.hpp"
#include "MDSvcOfCN.hpp"
#include "SHMIPCConst.hpp"
#include "SHMIPCMsgId.hpp"
#include "SHMIPCUtil.hpp"
#include "SHMSrv.hpp"
#include "db/TBLRecSetMaker.hpp"
#include "db/TBLSymbolInfo.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/DataStruOfMD.hpp"
#include "def/RawMD.hpp"
#include "def/RawMDAsyncTaskArg.hpp"
#include "util/BQMDUtil.hpp"
#include "util/Datetime.hpp"
#include "util/File.hpp"
#include "util/FlowCtrlSvc.hpp"
#include "util/Literal.hpp"
#include "util/MarketDataCond.hpp"
#include "util/String.hpp"
#include "util/TaskDispatcher.hpp"

namespace bq::md::svc {

RawMDHandler::RawMDHandler(MDSvcOfCN const* mdSvc)
    : mdSvc_(mdSvc),
      topic2LastExchTsGroup_(std::make_shared<Topic2LastTsGroup>()),
      saveMarketData_(CONFIG["saveMarketData"].as<bool>(false)),
      checkIFExchTsOfMDIsInc_(CONFIG["checkIFExchTsOfMDIsInc"].as<bool>(true)) {
}

int RawMDHandler::init() {
  const auto rawMDHandlerParamInStrFmt =
      SetParam(DEFAULT_TASK_DISPATCHER_PARAM,
               CONFIG["rawMDHandlerParam"].as<std::string>());
  const auto [ret, rawMDHandlerParam] =
      MakeTaskDispatcherParam(rawMDHandlerParamInStrFmt);
  if (ret != 0) {
    LOG_E("Init failed. {}", rawMDHandlerParamInStrFmt);
    return ret;
  }

  taskDispatcher_ = std::make_shared<TaskDispatcher<RawMDSPtr>>(
      rawMDHandlerParam,
      [this](const auto& task) { return makeAsyncTask(task); },
      [](auto& asyncTask, auto taskSpecificThreadPoolSize) {
        InitTopicInfo(asyncTask->task_);
        const auto threadNo =
            asyncTask->task_->topicHash_ % taskSpecificThreadPoolSize;
        return threadNo;
      },
      [this](auto& asyncTask) { handle(asyncTask); });

  taskDispatcher_->init();
  return ret;
}

void RawMDHandler::start() { taskDispatcher_->start(); }
void RawMDHandler::stop() { taskDispatcher_->stop(); }

void RawMDHandler::dispatch(RawMDSPtr& task) {
  taskDispatcher_->dispatch(task);
}

void RawMDHandler::handle(RawMDAsyncTaskSPtr& asyncTask) {
  switch (asyncTask->task_->msgType_) {
    case MsgType::NewSymbol:
      handleNewSymbol(asyncTask);
      break;

    case MsgType::Tickers:
      if (handleMDTickers(asyncTask) && saveMarketData_) {
        mdSvc_->getMDStorageSvc()->dispatch(asyncTask);
      }
      break;

    case MsgType::Trades:
      if (handleMDTrades(asyncTask) && saveMarketData_) {
        mdSvc_->getMDStorageSvc()->dispatch(asyncTask);
      }
      break;

    case MsgType::Orders:
      if (handleMDOrders(asyncTask) && saveMarketData_) {
        mdSvc_->getMDStorageSvc()->dispatch(asyncTask);
      }
      break;

    case MsgType::Books:
      if (handleMDBooks(asyncTask) && saveMarketData_) {
        mdSvc_->getMDStorageSvc()->dispatch(asyncTask);
      }
      break;

    default:
      assert(1 == 2 && "Entered an impossible code segment");
      break;
  }
}

bool RawMDHandler::checkIfMDIsLegal(RawMDAsyncTaskSPtr& asyncTask,
                                    std::uint64_t exchTs) {
  if (!checkIFExchTsOfMDIsInc_) return true;
  const auto topic = asyncTask->task_->topic_;
  {
    std::lock_guard<std::mutex> guard(mtxTopic2LastExchTsGroup_);
    auto iter = topic2LastExchTsGroup_->find(topic);
    if (iter == std::end(*topic2LastExchTsGroup_)) {
      (*topic2LastExchTsGroup_)[topic] = exchTs;
      return true;
    } else {
      auto& exchTsInCache = iter->second;
      if (exchTsInCache <= exchTs) {
        exchTsInCache = exchTs;
        return true;
      } else {
        LOG_W("Found exchTs in cache {} greater than exchTs {}. {}",
              exchTsInCache, exchTs, topic);
        return false;
      }
    }
  }
  return true;
}

}  // namespace bq::md::svc
