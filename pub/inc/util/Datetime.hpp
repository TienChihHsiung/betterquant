#pragma once

#include "util/Pch.hpp"

namespace bq {

std::uint64_t GetTotalNSSince1970();
std::uint64_t GetTotalUSSince1970();
std::uint64_t GetTotalMSSince1970();
std::uint64_t GetTotalSecSince1970();

std::string ConvertTsToDBTime(std::uint64_t ts);
std::string ConvertTsToPtime(std::uint64_t ts);
std::uint64_t ConvertDBTimeToTS(std::string dbTime);
std::tuple<int, std::uint64_t> ConvertISODatetimeToTs(
    const std::string& isoDatetime);

}  // namespace bq