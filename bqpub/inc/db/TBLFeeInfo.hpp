/*!
 * \file TBLFeeInfo.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#pragma once

#include "def/BQDef.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq::db::feeInfo {

struct FieldGroupOfKey {
  AcctId acctId;
  std::string side;
  std::string marketCode;
  std::string symbolType;
  std::string symbolCode;

  JSER(FieldGroupOfKey, acctId, side, marketCode, symbolType, symbolCode)
};

struct FieldGroupOfVal {
  std::string commission;
  std::string minCommission;
  std::string stampDuty;
  std::string minStampDuty;
  std::string transFee;
  std::string minTransFee;
  std::string updateTime;

  JSER(FieldGroupOfVal, commission, minCommission, stampDuty, minStampDuty,
       transFee, minTransFee, updateTime)
};

struct FieldGroupOfAll {
  AcctId acctId;
  std::string side;
  std::string marketCode;
  std::string symbolType;
  std::string symbolCode;

  std::string commission;
  std::string minCommission;
  std::string stampDuty;
  std::string minStampDuty;
  std::string transFee;
  std::string minTransFee;
  std::string updateTime;
  ;

  JSER(FieldGroupOfAll, acctId, side, marketCode, symbolType, symbolCode,
       commission, minCommission, stampDuty, minStampDuty, transFee,
       minTransFee, updateTime)
};

struct TableSchema {
  inline const static std::string TableName = "`BetterQuant`.`feeInfo`";
  using KeyFields = FieldGroupOfKey;
  using ValFields = FieldGroupOfVal;
  using AllFields = FieldGroupOfAll;
};

using Record = FieldGroupOfAll;
using RecordSPtr = std::shared_ptr<Record>;
using RecordWPtr = std::weak_ptr<Record>;

}  // namespace bq::db::feeInfo

using TBLFeeInfo = bq::db::feeInfo::TableSchema;
