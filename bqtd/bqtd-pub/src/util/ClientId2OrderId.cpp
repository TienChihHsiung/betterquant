/*!
 * \file ClientId2OrderId.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/01/04
 *
 * \brief
 */

#include "util/ClientId2OrderId.hpp"

#include "def/Def.hpp"
#include "util/File.hpp"
#include "util/Logger.hpp"

namespace bq::td {

void ClientId2OrderId::load(const std::string& filename) {
  if (boost::filesystem::exists(filename) == false) {
    LOG_I("The file {} does not exist, the data will not be loaded.", filename);
    return;
  }

  const auto [statusCode, lineGroup] = LoadFileContToVec(filename);
  if (statusCode != 0) {
    LOG_W("Load file content to vec from file {} failed.", filename);
    return;
  }

  for (std::size_t i = 0; i < lineGroup.size(); ++i) {
    if (i == 0) {
      lastClientId_ = CONV(ClientId, lineGroup[i]);
      continue;
    }
    std::vector<std::string> fieldGroup;
    boost::split(fieldGroup, lineGroup[i], boost::is_any_of(SEP_OF_LINE));
    if (fieldGroup.size() != 2) {
      LOG_W("Get invalid line from file {}. line = {}", filename, lineGroup[i]);
      continue;
    }
    const auto clientId = CONV(ClientId, fieldGroup[0]);
    const auto orderId = CONV(OrderId, fieldGroup[1]);
    {
      std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
      clientId2OrderId_.emplace(clientId, orderId);
      if (clientId2OrderId_.size() % 1000 == 0) {
        LOG_W("Size of client id info is {}.", clientId2OrderId_.size());
      }
    }
  }
  LOG_I("Load {} numbers of rec from file {}", lineGroup.size(), filename);
}

void ClientId2OrderId::save(const std::string& filename) {
  if (filename.empty()) return;

  std::vector<std::string> lineGroup;
  lineGroup.emplace_back(fmt::format("{}", lastClientId_));

  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
    for (const auto& rec : clientId2OrderId_) {
      const auto clientId = rec.first;
      const auto orderId = rec.second;
      const auto line = fmt::format("{}{}{}", clientId, SEP_OF_LINE, orderId);
      lineGroup.emplace_back(line);
    }
  }

  const auto statusCode = SaveDataToFile(filename, lineGroup);
  if (statusCode != 0) {
    LOG_W("Save data to file {} failed .", filename);
    return;
  }
  LOG_D("Save {} numbers of rec to file {}", lineGroup.size(), filename);
}

void ClientId2OrderId::add(ClientId clientId, OrderId orderId) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
    clientId2OrderId_.emplace(clientId, orderId);
    if (clientId2OrderId_.size() % 100 == 0) {
      LOG_W("Size of client id info is {}.", clientId2OrderId_.size());
    }
  }
}

void ClientId2OrderId::remove(ClientId clientId) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
    clientId2OrderId_.erase(clientId);
    if (clientId2OrderId_.size() % 100 == 0) {
      LOG_W("Size of client id info is {}.", clientId2OrderId_.size());
    }
  }
}

OrderId ClientId2OrderId::getOrderId(ClientId clientId) {
  {
    std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
    const auto iter = clientId2OrderId_.find(clientId);
    if (iter != std::end(clientId2OrderId_)) {
      return iter->second;
    }
  }
  return 0;
}

}  // namespace bq::td
