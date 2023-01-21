/*!
 * \file FeeInfoCache.cpp
 * \project BetterQuant
 *
 * \author byrnexu
 * \date 2022/09/08
 *
 * \brief
 */

#include "util/FeeInfoCache.hpp"

#include "db/DBE.hpp"
#include "db/TBLRecSetMaker.hpp"
#include "def/BQConst.hpp"
#include "def/DataStruOfTD.hpp"
#include "util/Datetime.hpp"

namespace bq {

FeeInfoCache::FeeInfoCache(const db::DBEngSPtr& dbEng)
    : dbEng_(dbEng), identity2FeeInfo_(std::make_shared<Identity2FeeInfo>()) {}

int FeeInfoCache::load(AcctId acctId) {
  auto sql = fmt::format("SELECT * FROM {} WHERE 1=1 ", TBLFeeInfo::TableName);
  if (acctId != 0) {
    sql = sql + fmt::format("AND `acctId` = '{}' ", acctId);
  }

  auto [retOfExec, tblRecSet] =
      db::TBLRecSetMaker<TBLFeeInfo>::ExecSql(dbEng_, sql);
  if (retOfExec != 0) {
    LOG_W("Query fee info failed.");
    return retOfExec;
  }

  std::uint64_t maxUpdateTime{0};
  auto identity2FeeInfo = std::make_shared<Identity2FeeInfo>();
  for (const auto& tblRec : *tblRecSet) {
    const auto rec = tblRec.second->getRecWithAllFields();

    const auto curUpdateTime = ConvertDBTimeToTS(rec->updateTime);
    if (curUpdateTime > maxUpdateTime) maxUpdateTime = curUpdateTime;

    const auto identity = fmt::format("{}{}{}{}{}{}{}{}{}",                  //
                                      rec->acctId, SEP_OF_REC_IDENTITY,      //
                                      rec->side, SEP_OF_REC_IDENTITY,        //
                                      rec->marketCode, SEP_OF_REC_IDENTITY,  //
                                      rec->symbolType, SEP_OF_REC_IDENTITY,  //
                                      rec->symbolCode);
    const auto hash = XXH3_64bits(identity.data(), identity.size());

    const auto feeInfo = std::make_shared<FeeInfo>();
    feeInfo->acctId_ = rec->acctId;
    feeInfo->side_ = rec->side;
    feeInfo->marketCode_ = rec->marketCode;
    feeInfo->symbolType_ = rec->symbolType;
    feeInfo->symbolCode_ = rec->symbolCode;
    feeInfo->commission_ = CONV(Decimal, rec->commission);
    feeInfo->minCommission_ = CONV(Decimal, rec->minCommission);
    feeInfo->stampDuty_ = CONV(Decimal, rec->stampDuty);
    feeInfo->minStampDuty_ = CONV(Decimal, rec->minStampDuty);
    feeInfo->transFee_ = CONV(Decimal, rec->transFee);
    feeInfo->minTransFee_ = CONV(Decimal, rec->minTransFee);
    feeInfo->updateTime_ = ConvertDBTimeToTS(rec->updateTime);

    identity2FeeInfo->emplace(hash, feeInfo);
  }
  LOG_D("Load fee info. [size = {}]", identity2FeeInfo->size());

  if (maxUpdateTime > updateTime_) {
    updateTime_ = maxUpdateTime;
    {
      std::lock_guard<std::ext::spin_mutex> guard(mtxIdentity2FeeInfo_);
      identity2FeeInfo_ = identity2FeeInfo;
    }
  }

  return 0;
}

FeeInfoSPtr FeeInfoCache::getFeeInfo(const OrderInfo* orderInfo) const {
  return getFeeInfo(orderInfo->acctId_, ENUM_TO_STR(orderInfo->side_),
                    GetMarketName(orderInfo->marketCode_),
                    ENUM_TO_STR(orderInfo->symbolType_),
                    orderInfo->symbolCode_);
}

FeeInfoSPtr FeeInfoCache::getFeeInfo(AcctId acctId, const std::string& side,
                                     const std::string& marketCode,
                                     const std::string& symbolType,
                                     const std::string& symbolCode) const {
  const auto identity = fmt::format("{}{}{}{}{}{}{}{}{}",             //
                                    acctId, SEP_OF_REC_IDENTITY,      //
                                    side, SEP_OF_REC_IDENTITY,        //
                                    marketCode, SEP_OF_REC_IDENTITY,  //
                                    symbolType, SEP_OF_REC_IDENTITY,  //
                                    symbolCode);
  const auto hash = XXH3_64bits(identity.data(), identity.size());
  {
    const auto iter = identity2FeeInfo_->find(hash);
    if (iter != std::end(*identity2FeeInfo_)) {
      return iter->second;
    }
  }

  const auto identityWithEmptySymbol =
      fmt::format("{}{}{}{}{}{}{}{}{}",             //
                  acctId, SEP_OF_REC_IDENTITY,      //
                  side, SEP_OF_REC_IDENTITY,        //
                  marketCode, SEP_OF_REC_IDENTITY,  //
                  symbolType, SEP_OF_REC_IDENTITY,  //
                  "");
  const auto hashWithEmptySymbol = XXH3_64bits(identityWithEmptySymbol.data(),
                                               identityWithEmptySymbol.size());
  {
    const auto iter = identity2FeeInfo_->find(hashWithEmptySymbol);
    if (iter != std::end(*identity2FeeInfo_)) {
      return iter->second;
    }
  }

  return nullptr;
}

Decimal FeeInfoCache::calcFee(const OrderInfo* orderInfo,
                              Decimal dealAmt) const {
  const auto feeInfo = getFeeInfo(orderInfo);
  if (feeInfo == nullptr) {
    LOG_W("Calc fee failed because of get fee info failed. {}",
          orderInfo->toShortStr());
    return 0;
  }

  if (dealAmt == 0) {
    dealAmt = orderInfo->dealSize_ * orderInfo->avgDealPrice_;
  }
  if (dealAmt < 0) dealAmt *= -1;

  auto commission = dealAmt * feeInfo->commission_;
  if (commission < feeInfo->minCommission_) {
    commission = feeInfo->minCommission_;
  }

  auto stampDuty = dealAmt * feeInfo->stampDuty_;
  if (stampDuty < feeInfo->minStampDuty_) {
    stampDuty = feeInfo->minStampDuty_;
  }

  auto transFee = dealAmt * feeInfo->transFee_;
  if (transFee < feeInfo->minTransFee_) {
    transFee = feeInfo->minTransFee_;
  }

  const auto ret = commission + stampDuty + transFee;
  return ret;
}

}  // namespace bq
