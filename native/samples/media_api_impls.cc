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

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "native/api/meet_media_sink_interface.h"
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

}  // namespace media_api_impls
