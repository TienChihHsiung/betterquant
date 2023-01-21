/*!
 * \file TDEngUtil.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/29
 *
 * \brief
 */

#pragma once

#include "util/Pch.hpp"

#ifdef __cplusplus
extern "C" {
#endif
using TAOS_RES = void;
#ifdef __cplusplus
}
#endif

namespace bq::tdeng {

std::tuple<int, std::uint32_t, std::string> GetJsonDataFromRes(
    TAOS_RES *res, std::uint32_t maxRecNum);

}
