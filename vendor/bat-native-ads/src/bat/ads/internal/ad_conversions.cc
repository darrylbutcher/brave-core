/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <utility>

#include "bat/ads/internal/ad_conversions.h"
#include "bat/ads/internal/static_values.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/time.h"
#include "brave_base/random.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"

using std::placeholders::_1;
using std::placeholders::_2;

namespace ads {

const char kAdConversionsStateName[] = "ad_conversions.json";

const char kAdConversionsListKey[] = "ad_conversions";

const char kAdConversionTimestampKey[] = "timestamp_in_seconds";
const char kAdConversionCreativeSetIdKey[] = "creative_set_id";
const char kAdConversionUuidKey[] = "uuid";

AdConversions::AdConversions(
    AdsImpl* ads,
    AdsClient* ads_client,
    Client* client)
    : is_initialized_(false),
      timer_id_(0),
      ads_(ads),
      ads_client_(ads_client),
      client_(client) {
  DCHECK(ads_);
  DCHECK(ads_client_);
  DCHECK(client_);
}

AdConversions::~AdConversions() {
  StopTimer();
}

void AdConversions::Initialize(
    InitializeCallback callback) {
  callback_ = callback;

  LoadState();
}

void AdConversions::ProcessQueue() {
  DCHECK(is_initialized_);

  if (queue_.empty()) {
    BLOG(INFO) << "Ad conversion queue is empty";
    return;
  }

  AdConversionQueueItemInfo queue_item = queue_.front();
  StartTimer(queue_item);
}

void AdConversions::Add(
    const std::string& creative_set_id,
    const std::string& uuid) {
  DCHECK(is_initialized_);

  uint64_t now = Time::NowInSeconds();
  client_->AppendTimestampToAdConversionHistoryForUuid(creative_set_id, now);

  AdConversionQueueItemInfo queue_item;
  auto rand_delay = brave_base::random::Geometric(kAdConversionFrequency);
  queue_item.timestamp_in_seconds = Time::NowInSeconds() + rand_delay;
  queue_item.creative_set_id = creative_set_id;
  queue_item.uuid = uuid;

  queue_.push_back(queue_item);

  std::sort(queue_.begin(), queue_.end(), [] (
      const AdConversionQueueItemInfo& a,
      const AdConversionQueueItemInfo& b) {
    return a.timestamp_in_seconds < b.timestamp_in_seconds;
  });

  SaveState();

  if (timer_id_ == 0) {
    ProcessQueue();
  }
}

bool AdConversions::OnTimer(
    const uint32_t timer_id) {
  if (timer_id != timer_id_) {
    return false;
  }

  timer_id_ = 0;

  DCHECK(!queue_.empty());
  if (queue_.empty()) {
    return true;
  }

  AdConversionQueueItemInfo queue_item = queue_.front();
  ProcessQueueItem(queue_item);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void AdConversions::ProcessQueueItem(
    const AdConversionQueueItemInfo& queue_item) {
  const uint64_t timestamp_in_seconds = queue_item.timestamp_in_seconds;
  const std::string creative_set_id = queue_item.creative_set_id;
  const std::string uuid = queue_item.uuid;

  BLOG(INFO) << "Ad conversion for uuid " << uuid << " with creative set id "
      << creative_set_id << " on " << Time::FromDoubleT(timestamp_in_seconds);

  ads_->ConfirmAction(uuid, creative_set_id, ConfirmationType::CONVERSION);

  Remove(uuid);

  ProcessQueue();
}

void AdConversions::StartTimer(
    const AdConversionQueueItemInfo& queue_item) {
  DCHECK(is_initialized_);
  DCHECK_EQ(0UL, timer_id_);

  StopTimer();

  uint64_t start_timer_in;
  if (queue_item.timestamp_in_seconds < Time::NowInSeconds()) {
    start_timer_in = queue_item.timestamp_in_seconds - Time::NowInSeconds();
  } else {
    start_timer_in = brave_base::random::Geometric(
        kExpiredAdConversionFrequency);
  }

  timer_id_ = ads_client_->SetTimer(start_timer_in);
  if (timer_id_ == 0) {
    BLOG(ERROR) << "Failed to start ad conversion timer";
    return;
  }

  BLOG(INFO) << "Started ad conversion timer for uuid " << queue_item.uuid
      << " and creative set id " << queue_item.creative_set_id << " on "
          << Time::FromDoubleT(queue_item.timestamp_in_seconds);
}

void AdConversions::StopTimer() {
  if (timer_id_ == 0) {
    return;
  }

  BLOG(INFO) << "Stopped ad conversion timer";

  ads_client_->KillTimer(timer_id_);
  timer_id_ = 0;
}

bool AdConversions::Remove(
    const std::string& uuid) {
  DCHECK(is_initialized_);

  auto iter = std::find_if(queue_.begin(), queue_.end(), [&uuid] (
      const auto& conversion) {
    return conversion.uuid == uuid;
  });

  if (iter == queue_.end()) {
    return false;
  }

  queue_.erase(iter);

  SaveState();

  return true;
}

void AdConversions::SaveState() {
  if (!is_initialized_) {
    return;
  }

  BLOG(INFO) << "Saving conversions state";

  std::string json = ToJson();
  auto callback = std::bind(&AdConversions::OnStateSaved, this, _1);
  ads_client_->Save(kAdConversionsStateName, json, callback);
}

void AdConversions::OnStateSaved(const Result result) {
  if (result != SUCCESS) {
    BLOG(ERROR) << "Failed to save conversions state";
    return;
  }

  BLOG(INFO) << "Successfully saved conversions state";
}

std::string AdConversions::ToJson() {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  auto conversions = GetAsList();
  dictionary.SetKey(kAdConversionsListKey,
      base::Value(std::move(conversions)));

  // Write to JSON
  std::string json;
  base::JSONWriter::Write(dictionary, &json);

  return json;
}

base::Value AdConversions::GetAsList() {
  base::Value list(base::Value::Type::LIST);

  for (const auto& queue_item : queue_) {
    base::Value dictionary(base::Value::Type::DICTIONARY);

    dictionary.SetKey(kAdConversionTimestampKey,
        base::Value(std::to_string(queue_item.timestamp_in_seconds)));
    dictionary.SetKey(kAdConversionCreativeSetIdKey,
        base::Value(queue_item.creative_set_id));
    dictionary.SetKey(kAdConversionUuidKey,
        base::Value(queue_item.uuid));

    list.GetList().push_back(std::move(dictionary));
  }

  return list;
}

void AdConversions::LoadState() {
  auto callback = std::bind(&AdConversions::OnStateLoaded, this, _1, _2);
  ads_client_->Load(kAdConversionsStateName, callback);
}

void AdConversions::OnStateLoaded(
    const Result result,
    const std::string& json) {
  is_initialized_ = true;

  if (result != SUCCESS) {
    BLOG(ERROR) << "Failed to load conversions state, resetting to default "
        "values";

    queue_.clear();
  } else {
    if (!FromJson(json)) {
      BLOG(ERROR) << "Failed to parse conversions state: " << json;

      callback_(FAILED);
      return;
    }

    BLOG(INFO) << "Successfully loaded conversions state";
  }

  callback_(SUCCESS);
}

bool AdConversions::FromJson(
    const std::string& json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    return false;
  }

  base::DictionaryValue* dictionary = nullptr;
  if (!value->GetAsDictionary(&dictionary)) {
    return false;
  }

  auto* conversions_list_value = dictionary->FindKey(kAdConversionsListKey);
  if (!conversions_list_value || !conversions_list_value->is_list()) {
    return false;
  }

  base::ListValue* list = nullptr;
  if (!conversions_list_value->GetAsList(&list)) {
    return false;
  }

  queue_ = GetAdConversionsFromList(list);

  SaveState();

  return true;
}

std::vector<AdConversionQueueItemInfo> AdConversions::GetAdConversionsFromList(
    const base::ListValue* list) const {
  std::vector<AdConversionQueueItemInfo> ad_conversions;

  DCHECK(list);
  if (!list) {
    return ad_conversions;
  }

  for (const auto& value : list->GetList()) {
    if (!value.is_dict()) {
      NOTREACHED();
      continue;
    }

    const base::DictionaryValue* dictionary = nullptr;
    value.GetAsDictionary(&dictionary);
    if (!dictionary) {
      NOTREACHED();
      continue;
    }

    AdConversionQueueItemInfo conversion;
    if (!GetAdConversionFromDictionary(dictionary, &conversion)) {
      NOTREACHED();
      continue;
    }

    ad_conversions.push_back(conversion);
  }

  return ad_conversions;
}

bool AdConversions::GetAdConversionFromDictionary(
    const base::DictionaryValue* dictionary,
    AdConversionQueueItemInfo* info) const {
  DCHECK(dictionary);
  if (!dictionary) {
    return false;
  }

  DCHECK(info);
  if (!info) {
    return false;
  }

  AdConversionQueueItemInfo queue_item_info;

  // Timestamp (There is no support for uint64_t. Writing JSON with such types
  // violates the spec. As we need a uint64_t, we need to use a double and cast
  // to a uint64_t)
  const auto timestamp = dictionary->FindDoubleKey(kAdConversionTimestampKey);
  if (!timestamp) {
    return false;
  }
  queue_item_info.timestamp_in_seconds = static_cast<uint64_t>(*timestamp);

  // Creative Set Id
  const auto* creative_set_id =
      dictionary->FindStringKey(kAdConversionCreativeSetIdKey);
  if (!creative_set_id) {
    return false;
  }
  queue_item_info.creative_set_id = *creative_set_id;

  // UUID
  const auto* uuid = dictionary->FindStringKey(kAdConversionUuidKey);
  if (!uuid) {
    return false;
  }
  queue_item_info.uuid = *uuid;

  return true;
}

}  // namespace ads
