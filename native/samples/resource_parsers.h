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

#ifndef NATIVE_EXAMPLES_RESOURCE_PARSERS_H_
#define NATIVE_EXAMPLES_RESOURCE_PARSERS_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "native/api/conference_resources.h"

namespace meet {
using Json = ::nlohmann::json;

inline std::string VideoAssignmentStringify(
    VideoAssignmentChannelToClient& update) {
  nlohmann::basic_json<> json_update;

  if (update.response.has_value()) {
    json_update["response"]["requestId"] = update.response->request_id;
    json_update["response"]["status"]["code"] = update.response->status.code();
    json_update["response"]["status"]["message"] =
        update.response->status.message();
    if (update.response->set_assignment.has_value()) {
      json_update["response"]["setAssignment"] =
          nlohmann::json::value_t::object;
    }
  }

  if (!update.resources.empty()) {
    nlohmann::basic_json<> resources;
    for (const auto& resource : update.resources) {
      nlohmann::basic_json<> resource_snapshot;
      resource_snapshot["id"] = resource.id;
      if (resource.assignment.has_value()) {
        nlohmann::basic_json<> assignment;
        assignment["label"] = resource.assignment->label;
        nlohmann::basic_json<> canvases;
        for (const auto& canvas : resource.assignment->canvases) {
          nlohmann::basic_json<> canvas_assignment;
          canvas_assignment["canvasId"] = canvas.canvas_id;
          canvas_assignment["ssrc"] = canvas.ssrc;
          canvas_assignment["mediaEntryId"] = canvas.media_entry_id;
          canvases.push_back(std::move(canvas_assignment));
        }
        assignment["canvases"] = std::move(canvases);
        resource_snapshot["assignment"] = std::move(assignment);
      }
      resources.push_back(std::move(resource_snapshot));
    }
    json_update["resources"] = std::move(resources);
  }
  return json_update.dump();
}

inline std::string MediaEntriesStringify(MediaEntriesChannelToClient& update) {
  nlohmann::basic_json<> json_update;

  if (!update.resources.empty()) {
    nlohmann::basic_json<> resources;
    for (const auto& resource : update.resources) {
      nlohmann::basic_json<> resource_snapshot;
      resource_snapshot["id"] = resource.id;
      if (resource.media_entry.has_value()) {
        nlohmann::basic_json<> media_entry;
        media_entry["participantId"] = resource.media_entry->participant_id;
        media_entry["audioCsrc"] = resource.media_entry->audio_csrc;
        media_entry["videoCsrcs"] = resource.media_entry->video_csrcs;
        media_entry["presenter"] = resource.media_entry->presenter;
        media_entry["screenshare"] = resource.media_entry->screenshare;
        media_entry["audioMuted"] = resource.media_entry->audio_muted;
        media_entry["videoMuted"] = resource.media_entry->video_muted;
        resource_snapshot["mediaEntry"] = std::move(media_entry);
      }
      resources.push_back(std::move(resource_snapshot));
    }
    json_update["resources"] = std::move(resources);
  }

  if (!update.deleted_resources.empty()) {
    nlohmann::basic_json<> deleted_resources;
    for (const auto& resource : update.deleted_resources) {
      nlohmann::basic_json<> deleted_resource;
      deleted_resource["id"] = resource.id;
      if (resource.media_entry.has_value()) {
        deleted_resource["mediaEntry"] = resource.media_entry.value();
      }
      deleted_resources.push_back(std::move(deleted_resource));
    }
    json_update["deletedResources"] = std::move(deleted_resources);
  }

  return json_update.dump();
}

inline constexpr absl::string_view ConnectionStateToString(
    SessionStatus::MeetingConnectionState state) {
  switch (state) {
    case SessionStatus::MeetingConnectionState::kWaiting:
      return "WAITING";
    case SessionStatus::MeetingConnectionState::kJoined:
      return "JOINED";
    case SessionStatus::MeetingConnectionState::kDisconnected:
      return "DISCONNECTED";
    case SessionStatus::MeetingConnectionState::kUnknown:
    default:
      return "UNKNOWN";
  }
}

inline std::string SessionControlStringify(
    SessionControlChannelToClient& update) {
  nlohmann::basic_json<> json_update;

  if (update.response.has_value()) {
    nlohmann::basic_json<> response;
    response["requestId"] = update.response->request_id;
    response["status"]["code"] = update.response->status.code();
    response["status"]["message"] = update.response->status.message();
    response["leaveResponse"] = nlohmann::json::value_t::object;
    json_update["response"] = std::move(response);
  }

  if (!update.resources.empty()) {
    nlohmann::basic_json<> resources;
    for (const auto& resource : update.resources) {
      nlohmann::basic_json<> resource_snapshot;
      resource_snapshot["id"] = resource.id;
      nlohmann::basic_json<> session_status;
      session_status["connectionState"] =
          ConnectionStateToString(resource.session_status.connection_state);
      resource_snapshot["sessionStatus"] = session_status;
      resources.push_back(std::move(resource_snapshot));
    }
    json_update["resources"] = std::move(resources);
  }

  return json_update.dump();
}

}  // namespace meet

#endif  // NATIVE_EXAMPLES_RESOURCE_PARSERS_H_
