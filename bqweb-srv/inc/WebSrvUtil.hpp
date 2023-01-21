/*!
 * \file WebSrvUtil.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/10
 *
 * \brief
 */

#pragma once

#include "util/Pch.hpp"

namespace bq {

std::string MakeCommonHttpBody(int statusCode, std::string data = "");

}
