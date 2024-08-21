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

#ifndef NATIVE_EXAMPLES_MEDIA_API_IMPLS_H_
#define NATIVE_EXAMPLES_MEDIA_API_IMPLS_H_

#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "native/api/conference_resources.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/samples/resource_parsers.h"
#include "webrtc/api/scoped_refptr.h"

namespace media_api_impls {

class VideoSink : public meet::MeetVideoSinkInterface {
 public:
  VideoSink(std::string file_location)
      : file_location_(std::move(file_location)) {};
  ~VideoSink() override = default;

  void OnFirstFrameReceived(uint32_t ssrc) override;

  void OnFrame(
      const meet::MeetVideoSinkInterface::MeetVideoFrame& frame) override;

 private:
  mutable absl::Mutex mutex_;
  std::optional<uint32_t> ssrc_ ABSL_GUARDED_BY(mutex_);
  std::ofstream write_file_;
  std::string file_location_;
};

class AudioSink : public meet::MeetAudioSinkInterface {
 public:
  AudioSink(std::string file_location)
      : file_location_(std::move(file_location)) {};

  ~AudioSink() override = default;

  void OnFirstFrameReceived(uint32_t ssrc) override;

  void OnFrame(
      const meet::MeetAudioSinkInterface::MeetAudioFrame& frame) override;

 private:
  mutable absl::Mutex mutex_;
  std::optional<uint32_t> ssrc_ ABSL_GUARDED_BY(mutex_);
  std::ofstream write_file_;
  std::string file_location_;
};

class SinkFactory : public meet::MeetMediaSinkFactoryInterface {
 public:
  SinkFactory(std::string file_location) : file_location_(file_location) {};
  ~SinkFactory() override = default;
  rtc::scoped_refptr<meet::MeetVideoSinkInterface> CreateVideoSink() override;
  rtc::scoped_refptr<meet::MeetAudioSinkInterface> CreateAudioSink() override;

 private:
  std::string file_location_;
};

class SessionObserver : public meet::MeetMediaApiSessionObserverInterface {
 public:
  SessionObserver() = default;
  ~SessionObserver() override = default;
  void OnResourceUpdate(meet::ResourceUpdate update) override {
    switch (update.hint) {
      case meet::ResourceHint::kMediaEntries:
        LOG(INFO) << "Received media entries update: "
                  << MediaEntriesStringify(update.media_entries_update.value());
        break;
      case meet::ResourceHint::kSessionControl:
        LOG(INFO) << "Received session control update: "
                  << SessionControlStringify(
                         update.session_control_update.value());
        LOG(INFO) << "Received session control update: "
                  << SessionControlStringify(
                         update.session_control_update.value());
        break;

      case meet::ResourceHint::kVideoAssignment:
        LOG(INFO) << "Received video assignment update: "
                  << VideoAssignmentStringify(
                         update.video_assignment_update.value());
        break;
      default:
        LOG(INFO) << "Received unknown resource update: ";
        break;
    }
  }
  void OnResourceRequestFailure(
      meet::MeetMediaApiSessionObserverInterface::ResourceRequestError error)
      override {
    LOG(INFO) << "OnResourceRequestFailure: " << error.status;
  }
};

}  // namespace media_api_impls

#endif  // NATIVE_EXAMPLES_MEDIA_API_IMPLS_H_
