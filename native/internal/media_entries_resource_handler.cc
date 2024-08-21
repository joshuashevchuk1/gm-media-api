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

#include "native/internal/media_entries_resource_handler.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "nlohmann/json.hpp"
#include "native/api/conference_resources.h"

namespace meet {
namespace {
using Json = ::nlohmann::json;

// Media entries resource channel is always opened with this label.
constexpr absl::string_view kMediaEntriesResourceName = "media-entries";

const Json* FindOrNull(const Json& json, absl::string_view key) {
  auto it = json.find(key);
  return it != json.cend() ? &*it : nullptr;
}

}  // namespace

absl::StatusOr<MediaEntriesChannelToClient>
MediaEntriesResourceHandler::ParseUpdate(absl::string_view update) {
  DLOG(INFO) << kMediaEntriesResourceName
             << " resource update received: " << update;

  const Json json_resource_update = Json::parse(update, /*cb=*/nullptr,
                                                /*allow_exceptions=*/false);

  if (!json_resource_update.is_object()) {
    return absl::InternalError(absl::StrCat(
        "Invalid ", kMediaEntriesResourceName, " json format: ", update));
  }

  MediaEntriesChannelToClient resource_update;
  // Resources
  if (const Json* resources_field =
          FindOrNull(json_resource_update, "resources");
      resources_field != nullptr) {
    if (!resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kMediaEntriesResourceName,
          " json format. Expected resources field to be an array: ", update));
    }

    std::vector<MediaEntriesResourceSnapshot>& resources =
        resource_update.resources;
    for (const Json& resource : *resources_field) {
      // Resources.resourceSnapshot
      MediaEntriesResourceSnapshot snapshot;

      // Resources.resourceSnapshot.id
      if (const Json* id_field = FindOrNull(resource, "id");
          id_field != nullptr) {
        snapshot.id = id_field->get<int64_t>();
      } else {
        snapshot.id = 0;
      }

      // Resources.resourceSnapshot.mediaEntry
      if (const Json* media_entry_field = FindOrNull(resource, "mediaEntry");
          media_entry_field != nullptr) {
        MediaEntry media_entry;

        // Resources.resourceSnapshot.mediaEntry.participantId
        if (const Json* participant_id_field =
                FindOrNull(*media_entry_field, "participantId");
            participant_id_field != nullptr) {
          media_entry.participant_id = participant_id_field->get<int32_t>();
        } else {
          media_entry.participant_id = 0;
        }

        // Resources.resourceSnapshot.mediaEntry.audioCsrc
        if (const Json* audio_csrc_field =
                FindOrNull(*media_entry_field, "audioCsrc");
            audio_csrc_field != nullptr) {
          media_entry.audio_csrc = audio_csrc_field->get<uint32_t>();
        } else {
          media_entry.audio_csrc = 0;
        }

        // Resources.resourceSnapshot.mediaEntry.videoCsrcs
        if (const Json* video_csrcs_field =
                FindOrNull(*media_entry_field, "videoCsrcs");
            video_csrcs_field != nullptr) {
          if (!video_csrcs_field->is_array()) {
            return absl::InternalError(absl::StrCat(
                "Invalid ", kMediaEntriesResourceName,
                " json format. Expected videoCsrcs field to be an array: ",
                update));
          }
          for (const Json& video_csrc : *video_csrcs_field) {
            media_entry.video_csrcs.push_back(video_csrc.get<uint32_t>());
          }
        } else {
          media_entry.video_csrcs.clear();
        }

        // Resources.resourceSnapshot.mediaEntry.presenter
        if (const Json* presenter_field =
                FindOrNull(*media_entry_field, "presenter");
            presenter_field != nullptr) {
          media_entry.presenter = presenter_field->get<bool>();
        } else {
          media_entry.presenter = false;
        }

        // Resources.resourceSnapshot.mediaEntry.screenshare
        if (const Json* screenshare_field =
                FindOrNull(*media_entry_field, "screenshare");
            screenshare_field != nullptr) {
          media_entry.screenshare = screenshare_field->get<bool>();
        } else {
          media_entry.screenshare = false;
        }

        // Resources.resourceSnapshot.mediaEntry.audioMuted
        if (const Json* audio_muted_field =
                FindOrNull(*media_entry_field, "audioMuted");
            audio_muted_field != nullptr) {
          media_entry.audio_muted = audio_muted_field->get<bool>();
        } else {
          media_entry.audio_muted = false;
        }

        // Resources.resourceSnapshot.mediaEntry.videoMuted
        if (const Json* video_muted_field =
                FindOrNull(*media_entry_field, "videoMuted");
            video_muted_field != nullptr) {
          media_entry.video_muted = video_muted_field->get<bool>();
        } else {
          media_entry.video_muted = false;
        }

        snapshot.media_entry = std::move(media_entry);
      } else {
        snapshot.media_entry = std::nullopt;
      }
      resources.push_back(std::move(snapshot));
    }
  }

  // deletedResources
  if (const Json* deleted_resources_field =
          FindOrNull(json_resource_update, "deletedResources");
      deleted_resources_field != nullptr) {
    if (!deleted_resources_field->is_array()) {
      return absl::InternalError(absl::StrCat(
          "Invalid ", kMediaEntriesResourceName,
          " json format. Expected deletedResources field to be an array: ",
          update));
    }

    std::vector<MediaEntriesDeletedResource>& deleted_resources =
        resource_update.deleted_resources;
    for (const Json& deleted_resource : *deleted_resources_field) {
      // deletedResources.deletedResource
      MediaEntriesDeletedResource deleted_entry;

      // deletedResources.deletedResource.id
      if (const Json* id_field = FindOrNull(deleted_resource, "id");
          id_field != nullptr) {
        deleted_entry.id = id_field->get<int64_t>();
      } else {
        deleted_entry.id = 0;
      }

      // deletedResources.deletedResource.mediaEntry
      if (const Json* media_entry_field =
              FindOrNull(deleted_resource, "mediaEntry");
          media_entry_field != nullptr) {
        deleted_entry.media_entry = media_entry_field->get<bool>();
      } else {
        deleted_entry.media_entry = std::nullopt;
      }
      deleted_resources.push_back(std::move(deleted_entry));
    }
  }

  return std::move(resource_update);
}
}  // namespace meet
