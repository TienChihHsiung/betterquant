/*!
 * \file Main.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "TDSrv.hpp"
#include "config-proj.hpp"
#include "def/MarketDataIF.hpp"
#include "util/BQUtil.hpp"
#include "util/Pch.hpp"
#include "util/ProgOpt.hpp"

using namespace bq::td::srv;

int main(int argc, char** argv) {
  bq::GFlagsHolder gflagsHolder(argc, argv, PROJ_VER, "--conf=filename");
  bq::PrintLogo();

  const auto svc = std::make_shared<bq::td::srv::TDSrv>(bq::FLAGS_conf);

  if (const auto ret = svc->init(); ret != 0) {
    return EXIT_FAILURE;
  }

  if (const auto ret = svc->run(); ret != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
