/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/bat_util.h"
#include "bat/ledger/internal/state/report_balance_state.h"
#include "base/json/json_reader.h"
#include "base/logging.h"

namespace ledger {

namespace {

// Do not change these values as they are required to transition legacy state
const char kAutoContributionsKey[] = "auto_contribute";
const char kAdEarningsKey[] = "earning_from_ads";
const char kGrantsKey[] = "grants";
const char kOneTimeDonationsKey[] = "one_time_donation";
const char kRecurringDonationsKey[] = "recurring_donation";

}  // namespace

ReportBalanceState::ReportBalanceState() = default;

ReportBalanceState::~ReportBalanceState() = default;

bool ReportBalanceState::FromJson(
    const std::string& json,
    ReportBalanceProperties* properties) const {
  DCHECK(properties);
  if (!properties) {
    NOTREACHED();
    return false;
  }

  auto json_value = base::JSONReader::Read(json);
  if (!json_value) {
    NOTREACHED();
    return false;
  }

  base::DictionaryValue* dictionary = nullptr;
  json_value->GetAsDictionary(&dictionary);
  if (!dictionary) {
    NOTREACHED();
    return false;
  }

  return FromDict(dictionary, properties);
}

bool ReportBalanceState::FromDict(
    const base::DictionaryValue* dictionary,
    ReportBalanceProperties* properties) const {
  DCHECK(dictionary);
  if (!dictionary) {
    NOTREACHED();
    return false;
  }

  DCHECK(properties);
  if (!properties) {
    NOTREACHED();
    return false;
  }

  ReportBalanceProperties report_balance_properties;

  // Grants
  const auto grants = dictionary->FindDoubleKey(kGrantsKey);
  if (grants) {
    report_balance_properties.grants = *grants;
  } else {
    const auto* grants_probi = dictionary->FindStringKey(kGrantsKey);
    if (!grants_probi) {
      NOTREACHED();
      return false;
    }

    report_balance_properties.grants =
        braveledger_bat_util::ProbiToDouble(*grants_probi);
  }

  // Earnings From Ads
  const auto ad_earnings = dictionary->FindDoubleKey(kAdEarningsKey);
  if (ad_earnings) {
    report_balance_properties.ad_earnings = *ad_earnings;
  } else {
    const auto* ad_earnings_probi = dictionary->FindStringKey(kAdEarningsKey);
    if (!ad_earnings_probi) {
      NOTREACHED();
      return false;
    }

    report_balance_properties.ad_earnings =
        braveledger_bat_util::ProbiToDouble(*ad_earnings_probi);
  }

  // Auto Contribute
  const auto auto_contributions =
      dictionary->FindDoubleKey(kAutoContributionsKey);
  if (auto_contributions) {
    report_balance_properties.auto_contributions = *auto_contributions;
  } else {
    const auto* auto_contributions_probi =
        dictionary->FindStringKey(kAutoContributionsKey);
    if (!auto_contributions_probi) {
      NOTREACHED();
      return false;
    }

    report_balance_properties.auto_contributions =
        braveledger_bat_util::ProbiToDouble(*auto_contributions_probi);
  }

  // Recurring Donation
  const auto recurring_donations =
      dictionary->FindDoubleKey(kRecurringDonationsKey);
  if (recurring_donations) {
    report_balance_properties.recurring_donations = *recurring_donations;
  } else {
    const auto* recurring_donations_probi =
        dictionary->FindStringKey(kRecurringDonationsKey);
    if (!recurring_donations_probi) {
      NOTREACHED();
      return false;
    }

    report_balance_properties.recurring_donations =
        braveledger_bat_util::ProbiToDouble(*recurring_donations_probi);
  }

  // One Time Donation
  const auto one_time_donations =
      dictionary->FindDoubleKey(kOneTimeDonationsKey);
  if (one_time_donations) {
    report_balance_properties.one_time_donations = *one_time_donations;
  } else {
    const auto* one_time_donations_probi =
        dictionary->FindStringKey(kOneTimeDonationsKey);
    if (!one_time_donations_probi) {
      NOTREACHED();
      return false;
    }

    report_balance_properties.one_time_donations =
        braveledger_bat_util::ProbiToDouble(*one_time_donations_probi);
  }

  *properties = report_balance_properties;

  return true;
}

bool ReportBalanceState::ToJson(
    JsonWriter* writer,
    const ReportBalanceProperties& properties) const {
  DCHECK(writer);
  if (!writer) {
    NOTREACHED();
    return false;
  }

  writer->StartObject();

  writer->String(kGrantsKey);
  writer->Double(properties.grants);

  writer->String(kAdEarningsKey);
  writer->Double(properties.ad_earnings);

  writer->String(kAutoContributionsKey);
  writer->Double(properties.auto_contributions);

  writer->String(kRecurringDonationsKey);
  writer->Double(properties.recurring_donations);

  writer->String(kOneTimeDonationsKey);
  writer->Double(properties.one_time_donations);

  writer->EndObject();

  return true;
}

std::string ReportBalanceState::ToJson(
    const ReportBalanceProperties& properties) const {
  rapidjson::StringBuffer buffer;
  JsonWriter writer(buffer);

  if (!ToJson(&writer, properties)) {
    NOTREACHED();
    return "";
  }

  return buffer.GetString();
}

}  // namespace ledger
