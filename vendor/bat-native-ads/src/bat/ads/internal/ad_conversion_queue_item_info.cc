/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ad_conversion_queue_item_info.h"

namespace ads {

AdConversionQueueItemInfo::AdConversionQueueItemInfo() :
    timestamp_in_seconds(0) {}

AdConversionQueueItemInfo::AdConversionQueueItemInfo(
    const AdConversionQueueItemInfo& info)
    : timestamp_in_seconds(info.timestamp_in_seconds),
      creative_set_id(info.creative_set_id),
      uuid(info.uuid) {}

AdConversionQueueItemInfo::~AdConversionQueueItemInfo() = default;

}  // namespace ads
