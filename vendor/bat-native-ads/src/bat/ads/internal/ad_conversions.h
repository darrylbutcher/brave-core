/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_AD_CONVERSIONS_H_
#define BAT_ADS_INTERNAL_AD_CONVERSIONS_H_

#include <string>
#include <vector>

#include "bat/ads/ads_client.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/client.h"
#include "bat/ads/internal/ad_conversion_queue_item_info.h"

#include "base/values.h"

namespace ads {

class AdsImpl;
class Client;

class AdConversions {
 public:
  AdConversions(
      AdsImpl* ads,
      AdsClient* ads_client,
      Client* client);
  ~AdConversions();

  void Initialize(
      InitializeCallback callback);

  void ProcessQueue();

  void Add(
      const std::string& creative_set_id,
      const std::string& uuid);

  bool OnTimer(
      const uint32_t timer_id);

 private:
  bool is_initialized_;

  InitializeCallback callback_;

  std::vector<AdConversionQueueItemInfo> queue_;

  void ProcessQueueItem(
      const AdConversionQueueItemInfo& info);

  void StartTimer(
      const AdConversionQueueItemInfo& queue_item);
  void StopTimer();

  bool Remove(
      const std::string& uuid);

  void SaveState();
  void OnStateSaved(
      const Result result);

  std::string ToJson();
  base::Value GetAsList();

  void LoadState();
  void OnStateLoaded(
      const Result result,
      const std::string& json);

  bool FromJson(
      const std::string& json);
  std::vector<AdConversionQueueItemInfo> GetAdConversionsFromList(
      const base::ListValue* list) const;
  bool GetAdConversionFromDictionary(
      const base::DictionaryValue* dictionary,
      AdConversionQueueItemInfo* info) const;

  uint32_t timer_id_;

  AdsImpl* ads_;  // NOT OWNED
  AdsClient* ads_client_;  // NOT OWNED
  Client* client_;  // NOT OWNED
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_AD_CONVERSIONS_H_
