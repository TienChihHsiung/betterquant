/*!
 * \file TDSvcOfCN.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "TDSvcOfCN.hpp"

#include "AssetsMgr.hpp"
#include "Config.hpp"
#include "OrdMgr.hpp"
#include "SHMIPC.hpp"
#include "SimedOrderInfoHandler.hpp"
#include "TDGateway.hpp"
#include "TDSrvTaskHandler.hpp"
#include "TDSvcUtil.hpp"
#include "db/DBE.hpp"
#include "db/TBLMonitor.hpp"
#include "db/TBLMonitorOfSymbolInfo.hpp"
#include "db/TBLOrderInfo.hpp"
#include "db/TBLRecSetMaker.hpp"
#include "def/BQDef.hpp"
#include "def/DataStruOfOthers.hpp"
#include "def/Def.hpp"
#include "def/SyncTask.hpp"
#include "util/ClientId2OrderId.hpp"
#include "util/ExceedFlowCtrlHandler.hpp"
#include "util/ExternalStatusCodeCache.hpp"
#include "util/FeeInfoCache.hpp"
#include "util/FlowCtrlSvc.hpp"
#include "util/Literal.hpp"
#include "util/ScheduleTaskBundle.hpp"
#include "util/Scheduler.hpp"
#include "util/SignalHandler.hpp"
#include "util/String.hpp"
#include "util/TaskDispatcher.hpp"
#include "util/TrdSymbolCache.hpp"

namespace bq::td::svc {

int TDSvcOfCN::prepareInit() {
  syncTaskGroup_ = std::make_shared<SyncTaskGroup>();
  syncTaskGroup_->reserve(1024);

  auto retOfConfInit = Config::get_mutable_instance().init(configFilename_);
  if (retOfConfInit != 0) {
    const auto statusMsg = fmt::format("Prepare init failed.");
    std::cerr << statusMsg << std::endl;
    return retOfConfInit;
  }

  const auto retOfLoggerInit = InitLogger(CONFIG);
  if (retOfLoggerInit != 0) {
    const auto statusMsg = fmt::format("Prepare init failed.");
    std::cerr << statusMsg << std::endl;
    return retOfLoggerInit;
  }

  return 0;
}

int TDSvcOfCN::doInit() {
  acctId_ = CONFIG["acctId"].as<AcctId>();

  const auto apiInfo = Config::get_const_instance().getApiInfo();
  appName_ = fmt::format("{}-{}-INSTANCE-{}", TOPIC_PREFIX_OF_TRADE_DATA,
                         apiInfo->apiName_, getAcctId());

  if (const auto ret = initDBEng(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  initTBLMonitorOfSymbolInfo();
  initTrdSymbolCache();
  initExternalStatusCodeCache();

  if (const auto ret = queryMaxNoUsedToCalcPos(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  initAssetsMgr();
  initOrdMgr();

  const auto pathOfClientId2OrderId =
      CONFIG["pathOfClientId2OrderId"].as<std::string>();
  if (boost::filesystem::exists(pathOfClientId2OrderId) == false) {
    boost::filesystem::create_directories(pathOfClientId2OrderId);
  }
  clientId2OrderId_ = std::make_shared<ClientId2OrderId>();

  feeInfoCache_ = std::make_shared<FeeInfoCache>(getDBEng());
  feeInfoCache_->load(getAcctId());

#ifdef SIMED_MODE
  simedOrderInfoHandler_ = std::make_shared<SimedOrderInfoHandler>(this);
#else
  assert(tdGateway_ != nullptr && "tdGateway_ != nullptr");
  if (const auto statusCode = tdGateway_->init(); statusCode != 0) {
    LOG_E("Do init failed. ");
    return statusCode;
  }
#endif

  tdSrvTaskHandler_ = std::make_shared<TDSrvTaskHandler>(this);
  initTDSrvTaskDispatcher();
  initSHMCliOfTDSrv();
  initSHMCliOfRiskMgr();

  flowCtrlSvc_ = std::make_shared<FlowCtrlSvc>(CONFIG);
  exceedFlowCtrlHandler_ =
      std::make_shared<ExceedFlowCtrlHandler>([this](auto& asyncTask) {
        getTDSrvTaskDispatcher()->dispatch(asyncTask);
      });

  scheduleTaskBundle_ = std::make_shared<ScheduleTaskBundle>();
  initScheduleTaskBundle();
  scheduleTaskBundleExecutor_ = std::make_shared<Scheduler>(
      appName_, [this]() { ExecScheduleTaskBundle(getScheduleTaskBundle()); },
      1 * 1000);

  return 0;
}

int TDSvcOfCN::initDBEng() {
  const auto dbEngParam = SetParam(db::DEFAULT_DB_ENG_PARAM,
                                   CONFIG["dbEngParam"].as<std::string>());
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

void TDSvcOfCN::initTBLMonitorOfSymbolInfo() {
  const auto sql = fmt::format("SELECT * FROM {};", TBLSymbolInfo::TableName);

  tblMonitorOfSymbolInfo_ = std::make_shared<db::TBLMonitorOfSymbolInfo>(
      getDBEng(), UINT32_MAX, sql, nullptr, db::EnableMonitoring::False);
}

void TDSvcOfCN::initTrdSymbolCache() {
  trdSymbolCache_ = std::make_shared<TrdSymbolCache>(getDBEng());
  trdSymbolCache_->load("", "", getAcctId());
}

void TDSvcOfCN::initExternalStatusCodeCache() {
  externalStatusCodeCache_ =
      std::make_shared<ExternalStatusCodeCache>(getDBEng());
  externalStatusCodeCache_->load("", 0);
}

int TDSvcOfCN::queryMaxNoUsedToCalcPos() {
  const auto sql = fmt::format(
      "SELECT * FROM {} WHERE acctId = {} "
      "ORDER BY noUsedToCalcPos DESC LIMIT 1;",
      TBLOrderInfo::TableName, getAcctId());
  const auto [ret, tblRecSet] =
      db::TBLRecSetMaker<TBLOrderInfo>::ExecSql(getDBEng(), sql);
  if (ret != 0) {
    LOG_W("Query maxNoUsedToCalcPos from db failed. {}", sql);
    return ret;
  }

  if (!tblRecSet->empty()) {
    const auto& tblRec = std::begin(*tblRecSet);
    const auto& orderInfo = tblRec->second;
    maxNoUsedToCalcPos_ = orderInfo->getRecWithAllFields()->noUsedToCalcPos;
  }
  LOG_I("Query maxNoUsedToCalcPos success. [maxNoUsedToCalcPos = {}]",
        maxNoUsedToCalcPos_);

  return 0;
}

void TDSvcOfCN::initAssetsMgr() {
  assetsMgr_ = std::make_shared<AssetsMgr>();
  const auto sql = fmt::format("SELECT * FROM `assetInfo` WHERE `acctId` = {};",
                               getAcctId());
  getAssetsMgr()->init(CONFIG, getDBEng(), sql);
}

void TDSvcOfCN::initOrdMgr() {
  const auto filled = magic_enum::enum_integer(OrderStatus::Filled);
  ordMgr_ = std::make_shared<OrdMgr>();
  const auto sql = fmt::format(
      "SELECT * FROM `orderInfo` WHERE `orderStatus` < {} AND `acctId` = {}; ",
      filled, getAcctId());
  getOrdMgr()->init(CONFIG, getDBEng(), sql);
}

int TDSvcOfCN::initTDSrvTaskDispatcher() {
  const auto tdSrvTaskDispatcherParamInStrFmt =
      SetParam(DEFAULT_TASK_DISPATCHER_PARAM,
               CONFIG["tdSrvTaskDispatcherParam"].as<std::string>());
  const auto [ret, tdSrvTaskDispatcherParam] =
      MakeTaskDispatcherParam(tdSrvTaskDispatcherParamInStrFmt);
  if (ret != 0) {
    LOG_E("[{}] Init taskdispatcher failed. {}", appName_,
          tdSrvTaskDispatcherParamInStrFmt);
    return ret;
  }

  const auto getThreadForAsyncTask = [](const auto& asyncTask,
                                        auto taskSpecificThreadPoolSize) {
    const auto shmHeader =
        static_cast<const SHMHeader*>(asyncTask->task_->data_);
    switch (shmHeader->msgId_) {
      case MSG_ID_ON_ORDER:
      case MSG_ID_ON_CANCEL_ORDER:
        return ThreadNo(0);
      default:
        return ThreadNo(1);
    }
  };

  const auto handleAsyncTask = [this](auto& asyncTask) {
    tdSrvTaskHandler_->handleAsyncTask(asyncTask);
  };

  tdSrvTaskDispatcher_ = std::make_shared<TaskDispatcher<SHMIPCTaskSPtr>>(
      tdSrvTaskDispatcherParam, nullptr, getThreadForAsyncTask,
      handleAsyncTask);
  tdSrvTaskDispatcher_->init();

  return ret;
}

void TDSvcOfCN::initSHMCliOfTDSrv() {
  const auto tdSrvChannel = CONFIG["tdSrvChannel"].as<std::string>();
  const auto addr =
      fmt::format("{}{}{}", appName_, SEP_OF_SHM_SVC, tdSrvChannel);

  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
    const auto task = std::make_shared<SHMIPCTask>(shmBuf, shmBufLen);
    auto asyncTask = std::make_shared<SHMIPCAsyncTask>(task);
    tdSrvTaskDispatcher_->dispatch(asyncTask);
  };

  shmCliOfTDSrv_ = std::make_shared<SHMCli>(addr, onSHMDataRecv);
  shmCliOfTDSrv_->setClientChannel(acctId_);
}

void TDSvcOfCN::initSHMCliOfRiskMgr() {
  const auto riskMgrChannel = CONFIG["riskMgrChannel"].as<std::string>();
  const auto addr =
      fmt::format("{}{}{}", appName_, SEP_OF_SHM_SVC, riskMgrChannel);

  const auto onSHMDataRecv = [this](const void* shmBuf, std::size_t shmBufLen) {
  };

  shmCliOfRiskMgr_ = std::make_shared<SHMCli>(addr, onSHMDataRecv);
  shmCliOfRiskMgr_->setClientChannel(acctId_);
}

void TDSvcOfCN::beforeInitScheduleTaskBundle() {
#ifndef SIMED_MODE
  const auto secIntervalOfSyncAssetsSnapshot =
      CONFIG["secIntervalOfSyncAssetsSnapshot"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "syncAssetsSnapshot",
      [this]() {
        auto asyncTask = MakeTDSrvSignal(MSG_ID_SYNC_ASSETS_SNAPSHOT);
        getTDSrvTaskDispatcher()->dispatch(asyncTask);
        return true;
      },
      ExecAtStartup::True, secIntervalOfSyncAssetsSnapshot * 1000));

  const auto secAgoTheOrderNeedToBeSynced =
      CONFIG["secAgoTheOrderNeedToBeSynced"].as<std::uint32_t>();
  const auto secIntervalOfSyncUnclosedOrderInfo =
      CONFIG["secIntervalOfSyncUnclosedOrderInfo"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "syncUnclosedOrderInfo",
      [this, secAgoTheOrderNeedToBeSynced]() {
        const auto orderInfoGroup =
            getOrdMgr()->getOrderInfoGroup(secAgoTheOrderNeedToBeSynced);
        for (const auto& orderInfo : orderInfoGroup) {
          auto asyncTask =
              MakeTDSrvSignal(MSG_ID_SYNC_UNCLOSED_ORDER_INFO, orderInfo);
          getTDSrvTaskDispatcher()->dispatch(asyncTask);
        }
        return true;
      },
      ExecAtStartup::True, secIntervalOfSyncUnclosedOrderInfo * 1000));
#endif

  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "exceedFlowCtrlHandler",
      [this]() {
        exceedFlowCtrlHandler_->handleExceedFlowCtrlTask();
        return true;
      },
      ExecAtStartup::False, MilliSecInterval(1000), UINT64_MAX,
      WriteLog::False));

  const auto secIntervalOfReloadExternalStatusCode =
      CONFIG["secIntervalOfReloadExternalStatusCode"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "reloadExternalStatusCode",
      [this]() {
        externalStatusCodeCache_->reload("", 0);
        return true;
      },
      ExecAtStartup::False, secIntervalOfReloadExternalStatusCode * 1000));

  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "sendTDGWReg",
      [this]() {
        sendTDGWReg();
        return true;
      },
      ExecAtStartup::False, MilliSecInterval(5000)));

  const auto milliSecIntervalOfSyncTask =
      CONFIG["milliSecIntervalOfSyncTask"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "syncTask",
      [this]() {
        handleSyncTaskGroup();
        return true;
      },
      ExecAtStartup::False, milliSecIntervalOfSyncTask));

  const auto secIntervalOfSaveClientId2OrderId =
      CONFIG["secIntervalOfSaveClientId2OrderId"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "saveClientId2OrderId",
      [this]() {
        const auto filename = getFilenameOfClientId2OrderId(getTradingDay());
        if (filename.empty()) return false;
        clientId2OrderId_->save(filename);
        return true;
      },
      ExecAtStartup::False, secIntervalOfSaveClientId2OrderId * 1000));

  const auto secIntervalOfReloadFeeInfo =
      CONFIG["secIntervalOfReloadFeeInfo"].as<std::uint32_t>();
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "reloadFeeInfo",
      [this]() {
        feeInfoCache_->reload(getAcctId());
        return true;
      },
      ExecAtStartup::False, secIntervalOfReloadFeeInfo * 1000));
}

int TDSvcOfCN::doRun() {
  getDBEng()->start();

  if (const auto ret = tblMonitorOfSymbolInfo_->start(); ret != 0) {
    LOG_E("Run failed.");
    return ret;
  }

#ifndef SIMED_MODE
  if (const auto statusCode = tdGateway_->start(); statusCode != 0) {
    LOG_E("Run failed. ");
    return statusCode;
  }
#endif

  tdSrvTaskDispatcher_->start();
  shmCliOfTDSrv_->start();
  shmCliOfRiskMgr_->start();
  sendTDGWReg();

  if (const auto ret = scheduleTaskBundleExecutor_->start(); ret != 0) {
    LOG_E("Start scheduler of multi task failed.");
    return ret;
  }

  return 0;
}

void TDSvcOfCN::sendTDGWReg() {
  shmCliOfTDSrv_->asyncSendReqWithZeroCopy(
      [&](void* shmBufOfReq) {
        auto tdGWReg = static_cast<TDGWReg*>(shmBufOfReq);
      },
      MSG_ID_ON_TDGW_REG, sizeof(TDGWReg));

  shmCliOfRiskMgr_->asyncSendReqWithZeroCopy(
      [&](void* shmBufOfReq) {
        auto tdGWReg = static_cast<TDGWReg*>(shmBufOfReq);
      },
      MSG_ID_ON_TDGW_REG, sizeof(TDGWReg));
}

void TDSvcOfCN::cacheSyncTaskGroup(MsgId msgId, const std::any& task,
                                   SyncToRiskMgr syncToRiskMgr,
                                   SyncToDB syncToDB) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    syncTaskGroup_->emplace_back(
        std::make_shared<SyncTask>(msgId, task, syncToRiskMgr, syncToDB));
  }
}

void TDSvcOfCN::handleSyncTaskGroup() {
  auto syncTaskGroup = std::make_shared<SyncTaskGroup>();
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    syncTaskGroup.swap(syncTaskGroup_);
  }

  if (syncTaskGroup->size() > 100) {
    LOG_W("Too many unprocessed task of sync. [num = {}]",
          syncTaskGroup->size());
  }

  for (const auto& rec : *syncTaskGroup) {
    if (rec->syncToRiskMgr_ == SyncToRiskMgr::False) continue;
    if (rec->msgId_ == MSG_ID_ON_ORDER || rec->msgId_ == MSG_ID_ON_ORDER_RET ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER ||
        rec->msgId_ == MSG_ID_ON_CANCEL_ORDER_RET) {
      const auto orderInfo = std::any_cast<OrderInfoSPtr>(rec->task_);
      shmCliOfRiskMgr_->asyncSendMsgWithZeroCopy(
          [&](void* shmBuf) {
            InitMsgBody(shmBuf, *orderInfo);
            LOG_I("Send order info to risk mgr. {}",
                  static_cast<OrderInfo*>(shmBuf)->toShortStr());
          },
          rec->msgId_, sizeof(OrderInfo));

    } else if (rec->msgId_ == MSG_ID_SYNC_ASSETS) {
      const auto updateInfoOfAssetGroup =
          std::any_cast<UpdateInfoOfAssetGroupSPtr>(rec->task_);
      NotifyAssetInfo(shmCliOfRiskMgr_, getAcctId(), updateInfoOfAssetGroup);

    } else {
      LOG_W("Unhandled task of sync to riskmgr. {} - {}", rec->msgId_,
            GetMsgName(rec->msgId_))
    }
  }

  for (const auto& rec : *syncTaskGroup) {
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

    } else if (rec->msgId_ == MSG_ID_SYNC_ASSETS) {
      const auto updateInfoOfAssetGroup =
          std::any_cast<UpdateInfoOfAssetGroupSPtr>(rec->task_);

      for (const auto& assetInfo :
           *updateInfoOfAssetGroup->assetInfoGroupAdd_) {
        const auto identity = GET_RAND_STR();
        const auto sql = assetInfo->getSqlOfInsert();
        const auto [ret, execRet] = dbEng_->asyncExec(identity, sql);
        if (ret != 0) {
          LOG_W("Insert asset info to db failed. [{}]", sql);
        }
      }

      for (const auto& assetInfo :
           *updateInfoOfAssetGroup->assetInfoGroupDel_) {
        const auto identity = GET_RAND_STR();
        const auto sql = assetInfo->getSqlOfDelete();
        const auto [ret, execRet] = dbEng_->asyncExec(identity, sql);
        if (ret != 0) {
          LOG_W("Del asset info from db failed. [{}]", sql);
        }
      }

      for (const auto& assetInfo :
           *updateInfoOfAssetGroup->assetInfoGroupChg_) {
        const auto identity = GET_RAND_STR();
        const auto sql = assetInfo->getSqlOfUpdate();
        const auto [ret, execRet] = dbEng_->asyncExec(identity, sql);
        if (ret != 0) {
          LOG_W("Update asset info from db failed. [{}]", sql);
        }
      }

    } else {
      LOG_W("Unhandled task of sync to db. {} - {}", rec->msgId_,
            GetMsgName(rec->msgId_))
    }
  }
}

void TDSvcOfCN::doExit(const boost::system::error_code* ec, int signalNum) {
  scheduleTaskBundleExecutor_->stop();
  getAssetsMgr()->syncUpdateInfoOfAssetGroupToDB();
  shmCliOfRiskMgr_->stop();
  shmCliOfTDSrv_->stop();
  tdSrvTaskDispatcher_->stop();
#ifndef SIMED_MODE
  tdGateway_->stop();
#endif
  clientId2OrderId_->save(getFilenameOfClientId2OrderId(getTradingDay()));
  tblMonitorOfSymbolInfo_->stop();
  getDBEng()->stop();
}

std::string TDSvcOfCN::getFilenameOfClientId2OrderId(
    const std::string& tradingDay) const {
  if (tradingDay.empty()) return "";
  const auto pathOfClientId2OrderId =
      CONFIG["pathOfClientId2OrderId"].as<std::string>();
  const auto ret = fmt::format("{}/{}-{}.dat", pathOfClientId2OrderId,
                               tradingDay, getAcctId());
  return ret;
}

}  // namespace bq::td::svc
