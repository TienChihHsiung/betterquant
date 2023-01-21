/*!
 * \file Config.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "util/ConfigBase.hpp"
#include "util/Pch.hpp"

namespace bq::td::svc {

struct ApiInfo {
  std::string apiName_;

  std::uint8_t clientId_;

  std::string traderServerIP_;
  std::int32_t traderServerPort_;

  std::string traderUsername_;
  std::string traderPassword_;
  std::string softwareKey_;

  std::string localIP_;
  std::uint32_t secIntervalOfReconnect_{10};
  std::uint32_t secIntervalOfHeartBeat_{15};

  std::string loggerPath_;
};
using ApiInfoSPtr = std::shared_ptr<ApiInfo>;

class Config : public ConfigBase,
               public boost::serialization::singleton<Config> {
 private:
  int afterInit(const std::string& configFilename) final;

 public:
  ApiInfoSPtr getApiInfo() const { return apiInfo_; }

 private:
  ApiInfoSPtr apiInfo_{nullptr};
};

}  // namespace bq::td::svc
