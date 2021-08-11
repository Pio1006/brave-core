/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/ntp_background_images/browser/ntp_background_images_data.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "brave/components/ntp_background_images/browser/url_constants.h"
#include "content/public/common/url_constants.h"

/* Sample json.
{
  "schemaVersion": 1,
  "images": [
    {
      "name": "ntp-2020/2021-1",
      "source": "background-image-source.png",
      "author": "Brave Software",
      "link": "https://brave.com/",
      "originalUrl": "Contributor sent the hi-res version through email",
      "license": "https://brave.com/about/"
    },
  ]
}
*/

namespace ntp_background_images {

namespace {

constexpr int kExpectedSchemaVersion = 1;

}  // namespace

Background::Background() = default;
Background::Background(const base::FilePath& image_file_path,
                       const std::string& author_name,
                       const std::string& author_link)
    : image_file(image_file_path),
    author(author_name),
    link(author_link) {}
Background::Background(const Background&) = default;
Background::~Background() = default;

NTPBackgroundImagesData::NTPBackgroundImagesData() {}

NTPBackgroundImagesData::NTPBackgroundImagesData(
    const std::string& json_string,
    const base::FilePath& installed_dir)
    : NTPBackgroundImagesData() {
  absl::optional<base::Value> json_value = base::JSONReader::Read(json_string);
  LOG(WARNING) << "NTPBackgroundImagesData::NTPBackgroundImagesData: !json_value: " << (!json_value);
  if (!json_value) {
    DVLOG(2) << "Read json data failed. Invalid JSON data";
    return;
  }

  LOG(WARNING) << "NTPBackgroundImagesData::NTPBackgroundImagesData: 1";
  absl::optional<int> incomingSchemaVersion =
      json_value->FindIntKey(kSchemaVersionKey);
  const bool schemaVersionIsValid = incomingSchemaVersion &&
      *incomingSchemaVersion == kExpectedSchemaVersion;
  if (!schemaVersionIsValid) {
    DVLOG(2) << __func__ << "Incoming NTP background images data was not valid."
            << " Schema version was "
            << (incomingSchemaVersion ? std::to_string(*incomingSchemaVersion)
                                      : "missing")
            << ", but we expected " << kExpectedSchemaVersion;
    return;
  }
  LOG(WARNING) << "NTPBackgroundImagesData::NTPBackgroundImagesData: 2";

  if (auto* images = json_value->FindListKey(kImagesKey)) {
    const int image_count = images->GetList().size();
    LOG(WARNING) << "NTPBackgroundImagesData::NTPBackgroundImagesData: image_count: " << image_count;
    for (int i = 0; i < image_count; ++i) {
      const auto& image = images->GetList()[i];
      Background background;
      background.image_file =
          installed_dir.AppendASCII(*image.FindStringKey(kImageSourceKey));
      background.author = *image.FindStringKey(kImageAuthorKey);
      background.link = *image.FindStringKey(kImageLinkKey);
      LOG(WARNING) << "NTPBackgroundImagesData::NTPBackgroundImagesData: not is_sponsored_image: background.image_file: " << background.image_file;

      backgrounds.push_back(background);
    }
  }
}

NTPBackgroundImagesData& NTPBackgroundImagesData::operator=(
    const NTPBackgroundImagesData& data) = default;
NTPBackgroundImagesData::NTPBackgroundImagesData(
    const NTPBackgroundImagesData& data) = default;
NTPBackgroundImagesData::~NTPBackgroundImagesData() = default;

bool NTPBackgroundImagesData::IsValid() const {
  return !backgrounds.empty();
}

base::Value NTPBackgroundImagesData::GetBackgroundAt(size_t index) {
  LOG(WARNING) << "NTPBackgroundImagesData::GetBackgroundAt: index: " << index << " IsValid: " << IsValid();
  DCHECK(index >= 0);

  base::Value data(base::Value::Type::DICTIONARY);
  if (!IsValid())
    return data;

  data.SetStringKey(kWallpaperImagePathKey,
                    backgrounds[index].image_file.AsUTF8Unsafe());

  data.SetStringKey(kImageAuthorKey, backgrounds[index].author);
  data.SetStringKey(kImageLinkKey, backgrounds[index].link);
  return data;
}

}  // namespace ntp_background_images
