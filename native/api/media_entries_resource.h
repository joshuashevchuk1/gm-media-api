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

#ifndef NATIVE_API_MEDIA_ENTRIES_RESOURCE_H_
#define NATIVE_API_MEDIA_ENTRIES_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// TODO: Update the docs for all the resource structs in this file
// and make it clear how resources are used. I.e. what each update is and how a
// client can/should react to them.

namespace meet {

struct MediaEntry {
  // Resource name for a participant. Interoperable with the Meet REST API.
  // Format: `conferenceRecords/{conference_record}/participants/{name}`.
  std::string participant_name;
  // Resource name for a participant session. Interoperable with the Meet REST
  // API.
  //
  // Format:
  // `conferenceRecords/participants/participantSessions/{session_name}`.
  std::string session_name;
  // The CSRC for any audio stream contributed by this participant. Will be
  // zero if no stream is provided.
  uint32_t audio_csrc = 0;
  // The CSRC for any video stream contributed by this participant. Will be
  // empty if no stream is provided.
  std::vector<uint32_t> video_csrcs;
  // Signals if the current entry is presenting.
  bool presenter = false;
  // Signals if the current entry is a screenshare.
  bool screenshare = false;
  // Signals if the audio stream is currently muted by the remote participant.
  bool audio_muted = false;
  // Signals if the video stream is currently muted by the remote participant.
  bool video_muted = false;
};

struct MediaEntriesResourceSnapshot {
  // The resource ID of the resource being updated.
  int64_t id = 0;
  std::optional<MediaEntry> media_entry;
};

struct MediaEntriesDeletedResource {
  // The resource ID of the resource being deleted.
  int64_t id = 0;
  std::optional<bool> media_entry;
};

// The top-level transport container for messages sent from server to
// client in the "media-entries" data channel.
struct MediaEntriesChannelToClient {
  // Resource snapshots. There is no implied order between the snapshots in the
  // list.
  std::vector<MediaEntriesResourceSnapshot> resources;
  // The list of deleted resources. There is no order between the entries in the
  // list.
  std::vector<MediaEntriesDeletedResource> deleted_resources;
};

}  // namespace meet

#endif  // NATIVE_API_MEDIA_ENTRIES_RESOURCE_H_
