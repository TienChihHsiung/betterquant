/*!
 * \file TDSvcDef.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "SHMHeader.hpp"
#include "util/Pch.hpp"

namespace bq::td::svc {

struct TDSrvSignal {
  TDSrvSignal(MsgId msgId) : shmHeader_(msgId) {}
  SHMHeader shmHeader_;
};

}  // namespace bq::td::svc
