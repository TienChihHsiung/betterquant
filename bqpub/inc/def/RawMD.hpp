/*!
 * \file RawMD.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/22
 *
 * \brief
 */

#pragma once

#include "def/BQConst.hpp"
#include "def/DefIF.hpp"
#include "util/Datetime.hpp"
#include "util/Pch.hpp"

namespace bq {

template <typename Task>
struct AsyncTask;

struct RawMD {
  RawMD(MsgType msgType, void* data, std::uint32_t dataLen) {
    msgType_ = msgType;
    localTs_ = GetTotalUSSince1970();

    dataLen_ = dataLen;
    data_ = malloc(dataLen);
    memcpy(data_, data, dataLen_);

    switch (msgType) {
      case MsgType::Tickers:
        mdType_ = MDType::Tickers;
        break;
      case MsgType::Trades:
        mdType_ = MDType::Trades;
        break;
      case MsgType::Orders:
        mdType_ = MDType::Orders;
        break;
      case MsgType::Books:
        mdType_ = MDType::Books;
        break;
      case MsgType::Candle:
        mdType_ = MDType::Candle;
        break;
      default:
        mdType_ = MDType::Others;
        break;
    }
  }

  ~RawMD() {
    SAFE_FREE(data_);
    SAFE_FREE(dataAfterConv_);
  }

  MsgType msgType_;
  std::uint64_t localTs_;

  std::uint32_t dataLen_{0};
  void* data_{nullptr};

  MDType mdType_;
  MarketCode marketCode_;
  SymbolType symbolType_;
  std::string symbolCode_;

  std::string topic_;
  TopicHash topicHash_;

  std::uint32_t dataAfterConvLen_{0};
  void* dataAfterConv_{nullptr};
};
using RawMDSPtr = std::shared_ptr<RawMD>;

using RawMDAsyncTask = AsyncTask<RawMDSPtr>;
using RawMDAsyncTaskSPtr = std::shared_ptr<RawMDAsyncTask>;

void InitTopicInfo(RawMDSPtr& rawMD);

}  // namespace bq
