/*!
 * \file WebSrvUtil.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#include "WebSrvUtil.hpp"

#include "def/StatusCode.hpp"

namespace bq {

std::string MakeCommonHttpBody(int statusCode, std::string data) {
  const auto statusMsg = GetStatusMsg(statusCode);
  auto ret = fmt::format(R"({{"statusCode":{},"stausMsg":"{}")", statusCode,
                         statusMsg);
  if (!data.empty()) {
    data[0] = ',';
  } else {
    data = "}";
  }
  ret = ret + data;
  return ret;
}

}  // namespace bq
