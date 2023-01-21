/*!
 * \file Main.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/12/20
 *
 * \brief
 */

#include "MDSvcOfXTP.hpp"
#include "config-proj.hpp"
#include "util/BQUtil.hpp"
#include "util/Pch.hpp"
#include "util/ProgOpt.hpp"

int main(int argc, char** argv) {
  bq::GFlagsHolder gflagsHolder(argc, argv, PROJ_VER, "--conf=filename");
  bq::PrintLogo();

  const auto svc =
      std::make_shared<bq::md::svc::xtp::MDSvcOfXTP>(bq::FLAGS_conf);

  if (const auto ret = svc->init(); ret != 0) {
    return EXIT_FAILURE;
  }

  if (const auto ret = svc->run(); ret != 0) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
