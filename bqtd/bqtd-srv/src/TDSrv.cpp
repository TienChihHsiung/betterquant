/*!
 * \file TDSrv.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "TDSrv.hpp"

#include "AssetsMgr.hpp"
#include "ClientChannelGroup.hpp"
#include "Config.hpp"
#include "FlowCtrlRuleMgr.hpp"
#include "OrdMgr.hpp"
#include "PosMgr.hpp"
#include "PosMgrRestorer.hpp"
#include "SHMHeader.hpp"
#include "SHMIPCTask.hpp"
#include "SHMSrv.hpp"
#include "StgEngTaskHandler.hpp"
#include "TDGWTaskHandler.hpp"
#include "TDSrvConst.hpp"
#include "TDSrvRiskPluginMgr.hpp"
#include "db/DBEngConst.hpp"
#include "db/TBLMonitorOfFlowCtrlRule.hpp"
#include "db/TBLMonitorOfSymbolInfo.hpp"
#include "def/BQDef.hpp"
#include "def/ConditionUtil.hpp"
#include "def/Const.hpp"
#include "def/DataStruOfAssets.hpp"
#include "def/DataStruOfOthers.hpp"
#include "def/PosInfo.hpp"
#include "def/SyncTask.hpp"
#include "util/BQUtil.hpp"
#include "util/Literal.hpp"
#include "util/Logger.hpp"
#include "util/ScheduleTaskBundle.hpp"
#include "util/Scheduler.hpp"
#include "util/StdExt.hpp"
#include "util/String.hpp"
#include "util/TaskDispatcher.hpp"

namespace bq::td::srv {

int TDSrv::prepareInit() {
  if (const auto ret = Config::get_mutable_instance().init(configFilename_);
      ret != 0) {
    const auto statusMsg = fmt::format("Prepare init failed.");
    std::cerr << statusMsg << std::endl;
    return ret;
  }

  if (const auto ret = InitLogger(configFilename_); ret != 0) {
    const auto statusMsg =
        fmt::format("Init td srv failed because of init logger failed. {}",
                    configFilename_);
    std::cerr << statusMsg << std::endl;
    return ret;
  }

  return 0;
}

int TDSrv::doInit() {
  if (const auto ret = initDBEng(); ret != 0) {
    LOG_E("Do init failed. ");
    return ret;
  }

  int statusCode = 0;
  std::string statusMsg;
  std::tie(statusCode, statusMsg, conditionFieldGroup_) =
      MakeConditionFieldGroup(ORDER_INFO_OFFLOAD_GRANULARITY);
  if (statusCode != 0) {
    LOG_W("Do init failed. [{}]", statusMsg);
    return statusCode;
  }

  initTBLMonitorOfFlowCtrlRule();

  tdSrvRiskPluginMgr_ = std::make_shared<TDSrvRiskPluginMgr>(this);
  tdSrvRiskPluginMgr_->load();

  posMgrRestorer_ = std::make_shared<PosMgrRestorer>(this);
  if (const auto ret = posMgrRestorer_->exec(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  initTBLMonitorOfSymbolInfo();

  tdGWGroup_ = std::make_shared<ClientChannelGroup>("TDGW");
  stgEngGroup_ = std::make_shared<ClientChannelGroup>("StgEng");

  tdGWTaskHandler_ = std::make_shared<TDGWTaskHandler>(this);
  stgEngTaskHandler_ = std::make_shared<StgEngTaskHandler>(this);

  if (const auto ret = initTDSrvTaskDispatcher(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  initSHMSrv();

  scheduleTaskBundle_ = std::make_shared<ScheduleTaskBundle>();
  initScheduleTaskBundle();
  scheduleTaskBundleExecutor_ = std::make_shared<Scheduler>(
      std::string("TD_SRV"),
      [this]() { ExecScheduleTaskBundle(getScheduleTaskBundle()); }, 1 * 1000);

  return 0;
}

int TDSrv::initDBEng() {
  const auto dbEngParam = SetParam(db::DEFAULT_DB_ENG_PARAM,
                                   CONFIG["dbEngParam"].as<std::string>());
  int retOfMakeDBEng = 0;
  std::tie(retOfMakeDBEng, dbEng_) = db::MakeDBEng(
      dbEngParam, [](db::DBTaskSPtr& dbTask, const StringSPtr& dbExecRet) {
        LOG_D("Exec sql finished. [{}] [exec result = {}]", dbTask->toStr(),
              *dbExecRet);
      });
  if (retOfMakeDBEng != 0) {
    LOG_E("Init dbeng failed. {}", dbEngParam);
    return retOfMakeDBEng;
  }

  if (auto retOfInit = getDBEng()->init(); retOfInit != 0) {
    LOG_E("Init dbeng failed. {}", dbEngParam);
    return retOfInit;
  }

  return 0;
}

void TDSrv::initTBLMonitorOfFlowCtrlRule() {
  const auto secIntervalOfMonitorOfFlowCtrlRule =
      CONFIG["secIntervalOfMonitorOfFlowCtrlRule"].as<std::uint32_t>();

  const auto sql =
      "SELECT * FROM `flowCtrlRule`"
      " WHERE `step` = 'InTDSrv' AND `enable` <> 0; ";

  const auto onTBLFlowCtrlRuleChg = [this](const auto& tblRecSetAdd,
                                           const auto& tblRecSetDel,
                                           const auto& tblRecSetChg) {
    for (const auto tblRec : *tblRecSetAdd) {
      const auto rec = tblRec.second->getRecWithAllFields();
      LOG_I("{}", "On TBLFlowCtrlRuleAdd");
    }
    for (const auto tblRec : *tblRecSetDel) {
      const auto rec = tblRec.second->getRecWithAllFields();
      LOG_I("{}", "On TBLFlowCtrlRuleDel");
    }
    for (const auto tblRec : *tblRecSetChg) {
      const auto rec = tblRec.second->getRecWithAllFields();
      LOG_I("{}", "On TBLFlowCtrlRuleChg");
    }
  };

  tblMonitorOfFlowCtrlRule_ = std::make_shared<db::TBLMonitorOfFlowCtrlRule>(
      getDBEng(), secIntervalOfMonitorOfFlowCtrlRule * 1000, sql,
      onTBLFlowCtrlRuleChg, db::EnableMonitoring::False);
  tblMonitorOfFlowCtrlRule_->start();
}

void TDSrv::initTBLMonitorOfSymbolInfo() {
  const auto milliSecIntervalOfTBLMonitorOfSymbolInfo =
      CONFIG["milliSecIntervalOfTBLMonitorOfSymbolInfo"].as<std::uint32_t>();

  const auto sql = fmt::format("SELECT * FROM {};", TBLSymbolInfo::TableName);

  const auto monitorSymbolTableChanges =
      CONFIG["monitorSymbolTableChanges"].as<bool>(true);
  const auto enableMonitoring = monitorSymbolTableChanges
                                    ? db::EnableMonitoring::True
                                    : db::EnableMonitoring::False;

  tblMonitorOfSymbolInfo_ = std::make_shared<db::TBLMonitorOfSymbolInfo>(
      getDBEng(), milliSecIntervalOfTBLMonitorOfSymbolInfo, sql, nullptr,
      enableMonitoring);
}

void TDSrv::initFlowCtrlRuleMgr() {
  const auto recFlowCtrlRuleGroup =
      tblMonitorOfFlowCtrlRule_->getFlowCtrlRuleGroup();
  const auto [statusCode, statusMsg] =
      std::ext::tls_get<FlowCtrlRuleMgr>().init(recFlowCtrlRuleGroup);
  if (statusCode != 0) {
    LOG_E("Do init failed. {}", statusMsg);
  }
}

void TDSrv::initPosMgr() {
  const auto sql = fmt::format("SELECT * FROM `posInfo`");
  std::ext::tls_get<PosMgr>().init(CONFIG, getDBEng(), sql);
}

void TDSrv::initAssetsMgr() {
  const auto sql = fmt::format("SELECT * FROM `assetInfo`");
  std::ext::tls_get<AssetsMgr>().init(CONFIG, getDBEng(), sql);
}

void TDSrv::initOrdMgr() {
  const auto filled = magic_enum::enum_integer(OrderStatus::Filled);
  const auto sql = fmt::format(
      "SELECT * FROM `orderInfo` WHERE `orderStatus` < {}; ", filled);
  std::ext::tls_get<OrdMgr>().init(CONFIG, getDBEng(), sql);
}

int TDSrv::initTDSrvTaskDispatcher() {
  const auto tdSrvTaskDispatcherParamInStrFmt =
      SetParam(DEFAULT_TASK_DISPATCHER_PARAM,
               CONFIG["tdSrvTaskDispatcherParam"].as<std::string>());
  const auto [ret, tdSrvTaskDispatcherParam] =
      MakeTaskDispatcherParam(tdSrvTaskDispatcherParamInStrFmt);
  if (ret != 0) {
    LOG_E("Init taskdispatcher failed. {}", tdSrvTaskDispatcherParamInStrFmt);
    return ret;
  }

  const auto makeAsyncTask = [this](const auto& task) {
    const auto hash = GetHashFromTask(task, conditionFieldGroup_);
    return std::make_tuple(0, std::make_shared<SHMIPCAsyncTask>(task, hash));
  };

  const auto getThreadForAsyncTask = [](const auto& asyncTask,
                                        auto taskSpecificThreadPoolSize) {
    const auto hash = std::any_cast<std::uint64_t>(asyncTask->arg_);
    const auto threadNo = hash % taskSpecificThreadPoolSize;
    return threadNo;
  };

  const auto handleAsyncTask = [this](auto& asyncTask) {
    const auto shmHeader =
        static_cast<const SHMHeader*>(asyncTask->task_->data_);
    switch (shmHeader->msgId_) {
      case MSG_ID_ON_ORDER:
        stgEngTaskHandler_->handleAsyncTask(asyncTask);
        break;
      case MSG_ID_ON_CANCEL_ORDER:
        stgEngTaskHandler_->handleAsyncTask(asyncTask);
        break;
      case MSG_ID_ON_STG_REG:
        stgEngTaskHandler_->handleAsyncTask(asyncTask);
        break;

      case MSG_ID_ON_ORDER_RET:
        tdGWTaskHandler_->handleAsyncTask(asyncTask);
        break;
      case MSG_ID_ON_CANCEL_ORDER_RET:
        tdGWTaskHandler_->handleAsyncTask(asyncTask);
        break;
      case MSG_ID_SYNC_ASSETS:
        tdGWTaskHandler_->handleAsyncTask(asyncTask);
        break;
      case MSG_ID_ON_TDGW_REG:
        tdGWTaskHandler_->handleAsyncTask(asyncTask);
        break;

      default:
        LOG_W("Unable to process msgId {}.", shmHeader->msgId_);
        break;
    }
  };

  auto onThreadStart = [this](std::uint32_t threadNo) {
    LOG_I("Init thread local data for thread {}.", threadNo);
    initFlowCtrlRuleMgr();
    initPosMgr();
    initAssetsMgr();
    initOrdMgr();
    getTDSrvRiskPluginMgr()->onThreadStart(threadNo);
  };

  auto onThreadExit = [this](std::uint32_t threadNo) {
    LOG_I("Uninit thread local data for thread {}.", threadNo);
    getTDSrvRiskPluginMgr()->onThreadExit(threadNo);
  };

  tdSrvTaskDispatcher_ = std::make_shared<TaskDispatcher<SHMIPCTaskSPtr>>(
      tdSrvTaskDispatcherParam, makeAsyncTask, getThreadForAsyncTask,
      handleAsyncTask, onThreadStart, onThreadExit);
  tdSrvTaskDispatcher_->init();

  return ret;
}

void TDSrv::initSHMSrv() {
  const auto tdGWAddr =
      fmt::format("{}@{}", AppName, CONFIG["tdGWChannel"].as<std::string>());
  shmSrvOfTDGW_ = std::make_shared<SHMSrv>(
      tdGWAddr, [this](const auto* shmBuf, std::size_t shmBufLen) {
        auto task = std::make_shared<SHMIPCTask>(shmBuf, shmBufLen);
        tdSrvTaskDispatcher_->dispatch(task);
      });

  const auto stgEngAddr =
      fmt::format("{}@{}", AppName, CONFIG["stgEngChannel"].as<std::string>());
  shmSrvOfStgEng_ = std::make_shared<SHMSrv>(
      stgEngAddr, [this](const auto* shmBuf, std::size_t shmBufLen) {
        auto task = std::make_shared<SHMIPCTask>(shmBuf, shmBufLen);
        tdSrvTaskDispatcher_->dispatch(task);
      });

  plugInChannel_ = CONFIG["plugInChannel"].as<std::string>();
  const auto plugInAddr = fmt::format("{}@{}", AppName, plugInChannel_);
  shmSrvOfPlugIn_ = std::make_shared<SHMSrv>(
      plugInAddr, [this](const auto* shmBuf, std::size_t shmBufLen) {
        const auto msgId = static_cast<const SHMHeader*>(shmBuf)->msgId_;
        LOG_W("Risk ctrl plug in recv msg {} - {}", msgId, GetMsgName(msgId));
      });

  topicOfTriggerRiskCtrl_ =
      fmt::format("{}{}TriggerRiskCrtl", plugInChannel_, SEP_OF_TOPIC);
}

void TDSrv::initScheduleTaskBundle() {
  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "removeExpiredChannel",
      [this]() {
        getTDGWGroup()->removeExpiredChannel();
        getStgEngGroup()->removeExpiredChannel();
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

  getScheduleTaskBundle()->emplace_back(std::make_shared<ScheduleTask>(
      "reloadTDSrvRiskPluginPath",
      [this]() {
        getTDSrvRiskPluginMgr()->load();
        return true;
      },
      ExecAtStartup::False, MilliSecInterval(5000)));
}

int TDSrv::doRun() {
  getDBEng()->start();
  if (const auto ret = tblMonitorOfSymbolInfo_->start(); ret != 0) {
    LOG_E("Run failed.");
    return ret;
  }

  tdSrvTaskDispatcher_->start();
  shmSrvOfTDGW_->start();
  shmSrvOfStgEng_->start();
  shmSrvOfPlugIn_->start();

  if (const auto ret = scheduleTaskBundleExecutor_->start(); ret != 0) {
    LOG_E("Start scheduler of multi task failed.");
    return ret;
  }

  return 0;
}

void TDSrv::cacheSyncTaskGroup(MsgId msgId, const std::any& task,
                               SyncToRiskMgr syncToRiskMgr, SyncToDB syncToDB) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    syncTaskGroup_.emplace_back(
        std::make_shared<SyncTask>(msgId, task, syncToRiskMgr, syncToDB));
  }
}

void TDSrv::handleSyncTaskGroup() {
  std::vector<SyncTaskSPtr> taskGroup;
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxSyncTaskGroup_);
    std::swap(taskGroup, syncTaskGroup_);
  }

  if (taskGroup.size() > 100) {
    LOG_W("Too many unprocessed task of sync. [num = {}]", taskGroup.size());
  }

  for (const auto& rec : taskGroup) {
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

    } else if (rec->msgId_ == MSG_ID_SYNC_POS_INFO) {
      const auto posChgInfo = std::any_cast<PosChgInfoSPtr>(rec->task_);
      for (const auto& posInfo : *posChgInfo) {
        const auto identity = GET_RAND_STR();
        const auto sql = posInfo->getSqlOfReplace();
        const auto [ret, execRet] = dbEng_->asyncExec(identity, sql);
        if (ret != 0) {
          LOG_W("Replace pos info from db failed. [{}]", sql);
        }
      }

    } else {
      LOG_W("Unhandled task of sync to db. {} - {}", rec->msgId_,
            GetMsgName(rec->msgId_))
    }
  }
}

void TDSrv::doExit(const boost::system::error_code* ec, int signalNum) {
  scheduleTaskBundleExecutor_->stop();
  shmSrvOfPlugIn_->stop();
  shmSrvOfStgEng_->stop();
  shmSrvOfTDGW_->stop();
  tdSrvTaskDispatcher_->stop();
  tblMonitorOfSymbolInfo_->stop();
  getDBEng()->stop();
}

}  // namespace bq::td::srv
