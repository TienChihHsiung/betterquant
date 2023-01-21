/*!
 * \file bq_v1_QueryHisMD.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/11/25
 *
 * \brief
 */

#pragma once

#include <drogon/HttpController.h>

#include "def/BQDef.hpp"

using namespace drogon;

namespace bq {
namespace v1 {

class QueryHisMD : public drogon::HttpController<QueryHisMD> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(QueryHisMD::queryBetween2Ts,
                "/v1/QueryHisMD/between/{marketCode}/{symbolType}/{symbolCode}/"
                "{mdType}?tsBegin={tsBegin}&tsEnd={tsEnd}&freq={freq}",
                Get);

  ADD_METHOD_TO(QueryHisMD::queryBeforeTs,
                "/v1/QueryHisMD/before/{marketCode}/{symbolType}/{symbolCode}/"
                "{mdType}?ts={ts}&num={num}&freq={freq}",
                Get);

  ADD_METHOD_TO(QueryHisMD::queryAfterTs,
                "/v1/QueryHisMD/after/{marketCode}/{symbolType}/{symbolCode}/"
                "{mdType}?ts={ts}&num={num}&freq={freq}",
                Get);

  METHOD_LIST_END

  void queryBetween2Ts(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       std::string &&marketCode, std::string &&symbolType,
                       std::string &&symbolCode, std::string &&mdType,
                       std::uint64_t tsBegin, std::uint64_t tsEnd,
                       std::string &&freq);

  void queryBeforeTs(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     std::string &&marketCode, std::string &&symbolType,
                     std::string &&symbolCode, std::string &&mdType,
                     std::uint64_t ts, int num, std::string &&freq) const;

  void queryAfterTs(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    std::string &&marketCode, std::string &&symbolType,
                    std::string &&symbolCode, std::string &&mdType,
                    std::uint64_t ts, int num, std::string &&freq) const;

 private:
  std::tuple<int, std::string, int, std::string> queryDataFromTDEng(
      const std::string &sql, std::uint32_t maxRecNum) const;
  std::string makeBody(int statusCode, const std::string &statusMsg,
                       std::string recSet) const;

  HttpResponsePtr makeHttpResponse(const std::string &body) const;
};

}  // namespace v1
}  // namespace bq
