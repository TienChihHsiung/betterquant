/*!
 * \file MDSvcOfCN.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "MDSvcOfCN.hpp"

#include "Config.hpp"
#include "MDCache.hpp"
#include "MDPlayback.hpp"
#include "MDStorageSvc.hpp"
#include "RawMDHandler.hpp"
#include "SHMIPCConst.hpp"
#include "SHMSrv.hpp"
#include "SHMSrvMsgHandler.hpp"
#include "db/DBE.hpp"
#include "db/DBEng.hpp"
#include "db/DBEngConst.hpp"
#include "def/SymbolInfo.hpp"
#include "tdeng/TDEngConnpool.hpp"
#include "tdeng/TDEngConst.hpp"
#include "tdeng/TDEngParam.hpp"
#include "util/Logger.hpp"
#include "util/MarketDataCond.hpp"
#include "util/String.hpp"
#include "util/TopicMgr.hpp"

using namespace std;

namespace bq::md::svc {

int MDSvcOfCN::prepareInit() {
  auto retOfConfInit = Config::get_mutable_instance().init(configFilename_);
  if (retOfConfInit != 0) {
    const auto statusMsg = fmt::format("Prepare init failed.");
    std::cerr << statusMsg << std::endl;
    return retOfConfInit;
  }

  const auto retOfLoggerInit = InitLogger(CONFIG);
  if (retOfLoggerInit != 0) {
    const auto statusMsg = fmt::format("Prepare init failed.");
    std::cerr << statusMsg << std::endl;
    return retOfLoggerInit;
  }

  marketCode2SHMSrvGroup_ = std::make_shared<MarketCode2SHMSrvGroup>();
  return 0;
}

MDSvcOfCN::~MDSvcOfCN() {}

int MDSvcOfCN::doInit() {
  if (const auto ret = initDBEng(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  if (const auto ret = initTDEng(); ret != 0) {
    LOG_E("Do init failed.");
    return ret;
  }

  shmSrvMsgHandler_ = std::make_shared<SHMSrvMsgHandler>(this);
  initSHMSrvGroup();

  assert(rawMDHandler_ != nullptr && "rawMDHandler_ != nullptr");
  if (const auto statusCode = rawMDHandler_->init(); statusCode != 0) {
    return statusCode;
  }

  if (Config::get_const_instance().isSimedMode()) {
    mdCache_ = std::make_shared<MDCache>(this);
    mdPlayback_ = std::make_shared<MDPlayback>(this);
  } else {
    mdStorageSvc_ = std::make_shared<MDStorageSvc>(this);
    if (const auto statusCode = mdStorageSvc_->init(); statusCode != 0) {
      return statusCode;
    }
  }

  initTopicMgr();
  return 0;
}

int MDSvcOfCN::initDBEng() {
  const auto dbEngParam = SetParam(db::DEFAULT_DB_ENG_PARAM,
                                   CONFIG["dbEngParam"].as<std::string>());
  int retOfMakeDBEng = 0;
  std::tie(retOfMakeDBEng, dbEng_) = db::MakeDBEng(
      dbEngParam, [](db::DBTaskSPtr& dbTask, const StringSPtr& dbExecRet) {
        LOG_D("Exec sql finished. [{}] [exec result = {}]", dbTask->toStr(),
              *dbExecRet);
      });
  if (retOfMakeDBEng != 0) {
    LOG_E("Init dbeng failed. {}", dbEngParam);
    return retOfMakeDBEng;
  }

  if (auto retOfInit = getDBEng()->init(); retOfInit != 0) {
    LOG_E("Init dbeng failed. {}", dbEngParam);
    return retOfInit;
  }

  return 0;
}

int MDSvcOfCN::initTDEng() {
  const auto tdEngParamInStrFmt = SetParam(
      tdeng::DEFAULT_TDENG_PARAM, CONFIG["tdEngParam"].as<std::string>());
  const auto [ret, tdEngParam] = tdeng::MakeTDEngParam(tdEngParamInStrFmt);
  if (ret != 0) {
    LOG_E("Init failed. {}", tdEngParamInStrFmt);
    return ret;
  }

  tdEngConnpool_ = std::make_shared<tdeng::TDEngConnpool>(tdEngParam);
  if (const auto statusCode = tdEngConnpool_->init(); statusCode != 0) {
    LOG_E("Init failed. {}", tdEngParamInStrFmt);
    return -1;
  }

  return 0;
}

void MDSvcOfCN::initSHMSrvGroup() {
  const auto apiInfo = Config::get_const_instance().getApiInfo();
  const auto symbolType = CONFIG["symbolType"].as<std::string>();
  for (std::size_t i = 0; i < CONFIG["marketCodeGroup"].size(); ++i) {
    const auto marketCode = CONFIG["marketCodeGroup"][i].as<std::string>();

    const auto addrOfSHMSrv = fmt::format(
        "{}-{}-INSTANCE-{}{}{}{}{}{}{}", TOPIC_PREFIX_OF_MARKET_DATA,
        apiInfo->apiName_, apiInfo->clientId_, SEP_OF_SHM_SVC,
        TOPIC_PREFIX_OF_MARKET_DATA, SEP_OF_SHM_SVC, marketCode, SEP_OF_SHM_SVC,
        symbolType);
    const auto shmSrv = std::make_shared<SHMSrv>(
        addrOfSHMSrv, [this](const auto* shmBuf, std::size_t shmBufLen) {
          assert(shmSrvMsgHandler_ != nullptr &&
                 "shmSrvMsgHandler_ != nullptr");
          shmSrvMsgHandler_->handleReq(shmBuf, shmBufLen);
        });

    const auto m = GetMarketCode(marketCode);
    marketCode2SHMSrvGroup_->emplace(m, shmSrv);
  }
}

void MDSvcOfCN::initTopicMgr() {
  if (topicMgr_ == nullptr) {
    topicMgr_ = std::make_shared<TopicMgr>(
        TopicMgrRole::Srv, [this](const auto& anyData) {
          const auto topicNeedSubAndUnSub =
              std::any_cast<std::tuple<TopicGroup, TopicGroup>>(anyData);
          assert(shmSrvMsgHandler_ != nullptr &&
                 "shmSrvMsgHandler_ != nullptr");
          handleTopicNeedSubAndUnSub(topicNeedSubAndUnSub);
        });
  }
}

void MDSvcOfCN::handleTopicNeedSubAndUnSub(
    const std::tuple<TopicGroup, TopicGroup>& topicGroupNeedSubAndUnsub) {
  if (CONFIG["subAllMarketData"].as<bool>(false) == true) {
    return;
  }

  const auto [topicGroupNeedSub, topicGroupNeedUnSub] =
      topicGroupNeedSubAndUnsub;

  if (!topicGroupNeedSub.empty()) {
    LOG_T("+++ Topic need sub: {}", boost::join(topicGroupNeedSub, ","));
  }

  if (!topicGroupNeedUnSub.empty()) {
    LOG_T("--- Topic need unsub: {}", boost::join(topicGroupNeedUnSub, ","));
  }

  MarketDataCondGroup marketDataCondNeedSub;
  for (const auto& topic : topicGroupNeedSub) {
    const auto [statusCode, marketDataCond] = GetMarketDataCondFromTopic(topic);
    if (statusCode != 0) {
      LOG_W("Handle topic of sub failed because of invalid topic. {}", topic);
      continue;
    }
    marketDataCondNeedSub.emplace_back(marketDataCond);
  }

  if (!marketDataCondNeedSub.empty()) {
    if (Config::get_const_instance().isSimedMode() == false) {
      doSub(marketDataCondNeedSub);
    }
  }

  MarketDataCondGroup marketDataCondNeedUnSub;
  for (const auto& topic : topicGroupNeedUnSub) {
    const auto [statusCode, marketDataCond] = GetMarketDataCondFromTopic(topic);
    if (statusCode != 0) {
      LOG_W("Handle topic of unsub failed because of invalid topic. {}", topic);
      continue;
    }
    marketDataCondNeedUnSub.emplace_back(marketDataCond);
  }

  if (!marketDataCondNeedUnSub.empty()) {
    if (Config::get_const_instance().isSimedMode() == false) {
      doUnSub(marketDataCondNeedUnSub);
    }
  }
}

int MDSvcOfCN::beforeRun() {
  const auto apiInfo = Config::get_const_instance().getApiInfo();
  LOG_I("Run market data service of {}.", apiInfo->apiName_);

  if (Config::get_const_instance().isSimedMode() == false) {
    mdStorageSvc_->start();
  }
  rawMDHandler_->start();
  for (const auto marketCode2SHMSrv : *marketCode2SHMSrvGroup_) {
    const auto& shmSrv = marketCode2SHMSrv.second;
    shmSrv->start();
  }

  if (Config::get_const_instance().isSimedMode()) {
    const auto statusCode = startGatewayOfSim();
    if (statusCode != 0) {
      LOG_E("Before run failed because of start gateway of sim failed.");
      return statusCode;
    }
  } else {
    const auto statusCode = startGateway();
    if (statusCode != 0) {
      LOG_E("Before run failed because of start gateway failed.");
      return statusCode;
    }
  }

  return 0;
}

int MDSvcOfCN::startGatewayOfSim() {
  const auto statusCode = mdCache_->start();
  if (statusCode != 0) {
    return statusCode;
  }
  mdPlayback_->start();
  return 0;
}

int MDSvcOfCN::afterRun() {
  topicMgr_->start();
  return SvcBase::afterRun();
}

void MDSvcOfCN::beforeExit(const boost::system::error_code* ec, int signalNum) {
  if (Config::get_const_instance().isSimedMode()) {
    stopGatewayOfSim();
  } else {
    stopGateway();
  }
}

void MDSvcOfCN::stopGatewayOfSim() {
  mdPlayback_->stop();
  mdCache_->stop();
}

void MDSvcOfCN::doExit(const boost::system::error_code* ec, int signalNum) {
  topicMgr_->stop();

  for (const auto marketCode2SHMSrv : *marketCode2SHMSrvGroup_) {
    const auto& shmSrv = marketCode2SHMSrv.second;
    shmSrv->stop();
  }
  rawMDHandler_->stop();

  if (Config::get_const_instance().isSimedMode() == false) {
    mdStorageSvc_->flushMDToTDEng();
    mdStorageSvc_->stop();
  }

  tdEngConnpool_->uninit();

  const auto apiInfo = Config::get_const_instance().getApiInfo();
  LOG_I("Exit market data service of {}.", apiInfo->apiName_);
}

SHMSrvSPtr MDSvcOfCN::getSHMSrv(MarketCode marketCode) const {
  const auto iter = marketCode2SHMSrvGroup_->find(marketCode);
  if (iter != std::end(*marketCode2SHMSrvGroup_)) {
    return iter->second;
  }
  return nullptr;
}

}  // namespace bq::md::svc
