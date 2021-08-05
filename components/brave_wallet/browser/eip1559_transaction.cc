/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/eip1559_transaction.h"

#include <algorithm>
#include <utility>

#include "base/values.h"
#include "brave/components/brave_wallet/browser/brave_wallet_utils.h"
#include "brave/components/brave_wallet/browser/rlp_encode.h"

namespace brave_wallet {

Eip1559Transaction::Eip1559Transaction() = default;
Eip1559Transaction::Eip1559Transaction(const TxData& tx_data,
                                       uint64_t chain_id,
                                       uint256_t max_priority_fee_per_gas,
                                       uint256_t max_fee_per_gas)
    : Eip2930Transaction(tx_data, chain_id),
      max_priority_fee_per_gas_(max_priority_fee_per_gas),
      max_fee_per_gas_(max_fee_per_gas) {
  type_ = 2;
}

Eip1559Transaction::~Eip1559Transaction() = default;

bool Eip1559Transaction::operator==(const Eip1559Transaction& tx) const {
  return Eip2930Transaction::operator==(tx) &&
         max_priority_fee_per_gas_ == tx.max_priority_fee_per_gas_ &&
         max_fee_per_gas_ == tx.max_fee_per_gas_;
}

// static
std::unique_ptr<Eip1559Transaction> Eip1559Transaction::FromValue(
    const base::Value& value) {
  auto tx_2930 = Eip2930Transaction::FromValue(value);
  if (!tx_2930)
    return nullptr;
  TxData tx_data(tx_2930->nonce(), tx_2930->gas_price(), tx_2930->gas_limit(),
                 tx_2930->to(), tx_2930->value(), tx_2930->data());

  const std::string* tx_max_priority_fee_per_gas =
      value.FindStringKey("max_priority_fee_per_gas");
  if (!tx_max_priority_fee_per_gas)
    return nullptr;
  uint256_t max_priority_fee_per_gas;
  if (!HexValueToUint256(*tx_max_priority_fee_per_gas,
                         &max_priority_fee_per_gas))
    return nullptr;

  const std::string* tx_max_fee_per_gas =
      value.FindStringKey("max_fee_per_gas");
  if (!tx_max_fee_per_gas)
    return nullptr;
  uint256_t max_fee_per_gas;
  if (!HexValueToUint256(*tx_max_fee_per_gas, &max_fee_per_gas))
    return nullptr;

  auto tx = std::make_unique<Eip1559Transaction>(
      tx_data, tx_2930->chain_id(), max_priority_fee_per_gas, max_fee_per_gas);
  tx->v_ = tx_2930->v();
  tx->r_ = tx_2930->r();
  tx->s_ = tx_2930->s();

  return tx;
}

void Eip1559Transaction::GetMessageToSign(uint64_t chain_id,
                                          GetMessageToSignCallback callback) {
  std::vector<uint8_t> result;
  result.push_back(type_);

  // TODO(darkdh): Migrate to std::vector<base::Value>, base::ListValue is
  // deprecated
  base::ListValue list;
  list.Append(RLPUint256ToBlobValue(chain_id_));
  list.Append(RLPUint256ToBlobValue(nonce_));
  list.Append(RLPUint256ToBlobValue(max_priority_fee_per_gas_));
  list.Append(RLPUint256ToBlobValue(max_fee_per_gas_));
  list.Append(RLPUint256ToBlobValue(gas_limit_));
  list.Append(base::Value(to_.bytes()));
  list.Append(RLPUint256ToBlobValue(value_));
  list.Append(base::Value(data_));
  list.Append(base::Value(AccessListToValue(access_list_)));

  const std::string rlp_msg = RLPEncode(std::move(list));
  result.insert(result.end(), rlp_msg.begin(), rlp_msg.end());
  std::move(callback).Run(KeccakHash(result));
}

void Eip1559Transaction::GetSignedTransaction(
    GetSignedTransactionCallback callback) {
  DCHECK(IsSigned());

  // TODO(darkdh): Migrate to std::vector<base::Value>, base::ListValue is
  // deprecated
  base::ListValue list;
  list.Append(RLPUint256ToBlobValue(chain_id_));
  list.Append(RLPUint256ToBlobValue(nonce_));
  list.Append(RLPUint256ToBlobValue(max_priority_fee_per_gas_));
  list.Append(RLPUint256ToBlobValue(max_fee_per_gas_));
  list.Append(RLPUint256ToBlobValue(gas_limit_));
  list.Append(base::Value(to_.bytes()));
  list.Append(RLPUint256ToBlobValue(value_));
  list.Append(base::Value(data_));
  list.Append(base::Value(AccessListToValue(access_list_)));
  list.Append(base::Value(v_));
  list.Append(base::Value(r_));
  list.Append(base::Value(s_));

  std::vector<uint8_t> result;
  result.push_back(type_);

  const std::string rlp_msg = RLPEncode(std::move(list));
  result.insert(result.end(), rlp_msg.begin(), rlp_msg.end());

  std::move(callback).Run(ToHex(result));
}

base::Value Eip1559Transaction::ToValue() const {
  base::Value tx = Eip2930Transaction::ToValue();

  tx.SetStringKey("max_priority_fee_per_gas",
                  Uint256ValueToHex(max_priority_fee_per_gas_));
  tx.SetStringKey("max_fee_per_gas", Uint256ValueToHex(max_fee_per_gas_));

  return tx;
}

uint256_t Eip1559Transaction::GetUpfrontCost(uint256_t block_base_fee) const {
  uint256_t inclusion_fee_per_gas =
      std::min(max_priority_fee_per_gas_, max_fee_per_gas_ - block_base_fee);
  uint256_t gas_price = inclusion_fee_per_gas + block_base_fee;

  return gas_limit_ * gas_price + value_;
}

}  // namespace brave_wallet
