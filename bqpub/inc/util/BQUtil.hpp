/*!
 * \file BQUtil.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/BQConst.hpp"
#include "def/BQDef.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq {

enum class MDType : std::uint8_t;

using ConditionFieldGroup = std::vector<std::string>;

std::string convertTopic(const std::string& topic);

std::tuple<int, std::string> GetAddrFromTopic(const std::string& appName,
                                              const std::string& topic);

std::tuple<int, std::string> GetChannelFromAddr(const std::string& shmSvcAddr);

std::tuple<std::string, TopicHash> MakeTopicInfo(const std::string& marketCode,
                                                 const std::string& symbolType,
                                                 const std::string& symbolCode,
                                                 MDType mdType,
                                                 const std::string& ext = "");

std::uint64_t GetHashFromTask(const SHMIPCTaskSPtr& task,
                              const ConditionFieldGroup& conditionFieldGroup);

AcctId GetAcctIdFromTask(const SHMIPCTaskSPtr& task);

std::string ToPrettyStr(Decimal value);

void PrintLogo();

}  // namespace bq
