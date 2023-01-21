/*!
 * \file TopicMgr.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/26
 *
 * \brief
 */

#include "def/BQDef.hpp"
#include "def/Const.hpp"
#include "def/Def.hpp"
#include "def/StatusCode.hpp"
#include "util/Pch.hpp"
#include "util/StdExt.hpp"

namespace bq {

using Subscriber2TopicGroup = std::map<std::string, std::set<std::string>>;
using Topic2SubscriberGroup = std::map<std::string, std::set<std::string>>;
using Subscriber2ActiveTimeGroup = std::map<std::string, std::uint64_t>;

class Scheduler;
using SchedulerSPtr = std::shared_ptr<Scheduler>;

using CBHandleSubInfo = std::function<void(const std::any&)>;

enum class TopicMgrRole { Cli, Mid, Srv };

class TopicMgr {
 public:
  TopicMgr(const TopicMgr&) = delete;
  TopicMgr& operator=(const TopicMgr&) = delete;
  TopicMgr(const TopicMgr&&) = delete;
  TopicMgr& operator=(const TopicMgr&&) = delete;

  explicit TopicMgr(TopicMgrRole role,
                    const CBHandleSubInfo& cbHandleSubInfo = nullptr);
  ~TopicMgr();

 public:
  void start();
  void stop();

 public:
  int add(const std::string& subscriber, const std::string& topic);

  int remove(const std::string& subscriber, const std::string& topic);

  void handleSubInfoForCli();

 public:
  void updateForMid(const std::string& subscriber2TopicGroupInJsonFmt);

  void handleSubInfoForMid();

 public:
  void updateForSrv(const std::string& subscriber2TopicGroupInJsonFmt);

  void handleSubInfoForSrv();

 public:
  void handleSubInfo();

  Subscriber2TopicGroup removeExpiredSubscriber();

  std::string toJson(const Subscriber2TopicGroup& subscriber2TopicGroup);

  Subscriber2TopicGroup fromJson(
      const std::string& subscriber2TopicGroupInJsonFmt);

 public:
  Subscriber2TopicGroup getSubscriber2TopicGroup() const {
    {
      std::lock_guard<std::mutex> guard(mtxTopicMgr);
      return subscriber2TopicGroup_;
    }
  }

 private:
  TopicMgrRole role_;

  CBHandleSubInfo cbHandleSubInfo_{nullptr};
  SchedulerSPtr scheduler_{nullptr};

  Subscriber2TopicGroup subscriber2TopicGroup_;
  Subscriber2ActiveTimeGroup subscriber2ActiveTimeGroup_;
  Topic2SubscriberGroup oldTopic2SubscriberGroup_;
  mutable std::mutex mtxTopicMgr;
};

}  // namespace bq
