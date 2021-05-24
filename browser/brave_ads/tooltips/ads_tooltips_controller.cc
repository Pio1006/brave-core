/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/brave_ads/tooltips/ads_tooltips_controller.h"

#include <memory>
#include <utility>

#include "brave/browser/brave_ads/ads_service_factory.h"
#include "brave/browser/ui/brave_tooltips/brave_tooltip_popup.h"

namespace brave_ads {

AdsTooltipsController::AdsTooltipsController(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

AdsTooltipsController::~AdsTooltipsController() = default;

void AdsTooltipsController::ShowTooltip(
    std::unique_ptr<brave_tooltips::BraveTooltip> tooltip) {
  DCHECK(tooltip);

  // If there's no delegate, set one so that clicks go back to the appropriate
  // handler
  tooltip->set_delegate(AsWeakPtr());

  const std::string tooltip_id = tooltip->id();
  DCHECK(!tooltip_popups_[tooltip_id]);
  tooltip_popups_[tooltip_id] =
      new brave_tooltips::BraveTooltipPopup(profile_, std::move(tooltip));
}

void AdsTooltipsController::CloseTooltip(const std::string& tooltip_id) {
  DCHECK(!tooltip_id.empty());

  if (!tooltip_popups_[tooltip_id]) {
    return;
  }

  auto* popup = tooltip_popups_[tooltip_id];
  if (!popup) {
    return;
  }

  popup->Close(false);
}

void AdsTooltipsController::OnTooltipWidgetDestroyed(
    const std::string& tooltip_id) {
  DCHECK(!tooltip_id.empty());
  tooltip_popups_.erase(tooltip_id);
}

}  // namespace brave_ads
