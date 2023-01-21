/*!
 * \file TDSrv.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "SHMIPCDef.hpp"
#include "SHMIPCMsgId.hpp"
#include "db/DBEngDef.hpp"
#include "util/Pch.hpp"
#include "util/StdExt.hpp"
#include "util/SvcBase.hpp"

namespace bq {
class PosMgr;
using PosMgrSPtr = std::shared_ptr<PosMgr>;
class AssetsMgr;
using AssetsMgrSPtr = std::shared_ptr<AssetsMgr>;
class OrdMgr;
using OrdMgrSPtr = std::shared_ptr<OrdMgr>;

struct SHMIPCTask;
using SHMIPCTaskSPtr = std::shared_ptr<SHMIPCTask>;
template <typename Task>
class TaskDispatcher;
template <typename Task>
using TaskDispatcherSPtr = std::shared_ptr<TaskDispatcher<Task>>;

class Scheduler;
using SchedulerSPtr = std::shared_ptr<Scheduler>;

struct ScheduleTask;
using ScheduleTaskSPtr = std::shared_ptr<ScheduleTask>;
using ScheduleTaskBundle = std::vector<ScheduleTaskSPtr>;
using ScheduleTaskBundleSPtr = std::shared_ptr<ScheduleTaskBundle>;

struct SyncTask;
using SyncTaskSPtr = std::shared_ptr<SyncTask>;

enum class SyncToRiskMgr;
enum class SyncToDB;

class FlowCtrlRuleMgr;
using FlowCtrlRuleMgrSPtr = std::shared_ptr<FlowCtrlRuleMgr>;

using ConditionFieldGroup = std::vector<std::string>;
}  // namespace bq

namespace bq::db {
class TBLMonitorOfFlowCtrlRule;
using TBLMonitorOfFlowCtrlRuleSPtr = std::shared_ptr<TBLMonitorOfFlowCtrlRule>;
class TBLMonitorOfSymbolInfo;
using TBLMonitorOfSymbolInfoSPtr = std::shared_ptr<TBLMonitorOfSymbolInfo>;
}  // namespace bq::db

namespace bq::td::srv {

class TDSrvRiskPluginMgr;
using TDSrvRiskPluginMgrSPtr = std::shared_ptr<TDSrvRiskPluginMgr>;

class PosMgrRestorer;
using PosMgrRestorerSPtr = std::shared_ptr<PosMgrRestorer>;

class TDGWTaskHandler;
using TDGWTaskHandlerSPtr = std::shared_ptr<TDGWTaskHandler>;

class StgEngTaskHandler;
using StgEngTaskHandlerSPtr = std::shared_ptr<StgEngTaskHandler>;

class ClientChannelGroup;
using ClientChannelGroupSPtr = std::shared_ptr<ClientChannelGroup>;

class TDSrv : public SvcBase {
 public:
  using SvcBase::SvcBase;

 private:
  int prepareInit() final;
  int doInit() final;

 private:
  int initDBEng();
  void initTBLMonitorOfFlowCtrlRule();
  void initTBLMonitorOfSymbolInfo();
  void initFlowCtrlRuleMgr();
  void initPosMgr();
  void initAssetsMgr();
  void initOrdMgr();
  int initTDSrvTaskDispatcher();
  void initSHMSrv();
  void initScheduleTaskBundle();

 public:
  int doRun() final;

 private:
  void doExit(const boost::system::error_code* ec, int signalNum) final;

 public:
  db::DBEngSPtr getDBEng() const { return dbEng_; }

  const TDSrvRiskPluginMgrSPtr& getTDSrvRiskPluginMgr() const {
    return tdSrvRiskPluginMgr_;
  }

  const db::TBLMonitorOfFlowCtrlRuleSPtr& getTBLMonitorOfFlowCtrlRule() {
    return tblMonitorOfFlowCtrlRule_;
  }

  const db::TBLMonitorOfSymbolInfoSPtr& getTBLMonitorOfSymbolInfo() const {
    return tblMonitorOfSymbolInfo_;
  }

  ClientChannelGroupSPtr getTDGWGroup() const { return tdGWGroup_; }
  ClientChannelGroupSPtr getStgEngGroup() const { return stgEngGroup_; }

  TaskDispatcherSPtr<SHMIPCTaskSPtr> getTDSrvTaskDispatcher() const {
    return tdSrvTaskDispatcher_;
  }

  SHMSrvSPtr getSHMSrvOfTDGW() const { return shmSrvOfTDGW_; }
  SHMSrvSPtr getSHMSrvOfStgEng() const { return shmSrvOfStgEng_; }
  SHMSrvSPtr getSHMSrvOfPlugIn() const { return shmSrvOfPlugIn_; }

  std::string getPlugInChannel() const { return plugInChannel_; }
  std::string getTopicOfTriggerRiskCtrl() const {
    return topicOfTriggerRiskCtrl_;
  }

  void cacheSyncTaskGroup(MsgId msgId, const std::any& task,
                          SyncToRiskMgr syncToRiskMgr, SyncToDB syncToDB);
  void handleSyncTaskGroup();

  ScheduleTaskBundleSPtr& getScheduleTaskBundle() {
    return scheduleTaskBundle_;
  };

 private:
  db::DBEngSPtr dbEng_{nullptr};

  db::TBLMonitorOfFlowCtrlRuleSPtr tblMonitorOfFlowCtrlRule_{nullptr};
  db::TBLMonitorOfSymbolInfoSPtr tblMonitorOfSymbolInfo_{nullptr};

  ConditionFieldGroup conditionFieldGroup_;

  TDSrvRiskPluginMgrSPtr tdSrvRiskPluginMgr_{nullptr};
  PosMgrRestorerSPtr posMgrRestorer_{nullptr};

  ClientChannelGroupSPtr tdGWGroup_{nullptr};
  ClientChannelGroupSPtr stgEngGroup_{nullptr};

  TDGWTaskHandlerSPtr tdGWTaskHandler_{nullptr};
  StgEngTaskHandlerSPtr stgEngTaskHandler_{nullptr};

  TaskDispatcherSPtr<SHMIPCTaskSPtr> tdSrvTaskDispatcher_{nullptr};

  SHMSrvSPtr shmSrvOfTDGW_{nullptr};
  SHMSrvSPtr shmSrvOfStgEng_{nullptr};
  SHMSrvSPtr shmSrvOfPlugIn_{nullptr};

  std::string plugInChannel_;
  std::string topicOfTriggerRiskCtrl_;

  std::vector<SyncTaskSPtr> syncTaskGroup_;
  std::ext::spin_mutex mtxSyncTaskGroup_;

  ScheduleTaskBundleSPtr scheduleTaskBundle_{nullptr};
  SchedulerSPtr scheduleTaskBundleExecutor_{nullptr};
};

}  // namespace bq::td::srv
