/*!
 * \file Config.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "Config.hpp"

#include "util/Util.hpp"

namespace bq::td::svc {

int Config::afterInit(const std::string& configFilename) {
  apiInfo_ = std::make_shared<ApiInfo>();

  apiInfo_->apiName_ = CONFIG["api"]["apiName"].as<std::string>();
  apiInfo_->clientId_ = CONFIG["api"]["clientId"].as<std::uint8_t>();

  apiInfo_->traderServerIP_ = CONFIG["api"]["traderServerIP"].as<std::string>();
  apiInfo_->traderServerPort_ =
      CONFIG["api"]["traderServerPort"].as<std::int32_t>();

  apiInfo_->traderUsername_ = CONFIG["api"]["traderUsername"].as<std::string>();
  apiInfo_->traderPassword_ = CONFIG["api"]["traderPassword"].as<std::string>();
  apiInfo_->softwareKey_ = CONFIG["api"]["softwareKey"].as<std::string>("");

  apiInfo_->localIP_ = CONFIG["api"]["localIP"].as<std::string>();
  apiInfo_->secIntervalOfReconnect_ =
      CONFIG["api"]["secIntervalOfReconnect"].as<std::uint32_t>();
  apiInfo_->secIntervalOfHeartBeat_ =
      CONFIG["api"]["secIntervalOfHeartBeat"].as<std::uint32_t>();

  apiInfo_->loggerPath_ = CONFIG["api"]["loggerPath"].as<std::string>();

  return 0;
}

}  // namespace bq::td::svc
