/*!
 * \file TopicMgr.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/26
 *
 * \brief
 */

#include "util/TopicMgr.hpp"

#include "util/BQUtil.hpp"
#include "util/Datetime.hpp"
#include "util/Logger.hpp"
#include "util/Scheduler.hpp"

namespace bq {

TopicMgr::TopicMgr(TopicMgrRole role, const CBHandleSubInfo& cbHandleSubInfo) {
  role_ = role;
  cbHandleSubInfo_ = cbHandleSubInfo;
  scheduler_ = std::make_shared<Scheduler>(
      "TopicMgr", [this]() { handleSubInfo(); }, 1 * 100);
}

TopicMgr::~TopicMgr() {}

void TopicMgr::start() { scheduler_->start(); }
void TopicMgr::stop() { scheduler_->stop(); }

int TopicMgr::add(const std::string& subscriber, const std::string& topic) {
  const auto internalTopic = convertTopic(topic);
  if (!boost::starts_with(internalTopic, TOPIC_PREFIX_OF_MARKET_DATA)) {
    return 0;
  }

  {
    std::lock_guard<std::mutex> guard(mtxTopicMgr);

    const auto iter = subscriber2TopicGroup_.find(subscriber);

    if (iter == std::end(subscriber2TopicGroup_)) {
      subscriber2TopicGroup_[subscriber].emplace(internalTopic);
      return 0;

    } else {
      auto& topicGroup = iter->second;
      if (topicGroup.find(internalTopic) == std::end(topicGroup)) {
        topicGroup.emplace(internalTopic);
        return 0;

      } else {
        return SCODE_BQPUB_TOPIC_ALREADY_SUB;
      }
    }
  }

  return 0;
}

int TopicMgr::remove(const std::string& subscriber, const std::string& topic) {
  const auto internalTopic = convertTopic(topic);
  if (!boost::starts_with(internalTopic, TOPIC_PREFIX_OF_MARKET_DATA)) {
    return 0;
  }

  {
    std::lock_guard<std::mutex> guard(mtxTopicMgr);

    const auto iter = subscriber2TopicGroup_.find(subscriber);

    if (iter == std::end(subscriber2TopicGroup_)) {
      return SCODE_BQPUB_TOPIC_NOT_SUB;

    } else {
      auto& topicGroup = iter->second;
      const auto iterOfTopicGroup = topicGroup.find(internalTopic);
      if (iterOfTopicGroup != std::end(topicGroup)) {
        topicGroup.erase(iterOfTopicGroup);

        if (topicGroup.empty()) {
          subscriber2TopicGroup_.erase(iter);
        }

        return 0;

      } else {
        return SCODE_BQPUB_TOPIC_NOT_SUB;
      }
    }
  }
}

void TopicMgr::handleSubInfoForCli() {
  const auto subscriber2TopicGroup = getSubscriber2TopicGroup();
  const auto subscriber2TopicGroupInJsonFmt = toJson(subscriber2TopicGroup);
  if (cbHandleSubInfo_) {
    LOG_T("Sync {}", subscriber2TopicGroupInJsonFmt);
    cbHandleSubInfo_(subscriber2TopicGroupInJsonFmt);
  }
}

void TopicMgr::updateForMid(const std::string& subscriber2TopicGroupInJsonFmt) {
  const auto subscriber2TopicGroup = fromJson(subscriber2TopicGroupInJsonFmt);
  {
    std::lock_guard<std::mutex> guard(mtxTopicMgr);
    for (const auto& subscriber2Topic : subscriber2TopicGroup) {
      const auto subscriber = subscriber2Topic.first;
      subscriber2TopicGroup_[subscriber] = subscriber2Topic.second;
      const auto now = GetTotalSecSince1970();
      subscriber2ActiveTimeGroup_[subscriber] = now;
    }
  }
}

void TopicMgr::handleSubInfoForMid() {
  Subscriber2TopicGroup subscriber2TopicGroup;
  {
    std::lock_guard<std::mutex> guard(mtxTopicMgr);
    subscriber2TopicGroup = removeExpiredSubscriber();
  }
  const auto subscriber2TopicGroupInJsonFmt = toJson(subscriber2TopicGroup);
  if (cbHandleSubInfo_) {
    LOG_I("Sync {}", subscriber2TopicGroupInJsonFmt);
    cbHandleSubInfo_(subscriber2TopicGroupInJsonFmt);
  }
}

void TopicMgr::updateForSrv(const std::string& subscriber2TopicGroupInJsonFmt) {
  updateForMid(subscriber2TopicGroupInJsonFmt);
}

void TopicMgr::handleSubInfoForSrv() {
  TopicGroup topicGroupNeedSub;
  TopicGroup topicGroupNeedUnSub;
  {
    std::lock_guard<std::mutex> guard(mtxTopicMgr);

    removeExpiredSubscriber();

    Topic2SubscriberGroup newTopic2SubscriberGroup;
    for (const auto& subscriber2Topic : subscriber2TopicGroup_) {
      const auto& subscriber = subscriber2Topic.first;
      const auto& topicGroup = subscriber2Topic.second;
      for (const auto& topic : topicGroup) {
        newTopic2SubscriberGroup[topic].emplace(subscriber);
      }
    }

    for (const auto& topic2Subscriber : newTopic2SubscriberGroup) {
      const auto& topic = topic2Subscriber.first;
      if (oldTopic2SubscriberGroup_.find(topic) ==
          std::end(oldTopic2SubscriberGroup_)) {
        topicGroupNeedSub.emplace(topic);
      }
    }

    for (const auto& topic2Subscriber : oldTopic2SubscriberGroup_) {
      const auto& topic = topic2Subscriber.first;
      if (newTopic2SubscriberGroup.find(topic) ==
          std::end(newTopic2SubscriberGroup)) {
        topicGroupNeedUnSub.emplace(topic);
      }
    }

    oldTopic2SubscriberGroup_ = newTopic2SubscriberGroup;
  }

  if (!topicGroupNeedSub.empty()) {
    LOG_I("Topic need sub: {}", boost::join(topicGroupNeedSub, ","));
  }

  if (!topicGroupNeedUnSub.empty()) {
    LOG_I("Topic need unsub: {}", boost::join(topicGroupNeedUnSub, ","));
  }

  if (cbHandleSubInfo_) {
    cbHandleSubInfo_(std::make_tuple(topicGroupNeedSub, topicGroupNeedUnSub));
  }
}

Subscriber2TopicGroup TopicMgr::removeExpiredSubscriber() {
  const std::uint32_t expiredTime = 10;
  for (const auto& subscriber2ActiveTime : subscriber2ActiveTimeGroup_) {
    const auto& subscriber = subscriber2ActiveTime.first;
    const auto now = GetTotalSecSince1970();
    const auto activeTime = subscriber2ActiveTime.second;
    if (now - activeTime > expiredTime) {
      subscriber2TopicGroup_.erase(subscriber);
    }
  }
  return subscriber2TopicGroup_;
}

void TopicMgr::handleSubInfo() {
  switch (role_) {
    case TopicMgrRole::Cli:
      LOG_T("Handle sub info for cli.");
      handleSubInfoForCli();
      break;
    case TopicMgrRole::Mid:
      LOG_T("Handle sub info for mid.");
      handleSubInfoForMid();
      break;
    case TopicMgrRole::Srv:
      LOG_T("Handle sub info for srv.");
      handleSubInfoForSrv();
      break;
  }
}

std::string TopicMgr::toJson(
    const Subscriber2TopicGroup& subscriber2TopicGroup) {
  rapidjson::StringBuffer strBuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);

  writer.StartObject();
  writer.Key("subscriber2TopicGroup");
  writer.StartArray();

  for (const auto& subscriber2Topic : subscriber2TopicGroup) {
    const auto& subscriber = subscriber2Topic.first;
    const auto& topicGroup = subscriber2Topic.second;

    writer.StartObject();

    writer.Key("subscriber");
    writer.String(subscriber.c_str());

    writer.Key("topicGroup");
    writer.StartArray();
    for (const auto& topic : topicGroup) {
      writer.String(topic.c_str());
    }
    writer.EndArray();

    writer.EndObject();
  }

  writer.EndArray();
  writer.EndObject();

  const auto ret = strBuf.GetString();
  return ret;
}

Subscriber2TopicGroup TopicMgr::fromJson(
    const std::string& subscriber2TopicGroupInJsonFmt) {
  Doc doc;
  doc.Parse(subscriber2TopicGroupInJsonFmt.c_str());

  Subscriber2TopicGroup ret;
  for (std::size_t i = 0; i < doc["subscriber2TopicGroup"].Size(); ++i) {
    const auto& d = doc["subscriber2TopicGroup"][i];
    const auto subscriber = d["subscriber"].GetString();
    for (std::size_t j = 0; j < d["topicGroup"].Size(); ++j) {
      const auto topic = d["topicGroup"][j].GetString();
      ret[subscriber].emplace(topic);
    }
  }
  return ret;
}

}  // namespace bq
