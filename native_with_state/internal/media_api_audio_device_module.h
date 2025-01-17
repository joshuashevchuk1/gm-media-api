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

#ifndef NATIVE_WITH_STATE_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_
#define NATIVE_WITH_STATE_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_

#include <cstdint>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "webrtc/api/audio/audio_device.h"
#include "webrtc/api/audio/audio_device_defines.h"
#include "webrtc/modules/audio_device/include/audio_device_default.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {

// Very simple implementation of an AudioDeviceModule.
//
// WebRTC has platform dependent implementations. However they are not fully
// supported and have no guarantees of future compatibility. This is because
// the only truly supported AudioDeviceModule (ADM) is the one in Chrome.
// Everything else is a "use at your own risk" implementation.
//
// Because we cannot guarantee the platform this client will always run on,
// there's no guarantee an implementation won't compile using the
// `DummyAudioDeviceModule`. That WebRTC implementation does nothing and no
// audio will be provided.
//
// To overcome these challenges, this is a provided implementation that does
// the bare minimum to provide audio. Nothing more, nothing less. If an end
// user requires more functionality and complexity, they are relegated to
// rolling their own implementation.
class MediaApiAudioDeviceModule
    : public webrtc::webrtc_impl::AudioDeviceModuleDefault<
          webrtc::AudioDeviceModule> {
 public:
  MediaApiAudioDeviceModule() = default;

  ~MediaApiAudioDeviceModule() override { StopPlayout(); };

  int32_t InitPlayout() override;
  int32_t StopPlayout() override final;
  int32_t RegisterAudioCallback(webrtc::AudioTransport* callback) override;
  bool Playing() const override { return task_thread_ != nullptr; };

 private:
  // Periodically calls the registered audio callback, registered by WebRTC
  // internals, to provide audio data. It is to be invoked every 10ms with a
  // sampling rate of 48000 Hz. If this is not done, no audio will be provided
  // to the registered audio sinks with the RTPReceiver of the RTPTransceiver
  // that remote audio is being received on.
  void ProcessPlayData();

  mutable absl::Mutex mutex_;
  webrtc::AudioTransport* audio_callback_ ABSL_GUARDED_BY(mutex_) = nullptr;
  std::unique_ptr<rtc::Thread> task_thread_;
};

}  // namespace meet

#endif  // NATIVE_WITH_STATE_INTERNAL_MEDIA_API_AUDIO_DEVICE_MODULE_H_
