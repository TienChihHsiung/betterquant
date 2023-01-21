/*!
 * \file StgEngImpl.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "StgEngImpl.hpp"

#include "OrdMgr.hpp"
#include "PosMgr.hpp"
#include "SHMIPC.hpp"
#include "StgEngConst.hpp"
#include "StgEngUtil.hpp"
#include "StgInstTaskHandlerImpl.hpp"
#include "db/DBE.hpp"
#include "db/DBEngConst.hpp"
#include "db/TBLMonitorOfStgInstInfo.hpp"
#include "db/TBLMonitorOfSymbolInfo.hpp"
#include "def/AssetInfo.hpp"
#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/CommonIPCData.hpp"
#include "def/Const.hpp"
#include "def/DataStruOfOthers.hpp"
#include "def/DataStruOfStg.hpp"
#include "def/Def.hpp"
#include "def/Pnl.hpp"
#include "def/SimedTDInfo.hpp"
#include "def/StatusCode.hpp"
#include "def/SyncTask.hpp"
#include "tdeng/TDEngConnpool.hpp"
#include "tdeng/TDEngConst.hpp"
#include "tdeng/TDEngParam.hpp"
#include "util/AcctInfoCache.hpp"
#include "util/File.hpp"
#include "util/Literal.hpp"
#include "util/MarketDataCache.hpp"
#include "util/MarketDataCond.hpp"
#include "util/Random.hpp"
#include "util/ScheduleTaskBundle.hpp"
#include "util/Scheduler.hpp"
#include "util/String.hpp"
#include "util/SubMgr.hpp"
#include "util/TaskDispatcher.hpp"
#include "util/TopicMgr.hpp"
#include "util/Util.hpp"

namespace bq::stg {

StgEngImpl::StgEngImpl(const std::string& configFilename)
    : SvcBase(configFilename) {}

int StgEngImpl::prepareInit() {
  syncTaskGroup_.reserve(1024);

  try {
    config_ = YAML::LoadFile(configFilename_);
  } catch (const std::exception& e) {
    std::cerr << fmt::format("Open config filename {} failed. [{}]",
                             configFilename_, e.what())
              << std::endl;
    return SCODE_STG_ENG_INVALID_CONFIG_FILENAME;
  }

  const auto ret = InitLogger(configFilename_);
  if (ret != 0) {
    const auto statusMsg =
        fmt::format("Init stg {} failed because of init logger failed. {}",
                    getStgId(), configFilename_);
    std::cerr << statusMsg << std::endl;
    return ret;
  }

  return 0;
}

int StgEngImpl::doInit() {
  stgId_ = getConfig()["stgId"].as<StgId>();
  appName_ = fmt::format("Stg-{}", getStgId());
  rootDirOfStgPrivateData_ = fmt::format(
      "{}/{}", getConfig()["rootDirOfStgPrivateData"].as<std::string>(),
      getStgId());

  if (auto ret = initDBEng(); ret != 0) {
    LOG_E("[{}] Init failed.", appName_);
    return ret;
  }

  initTBLMonitorOfSymbolInfo();
  initTBLMonitorOfStgInstInfo();

  if (const auto ret = initTDEng(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  acctInfoCache_ = std::make_shared<AcctInfoCache>(getDBEng());
  marketDataCache_ = std::make_shared<MarketDataCache>();

  initSubMgr();
  initTopicMgr();
  initOrdMgr();
  initPosMgr();

  initStgInstTaskDispatcher();

  initSHMCliOfTDSrv();
  initSHMCliOfRiskMgr();
  initSHMCliOfWebSrv();

  scheduleTaskBundle_ = std::make_shared<ScheduleTaskBundle>();
  initScheduleTaskBundle();
  scheduleTaskBundleExecutor_ = std::make_shared<Scheduler>(
      getAppName(),
      [this]() {
        auto scheduleTaskBundle = getScheduleTaskBundle();
        ExecScheduleTaskBundle(scheduleTaskBundle);
      },
      1);

  return 0;
}

int StgEngImpl::initDBEng() {
  const auto dbEngParam = SetParam(db::DEFAULT_DB_ENG_PARAM,
                                   getConfig()["dbEngParam"].as<std::string>());
  int retOfMakeDBEng = 0;
  std::tie(retOfMakeDBEng, dbEng_) = db::MakeDBEng(
      dbEngParam, [this](db::DBTaskSPtr& dbTask, const StringSPtr& dbExecRet) {
        LOG_D("[{}] Exec sql finished. [{}] [exec result = {}]", appName_,
              dbTask->toStr(), *dbExecRet);
      });
  if (retOfMakeDBEng != 0) {
    LOG_E("[{}] Init dbeng failed. {}", appName_, dbEngParam);
    return retOfMakeDBEng;
  }

  if (auto retOfInit = getDBEng()->init(); retOfInit != 0) {
    LOG_E("[{}] Init dbeng failed. {}", appName_, dbEngParam);
    return retOfInit;
  }

  return 0;
}

int StgEngImpl::initTDEng() {
  const auto tdEngParamInStrFmt = SetParam(
      tdeng::DEFAULT_TDENG_PARAM, getConfig()["tdEngParam"].as<std::string>());
  const auto [ret, tdEngParam] = tdeng::MakeTDEngParam(tdEngParamInStrFmt);
  if (ret != 0) {
    LOG_E("Init failed. {}", tdEngParamInStrFmt);
    return ret;
  }

  tdEngConnpool_ = std::make_shared<tdeng::TDEngConnpool>(tdEngParam);
  if (const auto statusCode = tdEngConnpool_->init(); statusCode != 0) {
    LOG_E("Init failed. {}", tdEngParamInStrFmt);
    return -1;
  }

  return 0;
}

void StgEngImpl::initTBLMonitorOfSymbolInfo() {
  const auto milliSecIntervalOfTBLMonitorOfSymbolInfo =
      getConfig()["milliSecIntervalOfTBLMonitorOfSymbolInfo"]
          .as<std::uint32_t>();
  const auto enableSymbolInfoMonitoring =
      getConfig()["enableSymbolInfoMonitoring"].as<bool>(true)
          ? db::EnableMonitoring::True
          : db::EnableMonitoring::False;
  const auto sql = fmt::format("SELECT * FROM {}", TBLSymbolInfo::TableName);
  tblMonitorOfSymbolInfo_ = std::make_shared<db::TBLMonitorOfSymbolInfo>(
      getDBEng(), milliSecIntervalOfTBLMonitorOfSymbolInfo, sql, nullptr,
      enableSymbolInfoMonitoring);
}

void StgEngImpl::initTBLMonitorOfStgInstInfo() {
  const auto cbOnStgInstInfoChg = [this](const auto& tblRecSetAdd,
                                         const auto& tblRecSetDel,
                                         const auto& tblRecSetChg) {
    for (const auto tblRec : *tblRecSetAdd) {
      const auto stgInstId = tblRec.second->getRecWithAllFields()->stgInstId;
      auto asynTask = MakeStgSignal(MSG_ID_ON_STG_INST_ADD, stgInstId);
      stgInstTaskDispatcher_->dispatch(asynTask);
    }
    for (const auto tblRec : *tblRecSetDel) {
      const auto stgInstId = tblRec.second->getRecWithAllFields()->stgInstId;
      auto asynTask = MakeStgSignal(MSG_ID_ON_STG_INST_DEL, stgInstId);
      stgInstTaskDispatcher_->dispatch(asynTask);
    }
    for (const auto tblRec : *tblRecSetChg) {
      const auto stgInstId = tblRec.second->getRecWithAllFields()->stgInstId;
      auto asynTask = MakeStgSignal(MSG_ID_ON_STG_INST_CHG, stgInstId);
      stgInstTaskDispatcher_->dispatch(asynTask);
    }
  };

  const auto milliSecIntervalOfTBLMonitorOfStgInstInfo =
      getConfig()["milliSecIntervalOfTBLMonitorOfStgInstInfo"]
          .as<std::uint32_t>();
  const auto sql = fmt::format(
      "SELECT a.`productId`, a.`stgId`, a.`stgName`, a.`userIdOfAuthor`, "
      "b.`stgInstId`, b.`stgInstParams`, b.`stgInstName`, b.`userId`, "
      "b.`isDel` FROM stgInfo a, stgInstInfo b WHERE a.`stgId` = {} AND "
      "a.`stgId` = b.`stgId` AND b.`isDel` = 0; ",
      getStgId());
  tblMonitorOfStgInstInfo_ = std::make_shared<db::TBLMonitorOfStgInstInfo>(
      getDBEng(), milliSecIntervalOfTBLMonitorOfStgInstInfo, sql,
      cbOnStgInstInfoChg);
}

void StgEngImpl::initSubMgr() {
  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
    const auto shmHeader = static_cast<const SHMHeader*>(shmBuf);
    const auto subscriberGroup =
        subMgr_->getSubscriberGroupByTopicHash(shmHeader->topicHash_);
    for (auto stgInstId : subscriberGroup) {
      auto asyncTask = std::make_shared<SHMIPCAsyncTask>(
          std::make_shared<SHMIPCTask>(shmBuf, shmBufLen), stgInstId);
      stgInstTaskDispatcher_->dispatch(asyncTask);
    }
  };

  subMgr_ = std::make_shared<SubMgr>(appName_, onSHMDataRecv);
}

void StgEngImpl::initTopicMgr() {
  topicMgr_ = std::make_shared<TopicMgr>(
      TopicMgrRole::Cli, [this](const auto& anyData) {
        const auto mdSvcIdentity =
            fmt::format("{}{}{}", SEP_OF_SHM_SVC, TOPIC_PREFIX_OF_MARKET_DATA,
                        SEP_OF_SHM_SVC);
        const auto subscriber2TopicGroupInJsonFmt =
            std::any_cast<std::string>(anyData);
        const auto addr2SHMCliGroup = subMgr_->getSHMCliGroup();
        const auto shmBufLen =
            sizeof(CommonIPCData) + subscriber2TopicGroupInJsonFmt.size() + 1;
        for (const auto& addr2SHMCliGroup : addr2SHMCliGroup) {
          if (!boost::contains(addr2SHMCliGroup.first, mdSvcIdentity)) {
            continue;
          }
          addr2SHMCliGroup.second->asyncSendMsgWithZeroCopy(
              [&](void* shmBuf) {
                auto commonIPCData = static_cast<CommonIPCData*>(shmBuf);
                memcpy(commonIPCData->data_,
                       subscriber2TopicGroupInJsonFmt.c_str(),
                       subscriber2TopicGroupInJsonFmt.size());
                commonIPCData->dataLen_ =
                    subscriber2TopicGroupInJsonFmt.size() + 1;
              },
              MSG_ID_SYNC_SUB_INFO, shmBufLen);
        }
      });
}

void StgEngImpl::initSHMCliOfTDSrv() {
  const auto stgEngChannelOfTDSrv =
      getConfig()["stgEngChannelOfTDSrv"].as<std::string>();
  const auto addr =
      fmt::format("{}{}{}", appName_, SEP_OF_SHM_SVC, stgEngChannelOfTDSrv);

  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
    const auto shmHeader = static_cast<const SHMHeader*>(shmBuf);
    StgInstId stgInstId = 1;
    if (shmHeader->msgId_ == MSG_ID_ON_ORDER_RET ||
        shmHeader->msgId_ == MSG_ID_ON_CANCEL_ORDER_RET) {
      stgInstId = static_cast<const OrderInfo*>(shmBuf)->stgInstId_;
    }
    auto asyncTask = std::make_shared<SHMIPCAsyncTask>(
        std::make_shared<SHMIPCTask>(shmBuf, shmBufLen), stgInstId);
    stgInstTaskDispatcher_->dispatch(asyncTask);
  };

  shmCliOfTDSrv_ = std::make_shared<SHMCli>(addr, onSHMDataRecv);
  shmCliOfTDSrv_->setClientChannel(getStgId());
}

void StgEngImpl::initSHMCliOfRiskMgr() {
  const auto stgEngChannelOfRiskMgr =
      getConfig()["stgEngChannelOfRiskMgr"].as<std::string>();
  const auto addr =
      fmt::format("{}{}{}", appName_, SEP_OF_SHM_SVC, stgEngChannelOfRiskMgr);

  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
    const auto shmHeader = static_cast<const SHMHeader*>(shmBuf);
    StgInstId stgInstId = 1;
    if (shmHeader->msgId_ == MSG_ID_ON_ORDER_RET ||
        shmHeader->msgId_ == MSG_ID_ON_CANCEL_ORDER_RET) {
      stgInstId = static_cast<const OrderInfo*>(shmBuf)->stgInstId_;
    }
    auto asyncTask = std::make_shared<SHMIPCAsyncTask>(
        std::make_shared<SHMIPCTask>(shmBuf, shmBufLen), stgInstId);
    stgInstTaskDispatcher_->dispatch(asyncTask);
  };

  shmCliOfRiskMgr_ = std::make_shared<SHMCli>(addr, onSHMDataRecv);
  shmCliOfRiskMgr_->setClientChannel(getStgId());
}

void StgEngImpl::initSHMCliOfWebSrv() {
  const auto stgEngChannelOfWebSrv =
      getConfig()["stgEngChannelOfWebSrv"].as<std::string>();
  const auto addr =
      fmt::format("{}{}{}", appName_, SEP_OF_SHM_SVC, stgEngChannelOfWebSrv);

  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
    const auto msgId = static_cast<const SHMHeader*>(shmBuf)->msgId_;
    switch (msgId) {
      case MSG_ID_ON_STG_MANUAL_INTERVENTION: {
        const auto [statusCode, stgInstId] =
            GetStgInstId(static_cast<const CommonIPCData*>(shmBuf));
        if (statusCode != 0) {
          LOG_W("Get invalid stgInstId from common ipc data of {} - {}.", msgId,
                GetMsgName(msgId));
          return;
        }
        LOG_I("Dispatch manual intervention: {}",
              static_cast<const CommonIPCData*>(shmBuf)->data_);
        auto asyncTask = std::make_shared<SHMIPCAsyncTask>(
            std::make_shared<SHMIPCTask>(shmBuf, shmBufLen), stgInstId);
        stgInstTaskDispatcher_->dispatch(asyncTask);
      } break;

      default:
        LOG_W("Unhandled msgId {}.", msgId);
        break;
    }
  };

  shmCliOfWebSrv_ = std::make_shared<SHMCli>(addr, onSHMDataRecv);
  shmCliOfWebSrv_->setClientChannel(getStgId());
}

void StgEngImpl::initOrdMgr() {
  const auto filled = magic_enum::enum_integer(OrderStatus::Filled);
  ordMgr_ = std::make_shared<OrdMgr>();
  const auto sql = fmt::format(
      "SELECT * FROM `orderInfo` WHERE `stgId` = {} AND `orderStatus` < {}; ",
      getStgId(), filled);
  getOrdMgr()->init(getConfig(), getDBEng(), sql);
}

void StgEngImpl::initPosMgr() {
  const auto sql =
      fmt::format("SELECT * FROM `posInfo` WHERE `stgId` = {}", getStgId());
  posMgr_ = std::make_shared<PosMgr>();
  getPosMgr()->init(getConfig(), getDBEng(), sql);
}

int StgEngImpl::initStgInstTaskDispatcher() {
  const auto stgInstTaskDispatcherParamInStrFmt =
      SetParam(DEFAULT_TASK_DISPATCHER_PARAM,
               getConfig()["stgInstTaskDispatcherParam"].as<std::string>());
  const auto [ret, stgInstTaskDispatcherParam] =
      MakeTaskDispatcherParam(stgInstTaskDispatcherParamInStrFmt);
  if (ret != 0) {
    LOG_E("[{}] Init taskdispatcher failed. {}", appName_,
          stgInstTaskDispatcherParamInStrFmt);
    return ret;
  }

  const auto getThreadForAsyncTask = [](const auto& asyncTask,
                                        auto taskSpecificThreadPoolSize) {
    const auto stgInstId = std::any_cast<StgInstId>(asyncTask->arg_);
    return (stgInstId - 1) % taskSpecificThreadPoolSize;
  };

  const auto handleAsyncTask = [this](auto& asyncTask) {
    stgInstTaskHandler_->handleAsyncTask(asyncTask);
  };

  stgInstTaskDispatcher_ = std::make_shared<TaskDispatcher<SHMIPCTaskSPtr>>(
      stgInstTaskDispatcherParam, nullptr, getThreadForAsyncTask,
      handleAsyncTask);
  stgInstTaskDispatcher_->init();

  return ret;
}

void StgEngImpl::initScheduleTaskBundle() {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxScheduleTaskBundle_);

    scheduleTaskBundle_->emplace_back(std::make_shared<ScheduleTask>(
        "reloadAcctInfoCache",
        [this]() {
          acctInfoCache_->reload();
          return true;
        },
        ExecAtStartup::True, MilliSecInterval(5000)));

    scheduleTaskBundle_->emplace_back(std::make_shared<ScheduleTask>(
        "sendStgReg",
        [this]() {
          sendStgReg();
          return true;
        },
        ExecAtStartup::False, MilliSecInterval(5000)));

    const auto milliSecIntervalOfSyncTask =
        getConfig()["milliSecIntervalOfSyncTask"].as<std::uint32_t>();
    scheduleTaskBundle_->emplace_back(std::make_shared<ScheduleTask>(
        "syncTask",
        [this]() {
          handleSyncTaskGroup();
          return true;
        },
        ExecAtStartup::False, milliSecIntervalOfSyncTask));
  }
}

int StgEngImpl::doRun() {
  if (stgInstTaskHandler_ == nullptr) {
    LOG_E(
        "[{}] Start failed because of "
        "StgInstTaskHandler is null, please install first.",
        appName_);
    return SCODE_STG_INST_TASK_HANDLER_NOT_INSTALL;
  }

  getDBEng()->start();

  if (auto ret = tblMonitorOfSymbolInfo_->start(); ret != 0) {
    LOG_E("[{}] Start failed.", appName_);
    return ret;
  }

  if (auto ret = tblMonitorOfStgInstInfo_->start(); ret != 0) {
    LOG_E("[{}] Start failed.", appName_);
    return ret;
  }

  topicMgr_->start();

  if (const auto [ret, stgInstInfo] =
          tblMonitorOfStgInstInfo_->getStgInstInfo(1);
      ret != 0) {
    LOG_E("[{}] The stg must have at least one instance with an id of 1.",
          appName_);
    return SCODE_STG_MUST_HAVE_STG_INST_1;
  }

  if (const auto [ret, stgInstInfo] =
          tblMonitorOfStgInstInfo_->getStgInstInfo(0);
      stgInstInfo != nullptr) {
    LOG_E("[{}] The id of the stg instance must start from 1", appName_);
    return SCODE_STG_INST_ID_MUST_START_FROM_1;
  }

  stgInstTaskDispatcher_->start();
  shmCliOfTDSrv_->start();
  shmCliOfRiskMgr_->start();
  shmCliOfWebSrv_->start();

  resetBarrierOfStgStartSignal();
  sendStgStartSignal();
  getBarrierOfStgStartSignal()->get_future().wait();
  sendStgInstStartSignal();

  sendStgReg();

  if (const auto ret = scheduleTaskBundleExecutor_->start(); ret != 0) {
    LOG_E("Start scheduler of multi task failed.");
    return ret;
  }

  return 0;
}

void StgEngImpl::sendStgStartSignal() {
  const StgInstId stgInstId = 1;
  auto asynTask = MakeStgSignal(MSG_ID_ON_STG_START, stgInstId);
  stgInstTaskDispatcher_->dispatch(asynTask);
}

void StgEngImpl::sendStgInstStartSignal() {
  const auto stgInstIdGroup = getTBLMonitorOfStgInstInfo()->getStgInstIdGroup();
  for (const auto stgInstId : stgInstIdGroup) {
    auto asynTask = MakeStgSignal(MSG_ID_ON_STG_INST_START, stgInstId);
    stgInstTaskDispatcher_->dispatch(asynTask);
  }
}

void StgEngImpl::sendStgReg() {
  shmCliOfTDSrv_->asyncSendReqWithZeroCopy(
      [&](void* shmBufOfReq) {
        auto stgReg = static_cast<StgReg*>(shmBufOfReq);
      },
      MSG_ID_ON_STG_REG, sizeof(StgReg));

  shmCliOfRiskMgr_->asyncSendReqWithZeroCopy(
      [&](void* shmBufOfReq) {
        auto stgReg = static_cast<StgReg*>(shmBufOfReq);
      },
      MSG_ID_ON_STG_REG, sizeof(StgReg));
}

void StgEngImpl::cacheSyncTaskGroup(MsgId msgId, const std::any& task,
                                    SyncToRiskMgr syncToRiskMgr,
                                    SyncToDB syncToDB) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    syncTaskGroup_.emplace_back(
        std::make_shared<SyncTask>(msgId, task, syncToRiskMgr, syncToDB));
  }
}

void StgEngImpl::handleSyncTaskGroup() {
  std::vector<SyncTaskSPtr> syncTaskGroup;
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    std::swap(syncTaskGroup, syncTaskGroup_);
  }

  if (syncTaskGroup.size() > 100) {
    LOG_W("Too many unprocessed task of sync. [num = {}]",
          syncTaskGroup.size());
  }

  for (const auto& rec : syncTaskGroup) {
    if (rec->syncToRiskMgr_ == SyncToRiskMgr::False) continue;
    if (rec->msgId_ == MSG_ID_ON_ORDER || rec->msgId_ == MSG_ID_ON_ORDER_RET ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER_RET) {
      const auto orderInfo = std::any_cast<OrderInfoSPtr>(rec->task_);
      shmCliOfRiskMgr_->asyncSendMsgWithZeroCopy(
          [&](void* shmBufOfReq) {
            InitMsgBody(shmBufOfReq, *orderInfo);
            LOG_I("Send order info to risk mgr. {}",
                  static_cast<OrderInfo*>(shmBufOfReq)->toShortStr());
          },
          rec->msgId_, sizeof(OrderInfo));
    } else {
      LOG_W("Unhandled task of sync to riskmgr. {} - {}", rec->msgId_,
            GetMsgName(rec->msgId_))
    }
  }

  for (const auto& rec : syncTaskGroup) {
    if (rec->syncToDB_ == SyncToDB::False) continue;
    if (rec->msgId_ == MSG_ID_ON_ORDER || rec->msgId_ == MSG_ID_ON_ORDER_RET ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER_RET) {
      const auto orderInfo = std::any_cast<OrderInfoSPtr>(rec->task_);
      const auto identity = GET_RAND_STR();
      const auto sql = orderInfo->getSqlOfUSPOrderInfoUpdate();
      const auto [ret, execRet] = getDBEng()->asyncExec(identity, sql);
      if (ret != 0) {
        LOG_W("Sync order info to db failed. [{}]", sql);
      }

    } else {
      LOG_W("Unhandled task of sync to db. {} - {}", rec->msgId_,
            GetMsgName(rec->msgId_))
    }
  }
}

void StgEngImpl::doExit(const boost::system::error_code* ec, int signalNum) {
  scheduleTaskBundleExecutor_->stop();
  shmCliOfWebSrv_->stop();
  shmCliOfRiskMgr_->stop();
  shmCliOfTDSrv_->stop();
  stgInstTaskDispatcher_->stop();
  topicMgr_->stop();
  tblMonitorOfSymbolInfo_->stop();
  tblMonitorOfStgInstInfo_->stop();
  tdEngConnpool_->uninit();
  getDBEng()->stop();
}

std::tuple<int, OrderId> StgEngImpl::order(const StgInstInfoSPtr& stgInstInfo,
                                           AcctId acctId, MarketCode marketCode,
                                           const std::string& symbolCode,
                                           Side side, PosSide posSide,
                                           Decimal orderPrice,
                                           Decimal orderSize, AlgoId algoId,
                                           const SimedTDInfoSPtr& simedTDInfo) {
  std::string simedTDInfoInJsonFmt = "";
  if (simedTDInfo != nullptr) {
    simedTDInfoInJsonFmt = ConvertSimedTDInfoToJsonFmt(simedTDInfo);
  }
  if (simedTDInfoInJsonFmt.size() > MAX_SIMED_TD_INFO - 1) {
    return {SCODE_STG_INVALID_SIMED_TD_INFO_SIZE, 0};
  }

  auto orderInfo =
      MakeOrderInfo(stgInstInfo, acctId, marketCode, symbolCode, side, posSide,
                    orderPrice, orderSize, algoId, simedTDInfoInJsonFmt);

  return order(orderInfo);
}

std::tuple<int, OrderId> StgEngImpl::order(OrderInfoSPtr& orderInfo) {
  if (orderInfo->marketCode_ == MarketCode::Others) {
    int retOfGetMarketCodeAndSymbolType = 0;
    std::tie(retOfGetMarketCodeAndSymbolType, orderInfo->marketCode_,
             orderInfo->symbolType_) =
        acctInfoCache_->getMarketCodeAndSymbolType(orderInfo->acctId_);
    if (retOfGetMarketCodeAndSymbolType != 0) {
      orderInfo->orderStatus_ = OrderStatus::Failed;
      orderInfo->statusCode_ = retOfGetMarketCodeAndSymbolType;
      LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
            GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
      return {retOfGetMarketCodeAndSymbolType, 0};
    }
  }

  if (orderInfo->marketCode_ == MarketCode::Others) {
    orderInfo->orderStatus_ = OrderStatus::Failed;
    orderInfo->statusCode_ = SCODE_STG_INVALID_MARKET_CODE;
    LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
          GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
    return {orderInfo->statusCode_, 0};
  }

  const auto [retOfGetSym, recSymbolInfo] =
      getTBLMonitorOfSymbolInfo()->getRecSymbolInfoBySymbolCode(
          GetMarketName(orderInfo->marketCode_), orderInfo->symbolCode_);
  if (retOfGetSym != 0) {
    orderInfo->orderStatus_ = OrderStatus::Failed;
    orderInfo->statusCode_ = retOfGetSym;
    LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
          GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
    return {retOfGetSym, 0};
  }

  if (orderInfo->symbolType_ == SymbolType::Others) {
    const auto symbolTypeVal =
        magic_enum::enum_cast<SymbolType>(recSymbolInfo->symbolType);
    if (symbolTypeVal.has_value()) {
      orderInfo->symbolType_ = symbolTypeVal.value();
    } else {
      orderInfo->orderStatus_ = OrderStatus::Failed;
      orderInfo->statusCode_ = SCODE_STG_INVALID_SYMBOL_TYPE_IN_DB;
      LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
            GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
      return {orderInfo->statusCode_, 0};
    }
  }

  const auto [retOfGetStgInst, stgInstInfo] =
      tblMonitorOfStgInstInfo_->getStgInstInfo(orderInfo->stgInstId_);
  if (retOfGetStgInst != 0) {
    orderInfo->orderStatus_ = OrderStatus::Failed;
    orderInfo->statusCode_ = retOfGetStgInst;
    LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
          GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
    return {retOfGetStgInst, 0};
  }

  if (orderInfo->symbolType_ == SymbolType::CN_MainBoard ||
      orderInfo->symbolType_ == SymbolType::CN_SecondBoard ||
      orderInfo->symbolType_ == SymbolType::CN_StartupBoard ||
      orderInfo->symbolType_ == SymbolType::CN_TechBoard ||
      orderInfo->symbolType_ == SymbolType::Spot) {
    orderInfo->posSide_ = PosSide::Both;
  }

  if (orderInfo->symbolType_ == SymbolType::CN_MainBoard ||
      orderInfo->symbolType_ == SymbolType::CN_SecondBoard ||
      orderInfo->symbolType_ == SymbolType::CN_StartupBoard ||
      orderInfo->symbolType_ == SymbolType::CN_TechBoard) {
    strncpy(orderInfo->feeCurrency_, CN_OFFICIAL_CURRENCY.c_str(),
            sizeof(orderInfo->feeCurrency_) - 1);
  }

  orderInfo->orderId_ = GET_RAND_INT();
  orderInfo->parValue_ = recSymbolInfo->parValue;
  strncpy(orderInfo->exchSymbolCode_, recSymbolInfo->exchSymbolCode.c_str(),
          sizeof(orderInfo->exchSymbolCode_) - 1);
  orderInfo->orderStatus_ = OrderStatus::Created;

  if (const auto ret = getOrdMgr()->add(orderInfo, DeepClone::True); ret != 0) {
    orderInfo->orderStatus_ = OrderStatus::Failed;
    orderInfo->statusCode_ = ret;
    LOG_W("[{}] Order failed. [{} - {}] {}", appName_, orderInfo->statusCode_,
          GetStatusMsg(orderInfo->statusCode_), orderInfo->toShortStr());
    return {ret, 0};
  }

#ifdef PERF_TEST
  EXEC_PERF_TEST("Order", orderInfo->orderTime_, 100, 10);
#endif

  shmCliOfTDSrv_->asyncSendMsgWithZeroCopy(
      [&](void* shmBufOfReq) {
        InitMsgBody(shmBufOfReq, *orderInfo);
#ifndef OPT_LOG
        LOG_I("Send order {}",
              static_cast<OrderInfo*>(shmBufOfReq)->toShortStr());
#endif
      },
      MSG_ID_ON_ORDER, sizeof(OrderInfo));

  cacheSyncTaskGroup(MSG_ID_ON_ORDER, orderInfo, SyncToRiskMgr::True,
                     SyncToDB::True);

#ifdef PERF_TEST
  EXEC_PERF_TEST("Order", orderInfo->orderTime_, 100, 10);
#endif
  return {0, orderInfo->orderId_};
}

int StgEngImpl::cancelOrder(OrderId orderId) {
  const auto [statusCode, orderInfo] =
      getOrdMgr()->getOrderInfo(orderId, DeepClone::True);
  if (statusCode != 0) {
    LOG_W("[{}] Cancel order {} failed. [{} - {}]", appName_, orderId,
          statusCode, GetStatusMsg(statusCode));
    return statusCode;
  }

  shmCliOfTDSrv_->asyncSendMsgWithZeroCopy(
      [&](void* shmBufOfReq) {
        InitMsgBody(shmBufOfReq, *orderInfo);
        LOG_I("Send cancel order {}",
              static_cast<OrderInfo*>(shmBufOfReq)->toShortStr());
      },
      MSG_ID_ON_CANCEL_ORDER, sizeof(OrderInfo));

  cacheSyncTaskGroup(MSG_ID_ON_CANCEL_ORDER, orderInfo, SyncToRiskMgr::True,
                     SyncToDB::False);

  return 0;
}

int StgEngImpl::sub(StgInstId subscriber, const std::string& topic) {
  const auto statusCode = subMgr_->sub(subscriber, topic);
  const auto s = fmt::format("{}-{}", getAppName(), subscriber);
  topicMgr_->add(s, topic);
  return statusCode;
}

int StgEngImpl::unSub(StgInstId subscriber, const std::string& topic) {
  const auto statusCode = subMgr_->unSub(subscriber, topic);
  const auto s = fmt::format("{}-{}", getAppName(), subscriber);
  topicMgr_->remove(s, topic);
  return statusCode;
}

std::tuple<int, std::string> StgEngImpl::queryHisMDBetween2Ts(
    const std::string& topic, std::uint64_t tsBegin, std::uint64_t tsEnd) {
  const auto [statusCode, marketDataCond] = GetMarketDataCondFromTopic(topic);
  if (statusCode != 0) return {statusCode, ""};
  return queryHisMDBetween2Ts(
      marketDataCond->marketCode_, marketDataCond->symbolType_,
      marketDataCond->symbolCode_, marketDataCond->mdType_, tsBegin, tsEnd,
      marketDataCond->ext_);
}

std::tuple<int, std::string> StgEngImpl::queryHisMDBetween2Ts(
    MarketCode marketCode, SymbolType symbolType, const std::string& symbolCode,
    MDType mdType, std::uint64_t tsBegin, std::uint64_t tsEnd,
    const std::string& ext) {
  std::string addr;
  const auto prefix =
      fmt::format(prefixOfQueryHisMDBetween,
                  getConfig()["webSrv"].as<std::string>("localhost"));
  if (mdType == MDType::Candle) {
    if (ext.empty()) {
      addr = fmt::format("{}/{}/{}/{}/{}?tsBegin={}&tsEnd={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), tsBegin, tsEnd);
    } else {
      addr = fmt::format("{}/{}/{}/{}/{}?tsBegin={}&tsEnd={}&freq={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), tsBegin, tsEnd, ext);
    }
  } else {
    addr = fmt::format("{}/{}/{}/{}/{}?tsBegin={}&tsEnd={}", prefix,
                       GetMarketName(marketCode),
                       magic_enum::enum_name(symbolType), symbolCode,
                       magic_enum::enum_name(mdType), tsBegin, tsEnd);
  }

  const auto timeoutOfQueryHisMD =
      getConfig()["timeoutOfQueryHisMD"].as<std::uint32_t>(60000);
  cpr::Response rsp =
      cpr::Get(cpr::Url{addr}, cpr::Timeout(timeoutOfQueryHisMD));
  if (rsp.status_code != cpr::status::HTTP_OK) {
    const auto statusMsg =
        fmt::format("Query his market data between 2 ts failed. [{}:{}] {} {}",
                    rsp.status_code, rsp.reason, rsp.text, rsp.url.str());
    LOG_W(statusMsg);
    return {SCODE_STG_SEND_HTTP_REQ_TO_QUERY_HIS_MD_FAILED, ""};
  } else {
    LOG_I("Send http req success. {}", addr);
  }

  return {0, rsp.text};
}

std::tuple<int, std::string> StgEngImpl::querySpecificNumOfHisMDBeforeTs(
    const std::string& topic, std::uint64_t ts, int num) {
  const auto [statusCode, marketDataCond] = GetMarketDataCondFromTopic(topic);
  if (statusCode != 0) return {statusCode, ""};
  return querySpecificNumOfHisMDBeforeTs(
      marketDataCond->marketCode_, marketDataCond->symbolType_,
      marketDataCond->symbolCode_, marketDataCond->mdType_, ts, num,
      marketDataCond->ext_);
}

std::tuple<int, std::string> StgEngImpl::querySpecificNumOfHisMDBeforeTs(
    MarketCode marketCode, SymbolType symbolType, const std::string& symbolCode,
    MDType mdType, std::uint64_t ts, int num, const std::string& ext) {
  std::string addr;
  const auto prefix =
      fmt::format(prefixOfQueryHisMDBefore,
                  getConfig()["webSrv"].as<std::string>("localhost"));
  if (mdType == MDType::Candle) {
    if (ext.empty()) {
      addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), ts, num);
    } else {
      addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}&freq={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), ts, num, ext);
    }
  } else {
    addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}", prefix,
                       GetMarketName(marketCode),
                       magic_enum::enum_name(symbolType), symbolCode,
                       magic_enum::enum_name(mdType), ts, num);
  }

  const auto timeoutOfQueryHisMD =
      getConfig()["timeoutOfQueryHisMD"].as<std::uint32_t>(60000);
  cpr::Response rsp =
      cpr::Get(cpr::Url{addr}, cpr::Timeout(timeoutOfQueryHisMD));
  if (rsp.status_code != cpr::status::HTTP_OK) {
    const auto statusMsg = fmt::format(
        "Query specific num of his market data before ts failed. [{}:{}] {} {}",
        rsp.status_code, rsp.reason, rsp.text, rsp.url.str());
    LOG_W(statusMsg);
    return {SCODE_STG_SEND_HTTP_REQ_TO_QUERY_HIS_MD_FAILED, ""};
  } else {
    LOG_I("Send http req success. {}", addr);
  }

  return {0, rsp.text};
}

std::tuple<int, std::string> StgEngImpl::querySpecificNumOfHisMDAfterTs(
    const std::string& topic, std::uint64_t ts, int num) {
  const auto [statusCode, marketDataCond] = GetMarketDataCondFromTopic(topic);
  if (statusCode != 0) return {statusCode, ""};
  return querySpecificNumOfHisMDAfterTs(
      marketDataCond->marketCode_, marketDataCond->symbolType_,
      marketDataCond->symbolCode_, marketDataCond->mdType_, ts, num,
      marketDataCond->ext_);
}

std::tuple<int, std::string> StgEngImpl::querySpecificNumOfHisMDAfterTs(
    MarketCode marketCode, SymbolType symbolType, const std::string& symbolCode,
    MDType mdType, std::uint64_t ts, int num, const std::string& ext) {
  std::string addr;
  const auto prefix =
      fmt::format(prefixOfQueryHisMDAfter,
                  getConfig()["webSrv"].as<std::string>("localhost"));
  if (mdType == MDType::Candle) {
    if (ext.empty()) {
      addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), ts, num);
    } else {
      addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}&freq={}", prefix,
                         GetMarketName(marketCode),
                         magic_enum::enum_name(symbolType), symbolCode,
                         magic_enum::enum_name(mdType), ts, num, ext);
    }
  } else {
    addr = fmt::format("{}/{}/{}/{}/{}?ts={}&num={}", prefix,
                       GetMarketName(marketCode),
                       magic_enum::enum_name(symbolType), symbolCode,
                       magic_enum::enum_name(mdType), ts, num);
  }

  const auto timeoutOfQueryHisMD =
      getConfig()["timeoutOfQueryHisMD"].as<std::uint32_t>(60000);
  cpr::Response rsp =
      cpr::Get(cpr::Url{addr}, cpr::Timeout(timeoutOfQueryHisMD));
  if (rsp.status_code != cpr::status::HTTP_OK) {
    const auto statusMsg = fmt::format(
        "Query specific num of his market data after ts failed. [{}:{}] {} {}",
        rsp.status_code, rsp.reason, rsp.text, rsp.url.str());
    LOG_W(statusMsg);
    return {SCODE_STG_SEND_HTTP_REQ_TO_QUERY_HIS_MD_FAILED, ""};
  } else {
    LOG_I("Send http req success. {}", addr);
  }

  return {0, rsp.text};
}

void StgEngImpl::installStgInstTimer(StgInstId stgInstId,
                                     const std::string& timerName,
                                     ExecAtStartup execAtStartUp,
                                     std::uint32_t milliSecInterval,
                                     std::uint64_t maxExecTimes) {
  LOG_I(
      "Install a timer named {} "
      "with a trigger interval of {} ms and a max exec times of 1.",
      timerName, milliSecInterval, maxExecTimes);

  const auto callback = [stgInstId, timerName, this]() {
    auto asynTask =
        MakeStgSignal(MSG_ID_ON_STG_INST_TIMER, stgInstId, timerName);
    stgInstTaskDispatcher_->dispatch(asynTask);
    return true;
  };

  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxScheduleTaskBundle_);
    scheduleTaskBundle_->emplace_back(std::make_shared<ScheduleTask>(
        timerName, callback, execAtStartUp, milliSecInterval, maxExecTimes));
    if (scheduleTaskBundle_->size() % 100 == 0) {
      LOG_W("[{}] Size of scheduleTaskBundle is {}", getAppName(),
            scheduleTaskBundle_->size());
    }
  }
}

bool StgEngImpl::saveStgPrivateData(StgInstId stgInstId,
                                    const std::string& jsonStr) {
  try {
    if (!boost::filesystem::exists(rootDirOfStgPrivateData_)) {
      boost::filesystem::create_directories(rootDirOfStgPrivateData_);
    }
  } catch (const std::exception& e) {
    LOG_W(
        "Save stg private data failed "
        "because of create directories {} failed. {}",
        rootDirOfStgPrivateData_, e.what());
    return false;
  }
  const auto filename =
      fmt::format("{}/{}.dat", rootDirOfStgPrivateData_, stgInstId);
  bool ret = OverwriteStrToFile(filename, jsonStr);
  return ret;
}

std::string StgEngImpl::loadStgPrivateData(StgInstId stgInstId) {
  const auto filename =
      fmt::format("{}/{}.dat", rootDirOfStgPrivateData_, stgInstId);
  const auto filecont = LoadFileContToStr(filename);
  return filecont;
}

void StgEngImpl::saveToDB(const PnlSPtr& pnl) {
  const auto identity = GET_RAND_STR();
  const auto sql = pnl->getSqlOfInsert();
  const auto [ret, execRet] = dbEng_->asyncExec(identity, sql);
  if (ret != 0) {
    LOG_W("Insert pnl to db failed. [{}]", sql);
  }
}

std::tuple<int, OrderInfoSPtr> StgEngImpl::getOrderInfo(OrderId orderId) const {
  return ordMgr_->getOrderInfo(orderId, DeepClone::True, LockFunc::True);
}

ScheduleTaskBundleSPtr StgEngImpl::getScheduleTaskBundle() {
  auto ret = std::make_shared<ScheduleTaskBundle>();
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxScheduleTaskBundle_);
    std::ext::erase_if(*scheduleTaskBundle_, [this](const auto& scheduleTask) {
      if (scheduleTask->execTimes_ == scheduleTask->maxExecTimes_) {
        const auto statusMsg = fmt::format(
            "[{}] scheduleTask {} finished. [execTimes = {}; maxExecTimes {}]",
            getAppName(), scheduleTask->scheduleTaskName_,
            scheduleTask->execTimes_, scheduleTask->maxExecTimes_);
        LOG_I(statusMsg);
        return true;
      } else {
        return false;
      }
    });
    ret->assign(std::begin(*scheduleTaskBundle_),
                std::end(*scheduleTaskBundle_));
  }
  return ret;
};

}  // namespace bq::stg
