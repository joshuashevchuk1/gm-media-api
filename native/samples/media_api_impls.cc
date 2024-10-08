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

#include "native/samples/media_api_impls.h"

#include <cstdint>
#include <filesystem>  // NOLINT
#include <fstream>
#include <ios>
#include <ostream>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/samples/resource_parsers.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/scoped_refptr.h"

namespace media_api_impls {

void AudioSink::OnFirstFrameReceived(uint32_t ssrc) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "OnFirstFrameReceived: " << ssrc;
  if (ssrc_.has_value() || ssrc == 0) {
    return;
  }
  ssrc_ = ssrc;
  write_file_.open(absl::StrCat(file_location_, ssrc, "_audio.pcm"),
                   std::ios::out | std::ios::binary | std::ios::app);
}

void AudioSink::OnFrame(
    const meet::MeetAudioSinkInterface::MeetAudioFrame& frame) {
  // Avoid logging empty noise frames until we've received an actual frame from
  // a valid SSRC source.
  absl::MutexLock lock(&mutex_);
  if (!ssrc_.has_value()) {
    return;
  }

  if (!write_file_.is_open()) {
    return;
  }

  // This can be played back later with the following command:
  //   $ ffplay -f s16le -ar 48k -ac 1 <filename>
  for (int16_t sample : frame.audio_data.pcm16) {
    write_file_.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
  }
}

void VideoSink::OnFrame(
    const meet::MeetVideoSinkInterface::MeetVideoFrame& frame) {
  // Avoid logging empty noise frames until we've received an actual frame from
  // a valid SSRC source.
  absl::MutexLock lock(&mutex_);
  if (!ssrc_.has_value()) {
    return;
  }

  if (!write_file_.is_open()) {
    return;
  }

  write_file_ << "csrc: " << frame.csrc.value_or(0) << ": "
              << frame.frame.height() << " x " << frame.frame.width() << "\n";
}

void VideoSink::OnFirstFrameReceived(uint32_t ssrc) {
  absl::MutexLock lock(&mutex_);
  LOG(INFO) << "OnFirstFrameReceived: " << ssrc;
  if (ssrc_.has_value() || ssrc == 0) {
    return;
  }
  ssrc_ = ssrc;
  write_file_.open(absl::StrCat(file_location_, ssrc, "_video.txt"),
                   std::ios::app);
}

rtc::scoped_refptr<meet::MeetVideoSinkInterface>
SinkFactory::CreateVideoSink() {
  return rtc::make_ref_counted<VideoSink>(file_location_);
}

rtc::scoped_refptr<meet::MeetAudioSinkInterface>
SinkFactory::CreateAudioSink() {
  return rtc::make_ref_counted<AudioSink>(file_location_);
}

void SessionObserver::OnResourceUpdate(ResourceUpdate update) {
  int64_t request_id = 0;
  absl::Status status = absl::OkStatus();

  switch (update.hint) {
    case meet::ResourceHint::kMediaEntries:
      LOG(INFO) << "Received media entries update: "
                << MediaEntriesStringify(update.media_entries_update.value());
      break;
    case meet::ResourceHint::kParticipants:
      LOG(INFO) << "Received participants update: "
                << ParticipantsStringify(update.participants_update.value());
      break;
    case meet::ResourceHint::kSessionControl:
      LOG(INFO) << "Received session control update: "
                << SessionControlStringify(
                       update.session_control_update.value());

      if (update.session_control_update.has_value() &&
          update.session_control_update.value().response.has_value()) {
        request_id =
            update.session_control_update.value().response.value().request_id;
        status = update.session_control_update.value().response.value().status;
      } else {
        LOG(ERROR) << "Session control update did not contain a response.";
      }
      break;
    case meet::ResourceHint::kVideoAssignment:
      LOG(INFO) << "Received video assignment update: "
                << VideoAssignmentStringify(
                       update.video_assignment_update.value());

      if (update.video_assignment_update.has_value() &&
          update.video_assignment_update.value().response.has_value()) {
        request_id =
            update.video_assignment_update.value().response.value().request_id;
        status = update.video_assignment_update.value().response.value().status;
      } else {
        LOG(ERROR) << "Video assignment update did not contain a response.";
      }
      break;
    default:
      LOG(INFO) << "Received unknown resource update: ";
      break;
  }

  absl::MutexLock lock(&response_callback_map_mutex_);
  if (auto& callback = response_callbacks_[request_id];
      callback != nullptr && request_id != 0) {
    callback(request_id, status);
    response_callbacks_.erase(request_id);
  }
}

void SessionObserver::OnResourceRequestFailure(ResourceRequestError error) {
  LOG(INFO) << "OnResourceRequestFailure: " << error.status;

  int64_t request_id = error.request_id;
  absl::MutexLock lock(&response_callback_map_mutex_);
  if (auto& callback = response_callbacks_[request_id]; callback != nullptr) {
    callback(request_id, error.status);
    response_callbacks_.erase(request_id);
  }
}

void SessionObserver::SetResourceResponseCallback(
    int64_t request_id, ResourceResponseCallback callback) {
  absl::MutexLock lock(&response_callback_map_mutex_);
  response_callbacks_[request_id] = std::move(callback);
}

void SessionObserver::OnClientStateUpdate(meet::MeetMediaApiClientState state) {
  switch (state) {
    case meet::MeetMediaApiClientState::kReady:
      // The client does not signal this state.
      break;
    case meet::MeetMediaApiClientState::kConnecting:
      LOG(INFO) << "Client state changed to connecting.";
      break;
    case meet::MeetMediaApiClientState::kJoining:
      LOG(INFO) << "Client state changed to joining.";
      break;
    case meet::MeetMediaApiClientState::kJoined:
      LOG(INFO) << "Client state changed to joined.";
      break;
    case meet::MeetMediaApiClientState::kDisconnected:
      LOG(INFO) << "Client state changed to disconnected.";
      break;
    case meet::MeetMediaApiClientState::kFailed:
      LOG(INFO) << "Client state changed to failed.";
      break;
  }
}

}  // namespace media_api_impls
