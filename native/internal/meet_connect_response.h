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

#ifndef NATIVE_INTERNAL_MEET_CONNECT_RESPONSE_H_
#define NATIVE_INTERNAL_MEET_CONNECT_RESPONSE_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace meet {

// MeetConnectResponse represents the response from Meet servers when sending a
// connectActiveConference request. A successful request will contain the answer
// to the SDP offer provided in the request. This answer is used to complete the
// WebRTC signaling process and join the conference.
struct MeetConnectResponse {
  static absl::StatusOr<MeetConnectResponse> FromRequestResponse(
      std::string request_response);

  std::string answer;
  absl::Status status = absl::OkStatus();
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEET_CONNECT_RESPONSE_H_
