/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/eligible_ads/ad_notifications/ad_features_info.h"

namespace ads {
namespace ad_notifications {
// TODO(Moritz Haller): rename "feature" as collision with base::feature
AdFeaturesInfo::AdFeaturesInfo() = default;

AdFeaturesInfo::AdFeaturesInfo(const AdFeaturesInfo& info) = default;

AdFeaturesInfo::~AdFeaturesInfo() = default;

}  // namespace ad_notifications
}  // namespace ads
