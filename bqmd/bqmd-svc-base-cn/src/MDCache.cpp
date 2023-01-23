/*!
 * \file MDCache.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/11/27
 *
 * \brief
 */

#include "MDCache.hpp"

#include "Config.hpp"
#include "MDSvcOfCN.hpp"
#include "def/DataStruOfMD.hpp"
#include "def/Def.hpp"
#include "def/StatusCode.hpp"
#include "tdeng/TDEngUtil.hpp"
#include "util/BQMDHis.hpp"
#include "util/Datetime.hpp"
#include "util/Json.hpp"
#include "util/Logger.hpp"
#include "util/MarketDataCond.hpp"
#include "util/String.hpp"

namespace bq::md::svc {

int MDCache::start() {
  secOfCacheMD_ = CONFIG["simedMode"]["secOfCacheMD"].as<std::uint32_t>(60);
  if (secOfCacheMD_ > MAX_SEC_OF_CACHE_MD_SIM) {
    LOG_W(
        "Cache a batch of his market data failed "
        "because of secOfCacheMD greater than {}.",
        MAX_SEC_OF_CACHE_MD_SIM);
    return -1;
  }

  const auto usOffsetOfExchAndLocalTs =
      CONFIG["simedMode"]["secOffsetOfExchAndLocalTs"].as<std::uint64_t>() *
      1000000ULL;

  int statusCode = 0;
  const auto playbackDateTimeStart =
      CONFIG["simedMode"]["playbackDateTimeStart"].as<std::string>("");
  std::tie(statusCode, exchTsStart_) =
      ConvertISODatetimeToTs(playbackDateTimeStart);
  if (statusCode != 0) {
    LOG_W(
        "Cache a batch of his market data failed "
        "because of invalid playbackDateTimeStart {} in config.",
        playbackDateTimeStart);
    return -1;
  }
  tsStart_ = exchTsStart_ - usOffsetOfExchAndLocalTs;

  const auto playbackDateTimeEnd =
      CONFIG["simedMode"]["playbackDateTimeEnd"].as<std::string>("");
  std::tie(statusCode, exchTsEnd_) =
      ConvertISODatetimeToTs(playbackDateTimeEnd);
  if (statusCode != 0) {
    LOG_W(
        "Cache a batch of his market data failed "
        "because of invalid playbackDateTimeEnd {} in config.",
        playbackDateTimeEnd);
    return -1;
  }
  tsEnd_ = exchTsEnd_ + usOffsetOfExchAndLocalTs;

  if (exchTsEnd_ <= exchTsStart_) {
    LOG_W(
        "Cache a batch of his market data failed because of invalid "
        "playbackDateTimeEnd {} <= playbackDateTimeStart {} in config.",
        playbackDateTimeEnd, playbackDateTimeStart);
    return -1;
  }

  tsStartOfCurCache_ = tsStart_;

  keepRunning_.store(true);
  threadCacheMDHis_ = std::make_unique<std::thread>([this]() { cacheMDHis(); });

  return 0;
}

void MDCache::stop() {
  keepRunning_.store(false);
  if (threadCacheMDHis_->joinable()) {
    threadCacheMDHis_->join();
  }
}

Ts2MarketDataOfSimGroupSPtr MDCache::pop() {
  Ts2MarketDataOfSimGroupSPtr ret{nullptr};
  {
    std::lock_guard<std::mutex> guard(mtxMDCache_);
    if (!mdCache_.empty()) {
      ret = mdCache_.front();
      mdCache_.pop_front();
    }
  }
  return ret;
}

void MDCache::cacheMDHis() {
  const auto intervalBetweenCacheCheck =
      CONFIG["simedMode"]["milliSecIntervalBetweenCacheCheck"]
          .as<std::uint32_t>(1);
  const auto numOfCacheMD =
      CONFIG["simedMode"]["numOfCacheMD"].as<std::uint32_t>(5);

  while (keepRunning_.load()) {
    while (true) {
      if (tsStartOfCurCache_ >= tsEnd_) {
        LOG_I("Has been cached to the md of {} in config, stop caching.",
              ConvertTsToPtime(tsEnd_));
        keepRunning_.store(false);
        break;
      }

      if (keepRunning_.load() == false) {
        break;
      }

      std::size_t cacheSize = 0;
      {
        std::lock_guard<std::mutex> guard(mtxMDCache_);
        cacheSize = mdCache_.size();
      }

      if (cacheSize >= numOfCacheMD) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(intervalBetweenCacheCheck));
        break;
      }

      cache1BatchOfHisMD();
    }
  }
}

void MDCache::cache1BatchOfHisMD() {
  auto tsEnd = tsStartOfCurCache_ + secOfCacheMD_ * 1000 * 1000;
  if (tsEnd > tsEnd_) {
    tsEnd = tsEnd_;
  }

  auto mdCacheOfCurBatch = makeMDCacheOfCurBatch(tsStartOfCurCache_, tsEnd);

  calcDelayBetweenAdjacentMD(mdCacheOfCurBatch);

  LOG_I("Cache {} numbers of market data between {} - {} success. ",
        mdCacheOfCurBatch->size(), ConvertTsToPtime(tsStartOfCurCache_),
        ConvertTsToPtime(tsEnd));

  if (!mdCacheOfCurBatch->empty()) {
    std::lock_guard<std::mutex> guard(mtxMDCache_);
    mdCache_.emplace_back(mdCacheOfCurBatch);
  }

  tsStartOfCurCache_ = tsEnd;
}

Ts2MarketDataOfSimGroupSPtr MDCache::makeMDCacheOfCurBatch(
    std::uint64_t startLocalTs, std::uint64_t endLocalTs) {
  auto ret = std::make_shared<Ts2MarketDataOfSimGroup>();
  const auto playbackMD = CONFIG["simedMode"]["playbackMD"].as<std::string>();
  std::string sql;
  if (boost::icontains(playbackMD, "where")) {
    sql = fmt::format("{} AND localts >= {} AND localts < {};", playbackMD,
                      startLocalTs, endLocalTs);
  } else {
    sql = fmt::format("{} WHERE localts >= {} AND localts < {};", playbackMD,
                      startLocalTs, endLocalTs);
  }
  const auto [statusCode, statusMsg, recNum, recSet] =
      tdeng::QueryDataFromTDEng(mdSvc_->getTDEngConnpool(), sql, UINT32_MAX);
  if (statusCode != 0) {
    LOG_W("Make market data of cur batch failed. [{} - {}] {}", statusCode,
          statusMsg, sql);
    return ret;
  }

  Doc doc;
  doc.Parse(recSet.data());
  for (std::size_t i = 0; i < doc["recSet"].Size(); ++i) {
    const auto exchTs = doc["recSet"][i]["exchts"].GetUint64();
    if (exchTs < exchTsStart_ || exchTs >= exchTsEnd_) continue;

    auto rec = std::make_shared<MarketDataOfSim>();
    rec->localTs_ = doc["recSet"][i]["localts"].GetUint64();
    rec->mdType_ =
        magic_enum::enum_cast<MDType>(doc["recSet"][i]["mdtype"].GetUint())
            .value();
    rec->data_ = Base64Decode(doc["recSet"][i]["data"].GetString());
    ret->emplace(rec->localTs_, rec);

    if (keepRunning_.load() == false) {
      return std::make_shared<Ts2MarketDataOfSimGroup>();
    }
  }

  return ret;
}

void MDCache::calcDelayBetweenAdjacentMD(
    Ts2MarketDataOfSimGroupSPtr& mdCacheOfCurBatch) {
  for (auto iter = std::begin(*mdCacheOfCurBatch);
       iter != std::end(*mdCacheOfCurBatch); ++iter) {
    const auto localTs = iter->first;
    auto& marketDataOfSim = iter->second;
    auto nextIter = std::next(iter, 1);
    if (nextIter != std::end(*mdCacheOfCurBatch)) {
      marketDataOfSim->delay_ = nextIter->second->localTs_ - localTs;
    } else {
      marketDataOfSim->delay_ = 0;
    }
  }
}

}  // namespace bq::md::svc
