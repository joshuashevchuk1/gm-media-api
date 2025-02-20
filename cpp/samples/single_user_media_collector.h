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

#ifndef CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_
#define CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/rtc_base/thread.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL once absl can be bumped
// to a version that supports it.

namespace media_api_samples {

// A basic media collector that collects audio and video streams from the
// conference. This is primarily useful for experimenting with media processing
// without having to worry about managing participant metadata. All audio goes
// to a single file, and all video goes to a single file. Therefore, this
// collector is best used for collecting data in a conference with a single
// participant.
class SingleUserMediaCollector : public meet::MediaApiClientObserverInterface {
 public:
  SingleUserMediaCollector(absl::string_view output_file_prefix,
                           std::unique_ptr<rtc::Thread> collector_thread)
      : output_file_prefix_(output_file_prefix),
        collector_thread_(std::move(collector_thread)) {}

  ~SingleUserMediaCollector() override {
    // Stop the thread to ensure that enqueued tasks do not access member fields
    // after they have been destroyed.
    collector_thread_->Stop();
  }

  void OnAudioFrame(meet::AudioFrame frame) override;
  void OnVideoFrame(meet::VideoFrame frame) override;
  void HandleAudioBuffer(std::vector<int16_t> pcm16);
  void HandleVideoBuffer(rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer);

  void OnResourceUpdate(meet::ResourceUpdate update) override {
    // This sample does not handle resource updates.
  }

  void OnJoined() override {
    LOG(INFO) << "SingleUserMediaCollector::OnJoined";
    join_notification_.Notify();
  }
  void OnDisconnected(absl::Status status) override {
    LOG(INFO) << "SingleUserMediaCollector::OnDisconnected " << status;
    disconnect_notification_.Notify();
  }

  absl::Status WaitForJoined(absl::Duration timeout) {
    if (!join_notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for joined notification");
    }
    return absl::OkStatus();
  }
  absl::Status WaitForDisconnected(absl::Duration timeout) {
    if (!disconnect_notification_.WaitForNotificationWithTimeout(timeout)) {
      return absl::DeadlineExceededError(
          "Timed out waiting for disconnected notification");
    }
    return absl::OkStatus();
  }

 private:
  struct VideoFile {
    int file_number;
    int width;
    int height;
    std::ofstream file;
  };

  std::string output_file_prefix_;
  // Audio file for all audio frames.
  //
  // The audio file is created when the first audio frame is received. Audio
  // format does not change, so a single file can be used for all audio frames.
  absl::Nullable<std::unique_ptr<std::ofstream>> audio_file_;
  // The current video file, or nullptr if no video frames have been received
  // yet.
  //
  // The first video file is created when the first video frame is received. If
  // the video frame size changes, a new video file is created.
  absl::Nullable<std::unique_ptr<VideoFile>> video_file_;

  absl::Notification join_notification_;
  absl::Notification disconnect_notification_;

  std::unique_ptr<rtc::Thread> collector_thread_;
};

}  // namespace media_api_samples

#endif  // CPP_SAMPLES_SINGLE_USER_MEDIA_COLLECTOR_H_
