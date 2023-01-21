/*!
 * \file CommonIPCData.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/11/25
 *
 * \brief
 */

#include "def/CommonIPCData.hpp"

namespace bq {

std::string CommonIPCData::toJson() const {
  std::string ret;
  ret = R"({"shmHeader":)" + shmHeader_.toJson();
  if (dataLen_ != 0) {
    ret = ret + R"(,"data":)" + data_;
  }
  ret = ret + "}";
  return ret;
}

CommonIPCDataSPtr MakeCommonIPCData(const std::string& str) {
  void* buff = malloc(sizeof(CommonIPCData) + str.size() + 1);
  std::shared_ptr<CommonIPCData> ret(static_cast<CommonIPCData*>(buff));
  ret->dataLen_ = str.size();
  memcpy(ret->data_, str.c_str(), str.size());
  *(static_cast<char*>(ret->data_) + ret->dataLen_) = '\0';
  return ret;
}

}  // namespace bq
