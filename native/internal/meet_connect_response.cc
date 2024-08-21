/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "native/internal/meet_connect_response.h"

#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"

namespace meet {
namespace {
using Json = ::nlohmann::json;

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

absl::StatusCode StringToStatusCode(std::string string_status) {
  if (string_status == "OK") {
    return absl::StatusCode::kOk;
  } else if (string_status == "CANCELLED") {
    return absl::StatusCode::kCancelled;
  } else if (string_status == "UNKNOWN") {
    return absl::StatusCode::kUnknown;
  } else if (string_status == "INVALID_ARGUMENT") {
    return absl::StatusCode::kInvalidArgument;
  } else if (string_status == "DEADLINE_EXCEEDED") {
    return absl::StatusCode::kDeadlineExceeded;
  } else if (string_status == "NOT_FOUND") {
    return absl::StatusCode::kNotFound;
  } else if (string_status == "ALREADY_EXISTS") {
    return absl::StatusCode::kAlreadyExists;
  } else if (string_status == "PERMISSION_DENIED") {
    return absl::StatusCode::kPermissionDenied;
  } else if (string_status == "UNAUTHENTICATED") {
    return absl::StatusCode::kUnauthenticated;
  } else if (string_status == "RESOURCE_EXHAUSTED") {
    return absl::StatusCode::kResourceExhausted;
  } else if (string_status == "FAILED_PRECONDITION") {
    return absl::StatusCode::kFailedPrecondition;
  } else if (string_status == "ABORTED") {
    return absl::StatusCode::kAborted;
  } else if (string_status == "OUT_OF_RANGE") {
    return absl::StatusCode::kOutOfRange;
  } else if (string_status == "UNIMPLEMENTED") {
    return absl::StatusCode::kUnimplemented;
  } else if (string_status == "INTERNAL") {
    return absl::StatusCode::kInternal;
  } else if (string_status == "UNAVAILABLE") {
    return absl::StatusCode::kUnavailable;
  } else if (string_status == "DATA_LOSS") {
    return absl::StatusCode::kDataLoss;
  } else {
    return absl::StatusCode::kUnknown;
  }
}

}  // namespace

absl::StatusOr<MeetConnectResponse> MeetConnectResponse::FromRequestResponse(
    std::string request_response) {
  DLOG(INFO) << "Parsing response from Meet servers: " << request_response;

  const Json json_request_response =
      Json::parse(request_response, /*cb=*/nullptr,
                  /*allow_exceptions=*/false);

  if (!json_request_response.is_object()) {
    return absl::InternalError(
        "Unexpected or malformed response from Meet servers.");
  }

  MeetConnectResponse response;
  // Response.answer
  if (const Json* answer_field = FindOrNull(json_request_response, "answer");
      answer_field != nullptr) {
    response.answer = answer_field->get<std::string>();
  }

  // Response.error
  if (const Json* error_field = FindOrNull(json_request_response, "error");
      error_field != nullptr) {
    // Response.error.status
    absl::StatusCode status_code = absl::StatusCode::kUnknown;
    if (const Json* code_field = FindOrNull(*error_field, "status");
        code_field != nullptr) {
      status_code = StringToStatusCode((code_field->get<std::string>()));
    }

    // Response.error.message
    std::string message;
    if (const Json* message_field = FindOrNull(*error_field, "message");
        message_field != nullptr) {
      message = message_field->get<std::string>();
    }
    response.status = absl::Status(status_code, std::move(message));
  }

  return std::move(response);
}

}  // namespace meet
