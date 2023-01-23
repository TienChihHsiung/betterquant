/*!
 * \file Config.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#pragma once

#include "util/ConfigBase.hpp"
#include "util/Pch.hpp"

namespace bq::md::svc {

struct ApiInfo {
  std::string apiName_;

  std::uint8_t clientId_;

  std::string quoteServerIP_;
  std::int32_t quoteServerPort_;

  std::string quoteUsername_;
  std::string quotePassword_;

  std::string protocolType_;

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
  bool isRealMode() const { return isRealMode_; }
  bool isSimedMode() const { return !isRealMode(); }

 private:
  ApiInfoSPtr apiInfo_{nullptr};
  bool isRealMode_{false};
};

}  // namespace bq::md::svc
