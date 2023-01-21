/*!
 * \file ClientId2OrderId.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/01/04
 *
 * \brief
 */

#pragma once

#include "def/BQDef.hpp"
#include "util/Pch.hpp"
#include "util/StdExt.hpp"

namespace bq::td {

using ClientId = std::uint64_t;

const static std::string SEP_OF_LINE = ":";

class ClientId2OrderId {
 public:
  void load(const std::string& filename);
  void save(const std::string& filename);

  void add(ClientId clientId, OrderId orderId);
  void remove(ClientId clientId);
  OrderId getOrderId(ClientId clientId);

  std::uint32_t getClientId() const {
    std::lock_guard<std::ext::spin_mutex> guard(mtxClientId2OrderId_);
    return ++lastClientId_;
  }

 private:
  mutable std::uint32_t lastClientId_{0};
  std::map<ClientId, OrderId> clientId2OrderId_;
  mutable std::ext::spin_mutex mtxClientId2OrderId_;
};

}  // namespace bq::td
