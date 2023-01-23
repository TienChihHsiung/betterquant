/*!
 * \file Config.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "Config.hpp"

#include "util/Logger.hpp"
#include "util/Util.hpp"

namespace bq::md::svc {

int Config::afterInit(const std::string& configFilename) {
  apiInfo_ = std::make_shared<ApiInfo>();

  apiInfo_->apiName_ = CONFIG["api"]["apiName"].as<std::string>();
  apiInfo_->clientId_ = CONFIG["api"]["clientId"].as<std::uint8_t>();

  apiInfo_->quoteServerIP_ = CONFIG["api"]["quoteServerIP"].as<std::string>();
  apiInfo_->quoteServerPort_ =
      CONFIG["api"]["quoteServerPort"].as<std::int32_t>();

  apiInfo_->quoteUsername_ = CONFIG["api"]["quoteUsername"].as<std::string>();
  apiInfo_->quotePassword_ = CONFIG["api"]["quotePassword"].as<std::string>();

  apiInfo_->protocolType_ = CONFIG["api"]["protocolType"].as<std::string>();

  apiInfo_->localIP_ = CONFIG["api"]["localIP"].as<std::string>();
  apiInfo_->secIntervalOfReconnect_ =
      CONFIG["api"]["secIntervalOfReconnect"].as<std::uint32_t>();
  apiInfo_->secIntervalOfHeartBeat_ =
      CONFIG["api"]["secIntervalOfHeartBeat"].as<std::uint32_t>();

  apiInfo_->loggerPath_ = CONFIG["api"]["loggerPath"].as<std::string>();

  isRealMode_ = CONFIG["simedMode"]["enable"].as<bool>() == false;

  return 0;
}

}  // namespace bq::md::svc
