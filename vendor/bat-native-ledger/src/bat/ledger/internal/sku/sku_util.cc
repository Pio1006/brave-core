/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <utility>

#include "bat/ledger/global_constants.h"
#include "bat/ledger/internal/sku/sku_util.h"
#include "bat/ledger/ledger.h"

namespace ledger {
namespace sku {

const char kAnonCardDestinationDev[] =
    "9094c3f2-b3ae-438f-bd59-92aaad92de5c";
const char kAnonCardDestinationStaging[] =
    "6654ecb0-6079-4f6c-ba58-791cc890a561";
const char kAnonCardDestinationProduction[] =
    "86f26f49-9d3b-4f97-9b56-d305ad7a856f";

const char kUpholdDestinationDev[] =
    "9094c3f2-b3ae-438f-bd59-92aaad92de5c";
const char kUpholdDestinationStaging[] =
    "6654ecb0-6079-4f6c-ba58-791cc890a561";
const char kUpholdDestinationProduction[] =
    "5d4be2ad-1c65-4802-bea1-e0f3a3a487cb";

// To-Do(@jumde) - Set the settlement address for production  when available
const char kGeminiDestinationDev[] = "60e5e863-8c3d-4341-8b54-23e2695a490c";
const char kGeminiDestinationStaging[] = "60e5e863-8c3d-4341-8b54-23e2695a490c";
const char kGeminiDestinationProduction[] = "";

std::string GetBraveDestination(const std::string& wallet_type) {
  if (wallet_type == constant::kWalletUphold) {
    return GetUpholdDestination();
  }

  if (wallet_type == constant::kWalletGemini) {
    return GetGeminiDestination();
  }

  if (wallet_type == constant::kWalletAnonymous) {
    return GetAnonCardDestination();
  }

  NOTREACHED();
  return "";
}

std::string GetAnonCardDestination() {
  if (ledger::_environment == type::Environment::PRODUCTION) {
    return kAnonCardDestinationProduction;
  }

  if (ledger::_environment == type::Environment::STAGING) {
    return kAnonCardDestinationStaging;
  }

  if (ledger::_environment == type::Environment::DEVELOPMENT) {
    return kAnonCardDestinationDev;
  }

  NOTREACHED();
  return kAnonCardDestinationDev;
}

std::string GetUpholdDestination() {
  if (ledger::_environment == type::Environment::PRODUCTION) {
    return kUpholdDestinationProduction;
  }

  if (ledger::_environment == type::Environment::STAGING) {
    return kUpholdDestinationStaging;
  }

  if (ledger::_environment == type::Environment::DEVELOPMENT) {
    return kUpholdDestinationDev;
  }

  NOTREACHED();
  return kUpholdDestinationDev;
}

std::string GetGeminiDestination() {
  if (ledger::_environment == type::Environment::PRODUCTION) {
    return kGeminiDestinationProduction;
  }

  if (ledger::_environment == type::Environment::STAGING) {
    return kGeminiDestinationStaging;
  }

  if (ledger::_environment == type::Environment::DEVELOPMENT) {
    return kGeminiDestinationDev;
  }

  NOTREACHED();
  return kGeminiDestinationDev;
}

}  // namespace sku
}  // namespace ledger
