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
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/meet_media_sink_interface.h"
#include "webrtc/api/scoped_refptr.h"

namespace media_api_impls {

class VideoSink : public meet::MeetVideoSinkInterface {
 public:
  explicit VideoSink(std::string file_location)
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
  explicit AudioSink(std::string file_location)
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
  explicit SinkFactory(std::string file_location)
      : file_location_(std::move(file_location)) {};
  ~SinkFactory() override = default;
  rtc::scoped_refptr<meet::MeetVideoSinkInterface> CreateVideoSink() override;
  rtc::scoped_refptr<meet::MeetAudioSinkInterface> CreateAudioSink() override;

 private:
  std::string file_location_;
};

class SessionObserver : public meet::MeetMediaApiSessionObserverInterface {
 public:
  // Callback for a specific request id that returns the response status for the
  // request.
  using ResourceResponseCallback =
      absl::AnyInvocable<void(int64_t request_id, absl::Status status)>;

  SessionObserver() = default;
  ~SessionObserver() override = default;
  void OnResourceUpdate(ResourceUpdate update) override;
  void OnResourceRequestFailure(ResourceRequestError error) override;

  // Sets a callback for a specific request id.
  //
  // Only one callback can be set at a time for a request id. Setting a new
  // callback will overwrite the existing callback if one exists.
  //
  // Callbacks are removed after being called.
  void SetResourceResponseCallback(int64_t request_id,
                                   ResourceResponseCallback callback);

 private:
  mutable absl::Mutex response_callback_map_mutex_;
  absl::flat_hash_map<int64_t, ResourceResponseCallback> response_callbacks_
      ABSL_GUARDED_BY(response_callback_map_mutex_);
};

}  // namespace media_api_impls

#endif  // NATIVE_EXAMPLES_MEDIA_API_IMPLS_H_
