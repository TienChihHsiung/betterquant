/*!
 * \file RawTDHandler.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/02
 *
 * \brief
 */

#pragma once

#include "def/BQTDDef.hpp"
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

struct RawTD;
using RawTDSPtr = std::shared_ptr<RawTD>;
using RawTDAsyncTask = AsyncTask<RawTDSPtr>;
using RawTDAsyncTaskSPtr = std::shared_ptr<RawTDAsyncTask>;
}  // namespace bq

namespace bq::td::svc {

class TDSvcOfCN;

class RawTDHandler {
 public:
  RawTDHandler(const RawTDHandler&) = delete;
  RawTDHandler& operator=(const RawTDHandler&) = delete;
  RawTDHandler(const RawTDHandler&&) = delete;
  RawTDHandler& operator=(const RawTDHandler&&) = delete;

  explicit RawTDHandler(TDSvcOfCN const* tdSvc);
  ~RawTDHandler() {}

 public:
  int init();

 private:
  virtual std::tuple<int, RawTDAsyncTaskSPtr> makeAsyncTask(
      const RawTDSPtr& task) = 0;

 public:
  void start();
  void stop();

 public:
  void dispatch(RawTDSPtr& task);

 private:
  void handle(RawTDAsyncTaskSPtr& asyncTask);

 private:
  void handleOrder(RawTDAsyncTaskSPtr& asyncTask);
  void handleSyncUnclosedOrder(RawTDAsyncTaskSPtr& asyncTask);
  void handleSyncAssetsSnapshot(RawTDAsyncTaskSPtr& asyncTask);

 protected:
  TDSvcOfCN const* tdSvc_{nullptr};
  TaskDispatcherSPtr<RawTDSPtr> taskDispatcher_{nullptr};
};

}  // namespace bq::td::svc
