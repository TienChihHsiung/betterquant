/*!
 * \file TDSrvRiskPlugin.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "TDSrvRiskPlugin.hpp"

#include "TDSrv.hpp"
#include "util/File.hpp"
#include "util/Logger.hpp"
#include "util/String.hpp"

namespace bq::td::srv {

TDSrvRiskPlugin::TDSrvRiskPlugin(TDSrv* tdSrv) : tdSrv_(tdSrv) {}

int TDSrvRiskPlugin::init(const std::string& configFilename) {
  try {
    node_ = YAML::LoadFile(configFilename);
  } catch (const std::exception& e) {
    const auto statusMsg = fmt::format("Init config by file {} failed. [{}]",
                                       configFilename, e.what());
    std::cerr << statusMsg << std::endl;
    return -1;
  }

  logger_ = makeLogger(configFilename);
  if (logger_ == nullptr) {
    const auto statusMsg = fmt::format(
        "Init risk plug in {} failed "
        "because of init logger by file {} failed. ",
        name(), configFilename);
    std::cerr << statusMsg << std::endl;
    return -1;
  }

  name_ = fmt::format("{}-v{}",  //
                      node_["name"].as<std::string>(),
                      node_["version"].as<std::string>());

  enable_ = node_["enable"].as<bool>(false);

  const auto configFileCont = LoadFileContToStr(configFilename);
  md5SumOfConf_ = MD5Sum(configFileCont);

  return 0;
}

void TDSrvRiskPlugin::onThreadStart(std::uint32_t threadNo) {
  L_I(logger(), "Risk plugin {} thread {} start.", name(), threadNo);
  doOnThreadStart(threadNo);
}

void TDSrvRiskPlugin::doOnThreadStart(std::uint32_t threadNo) {}

void TDSrvRiskPlugin::onThreadExit(std::uint32_t threadNo) {
  L_I(logger(), "Risk plugin {} thread {} exit.", name(), threadNo);
  doOnThreadExit(threadNo);
}

void TDSrvRiskPlugin::doOnThreadExit(std::uint32_t threadNo) {}

int TDSrvRiskPlugin::load() {
  L_I(logger(), "Load risk plugin {}.", name());
  return doLoad();
}

void TDSrvRiskPlugin::unload() {
  L_I(logger(), "Unload risk plugin {}.", name());
  doUnload();
}

TDSrv* TDSrvRiskPlugin::getTDSrv() const { return tdSrv_; }

}  // namespace bq::td::srv
