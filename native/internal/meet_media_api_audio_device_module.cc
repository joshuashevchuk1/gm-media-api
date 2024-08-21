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

#include "native/internal/meet_media_api_audio_device_module.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "webrtc/api/audio/audio_device_defines.h"
#include "webrtc/api/units/time_delta.h"
#include "webrtc/rtc_base/thread.h"
#include "webrtc/rtc_base/time_utils.h"

namespace meet {
namespace {

// Audio is sampled at 48000 Hz (cycles per second)
constexpr int kAudioSampleRatePerMillisecond = 48;
constexpr int kSamplingIntervalMillis = 10;
// This should be configurable but we will produce a mono output for now.
constexpr int kNumberOfAudioChannels = 1;
constexpr int kBytesPerSample = sizeof(int16_t);

}  // namespace

int32_t MeetMediaApiAudioDeviceModule::RegisterAudioCallback(
    webrtc::AudioTransport* callback) {
  absl::MutexLock lock(&mutex_);
  audio_callback_ = callback;
  return 0;
}

int32_t MeetMediaApiAudioDeviceModule::InitPlayout() {
  if (task_thread_ != nullptr) {
    LOG(WARNING) << "Init called on an already initialized "
                    "MeetMediaApiAudioDeviceModule.";
    return -1;
  }

  task_thread_ = rtc::Thread::Create();
  task_thread_->SetName("MeetMediaApiAudioDeviceModule", nullptr);
  task_thread_->Start();
  task_thread_->PostTask([this]() { ProcessPlayData(); });
  return 0;
}

int32_t MeetMediaApiAudioDeviceModule::StopPlayout() {
  if (task_thread_ == nullptr) {
    return 0;
  }

  task_thread_->Stop();
  task_thread_.reset();
  return 0;
}

void MeetMediaApiAudioDeviceModule::ProcessPlayData() {
  int64_t process_start_time = rtc::TimeMillis();
  const size_t number_of_samples = kAudioSampleRatePerMillisecond *
                                   kSamplingIntervalMillis *
                                   kNumberOfAudioChannels;
  std::vector<int16_t> sample_buffer(number_of_samples);
  size_t samples_out = 0;
  int64_t elapsed_time_ms = -1;
  int64_t ntp_time_ms = -1;

  {
    absl::MutexLock lock(&mutex_);
    if (audio_callback_ != nullptr) {
      audio_callback_->NeedMorePlayData(
          number_of_samples, kBytesPerSample, kNumberOfAudioChannels,
          // Sampling rate in Hz
          kAudioSampleRatePerMillisecond * 1000, sample_buffer.data(),
          samples_out, &elapsed_time_ms, &ntp_time_ms);
    }
  }

  task_thread_->PostDelayedHighPrecisionTask(
      [this]() { ProcessPlayData(); },
      // Delay the next sampling for either:
      // 1. (sampling interval) - (time to process current sample)
      // 2. No delay if current processing took longer than the desired 10ms
      std::max(webrtc::TimeDelta::Millis(
                   (process_start_time + kSamplingIntervalMillis) -
                   rtc::TimeMillis()),
               webrtc::TimeDelta::Zero()));
}

}  // namespace meet
