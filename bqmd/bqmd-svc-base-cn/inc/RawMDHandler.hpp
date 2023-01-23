/*!
 * \file RawMDHandler.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/11/16
 *
 * \brief
 */

#pragma once

#include "MDSvcOfCNDef.hpp"
#include "def/BQMDDef.hpp"
#include "util/Pch.hpp"

namespace bq {
template <typename Task>
class TaskDispatcher;
template <typename Task>
using TaskDispatcherSPtr = std::shared_ptr<TaskDispatcher<Task>>;
template <typename Task>
struct AsyncTask;
template <typename Task>
using AsyncTaskSPtr = std::shared_ptr<AsyncTask<Task>>;

struct RawMD;
using RawMDSPtr = std::shared_ptr<RawMD>;
using RawMDAsyncTask = AsyncTask<RawMDSPtr>;
using RawMDAsyncTaskSPtr = std::shared_ptr<RawMDAsyncTask>;

enum class MsgType : std::uint8_t;
}  // namespace bq

namespace bq::md {
struct RawMDAsyncTaskArg;
using RawMDAsyncTaskArgSPtr = std::shared_ptr<RawMDAsyncTaskArg>;
}  // namespace bq::md

namespace bq::md::svc {

class MDSvcOfCN;

class RawMDHandler {
 public:
  RawMDHandler(const RawMDHandler&) = delete;
  RawMDHandler& operator=(const RawMDHandler&) = delete;
  RawMDHandler(const RawMDHandler&&) = delete;
  RawMDHandler& operator=(const RawMDHandler&&) = delete;

  explicit RawMDHandler(MDSvcOfCN const* mdSvc);
  ~RawMDHandler() {}

 public:
  int init();

 public:
  virtual RawMDSPtr makeRawMD(MsgType msgType, const void* data,
                              std::size_t dataLen) = 0;

 private:
  virtual std::tuple<int, RawMDAsyncTaskSPtr> makeAsyncTask(
      const RawMDSPtr& task) = 0;

 public:
  void start();
  void stop();

 public:
  void dispatch(RawMDSPtr& task);

 private:
  void handle(RawMDAsyncTaskSPtr& asyncTask);

  virtual void handleNewSymbol(RawMDAsyncTaskSPtr& asyncTask) = 0;
  virtual bool handleMDTickers(RawMDAsyncTaskSPtr& asyncTask) = 0;
  virtual bool handleMDTrades(RawMDAsyncTaskSPtr& asyncTask) = 0;
  virtual bool handleMDOrders(RawMDAsyncTaskSPtr& asyncTask) = 0;
  virtual bool handleMDBooks(RawMDAsyncTaskSPtr& asyncTask) = 0;

 protected:
  bool checkIfMDIsLegal(RawMDAsyncTaskSPtr& asyncTask, std::uint64_t exchTs);

 protected:
  MDSvcOfCN const* mdSvc_{nullptr};

  Topic2LastTsGroupSPtr topic2LastExchTsGroup_{nullptr};
  mutable std::mutex mtxTopic2LastExchTsGroup_;

  TaskDispatcherSPtr<RawMDSPtr> taskDispatcher_{nullptr};

 private:
  bool saveMarketData_{false};
  bool checkIFExchTsOfMDIsInc_{true};
};

}  // namespace bq::md::svc
