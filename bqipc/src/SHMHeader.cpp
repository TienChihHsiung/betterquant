/*!
 * \file SHMHeader.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "SHMHeader.hpp"

#include "SHMIPCMsgId.hpp"
#include "def/Def.hpp"
#include "util/Datetime.hpp"

namespace bq {

std::string SHMHeader::toStr() const {
  const auto topic = strlen(topic_) == 0 ? "empty" : topic_;
  const auto ret = fmt::format("{} Channel: {} {} {} topicHash: {} topic: {}",
                               GetMsgName(msgId_), clientChannel_,
                               magic_enum::enum_name(direction_),
                               ConvertTsToPtime(timestamp_), topicHash_, topic);
  return ret;
}

std::string SHMHeader::toJson() const {
  rapidjson::StringBuffer strBuf;
  rapidjson::Writer<rapidjson::StringBuffer> writer(strBuf);
  writer.StartObject();

  writer.Key("msgId");
  writer.Uint(msgId_);

  writer.Key("clientChannel");
  writer.Uint(clientChannel_);

  writer.Key("direction");
  writer.String(ENUM_TO_STR(direction_).data());

  writer.Key("timestamp");
  writer.Uint64(timestamp_);

  writer.Key("topicHash");
  writer.Uint64(topicHash_);

  writer.Key("topic");
  writer.String(topic_);

  writer.EndObject();

  const auto ret = strBuf.GetString();
  return ret;
}

}  // namespace bq
