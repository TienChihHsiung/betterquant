/*!
 * \file TBLFlowCtrlRuleCache.hpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2023/01/11
 *
 * \brief
 */

#pragma once

#include "def/BQDef.hpp"
#include "def/Def.hpp"
#include "util/Pch.hpp"

namespace bq::db::flowCtrlRuleCache {

struct FieldGroupOfKey {
  std::string step;
  std::string target;
  std::string condition;
  std::string conditionValue;
  JSER(FieldGroupOfKey, step, target, condition, conditionValue)
};

struct FieldGroupOfVal {
  std::string data;
  std::string name;
  JSER(FieldGroupOfVal, data, name)
};

struct FieldGroupOfAll {
  std::string step;
  std::string target;
  std::string condition;
  std::string conditionValue;
  std::string data;
  std::uint32_t no;
  std::string name;

  std::string getKey() const {
    const auto ret = fmt::format("{}{}{}{}{}{}{}", step, SEP_OF_REC_IDENTITY,
                                 target, SEP_OF_REC_IDENTITY, condition,
                                 SEP_OF_REC_IDENTITY, conditionValue);
    return ret;
  }

  JSER(FieldGroupOfAll, step, target, condition, conditionValue, data, no, name)
};

struct TableSchema {
  inline const static std::string TableName =
      "`BetterQuant`.`flowCtrlRuleCache`";
  using KeyFields = FieldGroupOfKey;
  using ValFields = FieldGroupOfVal;
  using AllFields = FieldGroupOfAll;
};

using Record = FieldGroupOfAll;
using RecordSPtr = std::shared_ptr<Record>;
using RecordWPtr = std::weak_ptr<Record>;

}  // namespace bq::db::flowCtrlRuleCache

using TBLFlowCtrlRuleCache = bq::db::flowCtrlRuleCache::TableSchema;
