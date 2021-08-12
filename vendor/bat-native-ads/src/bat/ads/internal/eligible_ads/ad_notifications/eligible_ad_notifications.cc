/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/eligible_ads/ad_notifications/eligible_ad_notifications.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/rand_util.h"
#include "base/time/time.h"
#include "bat/ads/ad_notification_info.h"
#include "bat/ads/internal/ad_pacing/ad_pacing.h"
#include "bat/ads/internal/ad_priority/ad_priority.h"
#include "bat/ads/internal/ad_serving/ad_targeting/geographic/subdivision/subdivision_targeting.h"
#include "bat/ads/internal/ad_targeting/ad_targeting_segment_util.h"
#include "bat/ads/internal/ad_targeting/ad_targeting_values.h"
#include "bat/ads/internal/ads/ad_notifications/ad_notification_exclusion_rules.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/client/client.h"
#include "bat/ads/internal/database/tables/ad_events_database_table.h"
#include "bat/ads/internal/database/tables/creative_ad_notifications_database_table.h"
#include "bat/ads/internal/eligible_ads/ad_notifications/ad_features_info.h"
#include "bat/ads/internal/eligible_ads/eligible_ads_features.h"
#include "bat/ads/internal/eligible_ads/seen_ads.h"
#include "bat/ads/internal/eligible_ads/seen_advertisers.h"
#include "bat/ads/internal/features/ad_serving/ad_serving_features.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/resources/frequency_capping/anti_targeting_resource.h"

namespace ads {
namespace ad_notifications {

namespace {

const int kMatchesIntentChildSegmentWeight = 0;
const int kMatchesIntentParentSegmentWeight = 1;
const int kMatchesInterestChildSegmentWeight = 2;
const int kMatchesInterestParentSegmentWeight = 3;
const int kAdLastSeenInHoursWeight = 4;
const int kAdvertiserLastSeenInHoursWeight = 5;
const int kPriorityWeight = 6;

bool ShouldCapLastServedAd(const CreativeAdNotificationList& ads) {
  return ads.size() != 1;
}

std::map<std::string, AdFeaturesInfo> GroupEligibleAdsByCreativeInstanceId(
    const CreativeAdNotificationList& eligible_ads) {
  std::map<std::string, AdFeaturesInfo> ads;
  for (const auto& eligible_ad : eligible_ads) {
    const auto iter = ads.find(eligible_ad.creative_instance_id);
    if (iter != ads.end()) {
      iter->second.segments.push_back(eligible_ad.segment);
      continue;
    }

    AdFeaturesInfo ad_features;
    ad_features.segments = {eligible_ad.segment};
    ad_features.creative_ad_notification = eligible_ad;
    ads.insert({eligible_ad.creative_instance_id, ad_features});
  }

  return ads;
}

bool CalculateMatchesSegmentFeature(const SegmentList& user_segments,
                                    const SegmentList& ad_segments) {
  std::vector<std::string> v1 = user_segments;
  std::vector<std::string> v2 = ad_segments;

  std::sort(v1.begin(), v1.end());
  std::sort(v2.begin(), v2.end());

  std::vector<std::string> intersection;
  std::set_intersection(v1.begin(), v1.end(), v2.begin(), v2.end(),
                        std::back_inserter(intersection));

  if (intersection.size() > 0) {
    return true;
  }

  return false;
}

unsigned int CalculateAdLastSeenInHoursFeature(
    const AdEventList& ad_events,
    const std::string& creative_instance_id) {
  // ad_events are ordered by timestamp desc in DB
  const auto iter = std::find_if(
      ad_events.begin(), ad_events.end(),
      [&creative_instance_id](const AdEventInfo& ad_event) -> bool {
        return (ad_event.creative_instance_id == creative_instance_id &&
                ad_event.confirmation_type == ConfirmationType::kViewed);
      });

  if (iter == ad_events.end()) {
    return 0;
  }

  const base::Time now = base::Time::Now();
  const base::Time timestamp = base::Time::FromDoubleT(iter->timestamp);
  return (now - timestamp).InHours();
}

unsigned int CalculateAdvertiserLastSeenInHoursFeature(
    const AdEventList& ad_events,
    const std::string& advertiser_id) {
  // ad_events are ordered by timestamp desc in DB
  const auto iter = std::find_if(
      ad_events.begin(), ad_events.end(),
      [&advertiser_id](const AdEventInfo& ad_event) -> bool {
        return (ad_event.advertiser_id == advertiser_id &&
                ad_event.confirmation_type == ConfirmationType::kViewed);
      });

  if (iter == ad_events.end()) {
    return 0;
  }

  const base::Time now = base::Time::Now();
  const base::Time timestamp = base::Time::FromDoubleT(iter->timestamp);
  return (now - timestamp).InHours();
}

double CalculateScore(const AdFeaturesInfo& ad_features) {
  // TODO(Moritz Haller) Explain matching and weights
  // currently seven features
  // weights are non-negative reals, where sum_weights > 0 (see test)
  std::vector<double> weights = features::GetAdFeatureWeights();

  double score = 0.0;

  if (ad_features.matches_intent_child_segment) {
    score += weights.at(kMatchesIntentChildSegmentWeight);
  } else if (ad_features.matches_intent_parent_segment) {
    score += weights.at(kMatchesIntentParentSegmentWeight);
  }

  if (ad_features.matches_interest_child_segment) {
    score += weights.at(kMatchesInterestChildSegmentWeight);
  } else if (ad_features.matches_interest_parent_segment) {
    score += weights.at(kMatchesInterestParentSegmentWeight);
  }

  if (ad_features.ad_last_seen_in_hours <= base::Time::kHoursPerDay) {
    score += weights.at(kAdLastSeenInHoursWeight) *
             ad_features.ad_last_seen_in_hours / base::Time::kHoursPerDay;
  }

  if (ad_features.advertiser_last_seen_in_hours <= base::Time::kHoursPerDay) {
    score += weights.at(kAdvertiserLastSeenInHoursWeight) *
             ad_features.advertiser_last_seen_in_hours /
             base::Time::kHoursPerDay;
  }

  score += weights.at(kPriorityWeight) /
           ad_features.creative_ad_notification.priority;

  score *= ad_features.creative_ad_notification.ptr;

  return score;
}

double CalculateNormalisingConstant(
    const std::map<std::string, AdFeaturesInfo>& ads) {
  double normalising_constant = 0.0;
  for (const auto& ad : ads) {
    normalising_constant += ad.second.score;
  }

  return normalising_constant;
}

absl::optional<CreativeAdNotificationInfo> SampleFromAds(
    const std::map<std::string, AdFeaturesInfo>& ads) {
  double normalising_constant = CalculateNormalisingConstant(ads);

  const double rand = base::RandDouble();
  double sum = 0;
  CreativeAdNotificationInfo selected_ad;

  for (const auto& ad : ads) {
    double probability = ad.second.score / normalising_constant;
    sum += probability;
    if (rand < sum) {
      return ad.second.creative_ad_notification;
    }
  }

  return absl::nullopt;
}

}  // namespace

EligibleAds::EligibleAds(
    ad_targeting::geographic::SubdivisionTargeting* subdivision_targeting,
    resource::AntiTargeting* anti_targeting_resource)
    : subdivision_targeting_(subdivision_targeting),
      anti_targeting_resource_(anti_targeting_resource) {
  DCHECK(subdivision_targeting_);
  DCHECK(anti_targeting_resource_);
}

EligibleAds::~EligibleAds() = default;

void EligibleAds::SetLastServedAd(const CreativeAdInfo& creative_ad) {
  last_served_creative_ad_ = creative_ad;
}

void EligibleAds::GetForSegments(const SegmentList& segments,
                                 GetEligibleAdsCallback callback) {
  database::table::AdEvents database_table;
  database_table.GetAll([=](const Result result, const AdEventList& ad_events) {
    if (result != Result::SUCCESS) {
      BLOG(1, "Failed to get ad events");
      callback(/* was_allowed */ false, {});
      return;
    }

    const int max_count = features::GetBrowsingHistoryMaxCount();
    const int days_ago = features::GetBrowsingHistoryDaysAgo();
    AdsClientHelper::Get()->GetBrowsingHistory(
        max_count, days_ago, [=](const BrowsingHistoryList& history) {
          if (segments.empty()) {
            GetForUntargeted(ad_events, history, callback);
            return;
          }

          GetForParentChildSegments(segments, ad_events, history, callback);
        });
  });
}

void EligibleAds::GetForFeatures(const SegmentList& interest_segments,
                                 const SegmentList& intent_segments,
                                 SelectAdCallback callback) {
  database::table::AdEvents database_table;
  database_table.GetAll([=](const Result result, const AdEventList& ad_events) {
    if (result != Result::SUCCESS) {
      BLOG(1, "Failed to get ad events");
      callback(/* was_allowed */ false, absl::nullopt);
      return;
    }

    const int max_count = features::GetBrowsingHistoryMaxCount();
    const int days_ago = features::GetBrowsingHistoryDaysAgo();
    AdsClientHelper::Get()->GetBrowsingHistory(
        max_count, days_ago, [=](const BrowsingHistoryList& history) {
          GetEligibleAds(interest_segments, intent_segments, ad_events, history,
                         callback);
        });
  });
}

///////////////////////////////////////////////////////////////////////////////

void EligibleAds::GetEligibleAds(const SegmentList& interest_segments,
                                 const SegmentList& intent_segments,
                                 const AdEventList& ad_events,
                                 const BrowsingHistoryList& browsing_history,
                                 SelectAdCallback callback) const {
  BLOG(1, "Get eligible ads");

  database::table::CreativeAdNotifications database_table;
  database_table.GetAll([=](const Result result, const SegmentList& segments,
                            const CreativeAdNotificationList& ads) {
    if (ads.empty()) {
      BLOG(1, "No ads");
      callback(/* was_allowed */ true, absl::nullopt);
      return;
    }

    CreativeAdNotificationList eligible_ads = ApplyFrequencyCapping(
        ads,
        ShouldCapLastServedAd(ads) ? last_served_creative_ad_
                                   : CreativeAdInfo(),
        ad_events, browsing_history);

    if (eligible_ads.empty()) {
      BLOG(1, "No eligible ads");
      callback(/* was_allowed */ true, absl::nullopt);
      return;
    }

    CalcuateFeaturesAndSelectAd(eligible_ads, ad_events, interest_segments,
                                intent_segments, callback);
  });
}

void EligibleAds::CalcuateFeaturesAndSelectAd(
    const CreativeAdNotificationList& eligible_ads,
    const AdEventList& ad_events,
    const SegmentList& interest_segments,
    const SegmentList& intent_segments,
    SelectAdCallback callback) const {
  DCHECK(!eligible_ads.empty());
  std::map<std::string, AdFeaturesInfo> ads =
      GroupEligibleAdsByCreativeInstanceId(eligible_ads);
  // TODO(Moritz Haller): Add test for when feautres are not set
  for (auto& ad : ads) {
    ad.second.matches_intent_child_segment =
        CalculateMatchesSegmentFeature(intent_segments, ad.second.segments);
    ad.second.matches_intent_parent_segment = CalculateMatchesSegmentFeature(
        GetParentSegments(intent_segments), ad.second.segments);
    ad.second.matches_interest_child_segment =
        CalculateMatchesSegmentFeature(interest_segments, ad.second.segments);
    ad.second.matches_interest_parent_segment = CalculateMatchesSegmentFeature(
        GetParentSegments(interest_segments), ad.second.segments);
    ad.second.ad_last_seen_in_hours = CalculateAdLastSeenInHoursFeature(
        ad_events, ad.second.creative_ad_notification.creative_instance_id);
    ad.second.advertiser_last_seen_in_hours =
        CalculateAdvertiserLastSeenInHoursFeature(
            ad_events, ad.second.creative_ad_notification.advertiser_id);

    ad.second.score = CalculateScore(ad.second);
  }

  const absl::optional<CreativeAdNotificationInfo> ad = SampleFromAds(ads);

  // TODO(Moritz haller): implement for inline content ads
  callback(/* was_allowed */ true, ad);
}

void EligibleAds::GetForParentChildSegments(
    const SegmentList& segments,
    const AdEventList& ad_events,
    const BrowsingHistoryList& browsing_history,
    GetEligibleAdsCallback callback) const {
  DCHECK(!segments.empty());

  BLOG(1, "Get eligible ads for parent-child segments:");
  for (const auto& segment : segments) {
    BLOG(1, "  " << segment);
  }

  database::table::CreativeAdNotifications database_table;
  database_table.GetForSegments(
      segments, [=](const Result result, const SegmentList& segments,
                    const CreativeAdNotificationList& ads) {
        CreativeAdNotificationList eligible_ads =
            FilterIneligibleAds(ads, ad_events, browsing_history);

        if (eligible_ads.empty()) {
          BLOG(1, "No eligible ads for parent-child segments");
          GetForParentSegments(segments, ad_events, browsing_history, callback);
          return;
        }

        callback(/* was_allowed */ true, eligible_ads);
      });
}

void EligibleAds::GetForParentSegments(
    const SegmentList& segments,
    const AdEventList& ad_events,
    const BrowsingHistoryList& browsing_history,
    GetEligibleAdsCallback callback) const {
  DCHECK(!segments.empty());

  const SegmentList parent_segments = GetParentSegments(segments);
  if (parent_segments == segments) {
    callback(/* was_allowed */ false, {});
    return;
  }

  BLOG(1, "Get eligible ads for parent segments:");
  for (const auto& parent_segment : parent_segments) {
    BLOG(1, "  " << parent_segment);
  }

  database::table::CreativeAdNotifications database_table;
  database_table.GetForSegments(
      parent_segments, [=](const Result result, const SegmentList& segments,
                           const CreativeAdNotificationList& ads) {
        CreativeAdNotificationList eligible_ads =
            FilterIneligibleAds(ads, ad_events, browsing_history);

        if (eligible_ads.empty()) {
          BLOG(1, "No eligible ads for parent segments");
          GetForUntargeted(ad_events, browsing_history, callback);
          return;
        }

        callback(/* was_allowed */ true, eligible_ads);
      });
}

void EligibleAds::GetForUntargeted(const AdEventList& ad_events,
                                   const BrowsingHistoryList& browsing_history,
                                   GetEligibleAdsCallback callback) const {
  BLOG(1, "Get eligble ads for untargeted segment");

  const std::vector<std::string> segments = {ad_targeting::kUntargeted};

  database::table::CreativeAdNotifications database_table;
  database_table.GetForSegments(
      segments, [=](const Result result, const SegmentList& segments,
                    const CreativeAdNotificationList& ads) {
        CreativeAdNotificationList eligible_ads =
            FilterIneligibleAds(ads, ad_events, browsing_history);

        if (eligible_ads.empty()) {
          BLOG(1, "No eligible ads for untargeted segment");
        }

        callback(/* was_allowed */ true, eligible_ads);
      });
}

CreativeAdNotificationList EligibleAds::FilterIneligibleAds(
    const CreativeAdNotificationList& ads,
    const AdEventList& ad_events,
    const BrowsingHistoryList& browsing_history) const {
  if (ads.empty()) {
    return {};
  }

  CreativeAdNotificationList eligible_ads = ads;

  eligible_ads = FilterSeenAdvertisersAndRoundRobinIfNeeded(
      eligible_ads, AdType::kAdNotification);

  eligible_ads =
      FilterSeenAdsAndRoundRobinIfNeeded(eligible_ads, AdType::kAdNotification);

  eligible_ads = ApplyFrequencyCapping(
      eligible_ads,
      ShouldCapLastServedAd(ads) ? last_served_creative_ad_ : CreativeAdInfo(),
      ad_events, browsing_history);

  eligible_ads = PaceAds(eligible_ads);

  eligible_ads = PrioritizeAds(eligible_ads);

  return eligible_ads;
}

CreativeAdNotificationList EligibleAds::ApplyFrequencyCapping(
    const CreativeAdNotificationList& ads,
    const CreativeAdInfo& last_served_creative_ad,
    const AdEventList& ad_events,
    const BrowsingHistoryList& browsing_history) const {
  CreativeAdNotificationList eligible_ads = ads;

  frequency_capping::ExclusionRules exclusion_rules(
      subdivision_targeting_, anti_targeting_resource_, ad_events,
      browsing_history);

  const auto iter = std::remove_if(
      eligible_ads.begin(), eligible_ads.end(),
      [&exclusion_rules, &last_served_creative_ad](CreativeAdInfo& ad) {
        return exclusion_rules.ShouldExcludeAd(ad) ||
               ad.creative_instance_id ==
                   last_served_creative_ad.creative_instance_id;
      });

  eligible_ads.erase(iter, eligible_ads.end());

  return eligible_ads;
}

}  // namespace ad_notifications
}  // namespace ads
