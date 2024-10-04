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

#ifndef NATIVE_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_
#define NATIVE_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/session_control_resource.h"
#include "native/internal/resource_handler_interface.h"

namespace meet {

// Parses and dispatches session control resource updates from Meet servers.
class SessionControlResourceHandler
    : public ResourceHandlerInterface<SessionControlChannelToClient,
                                      SessionControlChannelFromClient> {
 public:
  static absl::string_view MeetingConnectionStateToString(
      SessionStatus::MeetingConnectionState state) {
    switch (state) {
      case SessionStatus::MeetingConnectionState::kWaiting:
        return "STATE_WAITING";
      case SessionStatus::MeetingConnectionState::kJoined:
        return "STATE_JOINED";
      case SessionStatus::MeetingConnectionState::kDisconnected:
        return "STATE_DISCONNECTED";
      default:
        return "STATE_UNKNOWN";
    }
  }

  static SessionStatus::MeetingConnectionState StringToMeetingConnectionState(
      absl::string_view state) {
    if (state == "STATE_WAITING") {
      return SessionStatus::MeetingConnectionState::kWaiting;
    } else if (state == "STATE_JOINED") {
      return SessionStatus::MeetingConnectionState::kJoined;
    } else if (state == "STATE_DISCONNECTED") {
      return SessionStatus::MeetingConnectionState::kDisconnected;
    } else {
      return SessionStatus::MeetingConnectionState::kUnknown;
    }
  }

  SessionControlResourceHandler() = default;
  ~SessionControlResourceHandler() override = default;

  absl::StatusOr<SessionControlChannelToClient> ParseUpdate(
      absl::string_view update) override;

  absl::StatusOr<std::string> Stringify(
      const SessionControlChannelFromClient& client_request) override;

  // SessionControlResourceHandler is neither copyable nor movable.
  SessionControlResourceHandler(const SessionControlResourceHandler&) = delete;
  SessionControlResourceHandler& operator=(
      const SessionControlResourceHandler&) = delete;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_SESSION_CONTROL_RESOURCE_HANDLER_H_
