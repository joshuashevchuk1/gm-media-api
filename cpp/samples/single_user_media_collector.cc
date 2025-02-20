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

#include "cpp/samples/single_user_media_collector.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "cpp/api/media_api_client_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame_buffer.h"
#include "webrtc/rtc_base/thread.h"

// TODO: Add ABSL_POINTERS_DEFAULT_NONNULL once absl can be bumped
// to a version that supports it.

namespace media_api_samples {

void SingleUserMediaCollector::OnAudioFrame(meet::AudioFrame frame) {
  // Copy the audio frame, since the frame is simply a view into an audio
  // buffer.
  std::vector<int16_t> pcm16(frame.pcm16.begin(), frame.pcm16.end());

  // Move audio processing to a separate thread since `OnAudioFrame`
  // implementations should move expensive work to a separate thread.
  collector_thread_->PostTask([this, pcm16 = std::move(pcm16)] {
    HandleAudioBuffer(std::move(pcm16));
  });
}

void SingleUserMediaCollector::OnVideoFrame(meet::VideoFrame frame) {
  // Move video processing to a separate thread since `OnVideoFrame`
  // implementations should move expensive work to a separate thread.
  collector_thread_->PostTask(
      [this, buffer = frame.frame.video_frame_buffer()] {
        HandleVideoBuffer(std::move(buffer));
      });
}

void SingleUserMediaCollector::HandleAudioBuffer(std::vector<int16_t> pcm16) {
  DCHECK(collector_thread_->IsCurrent());

  if (audio_file_ == nullptr) {
    std::string audio_output_file_name =
        absl::StrCat(output_file_prefix_, "_audio.pcm");

    LOG(INFO) << "Creating audio file: " << audio_output_file_name;
    audio_file_ = std::make_unique<std::ofstream>(
        audio_output_file_name,
        std::ios::binary | std::ios::out | std::ios::trunc);
  }

  for (int16_t sample : pcm16) {
    audio_file_->write(reinterpret_cast<const char*>(&sample), sizeof(sample));
  }
}

void SingleUserMediaCollector::HandleVideoBuffer(
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> buffer) {
  DCHECK(collector_thread_->IsCurrent());

  // If the video frame size changes, or if this is the first video frame,
  // create a new video file.
  if (video_file_ == nullptr || video_file_->width != buffer->width() ||
      video_file_->height != buffer->height()) {
    int file_number = video_file_ == nullptr ? 0 : video_file_->file_number + 1;
    std::string video_output_file_name =
        absl::StrCat(output_file_prefix_, "_video_", file_number, "_",
                     buffer->width(), "x", buffer->height(), ".yuv");

    LOG(INFO) << "Creating video file: " << video_output_file_name;
    video_file_ = std::make_unique<VideoFile>(
        file_number, buffer->width(), buffer->height(),
        std::ofstream(video_output_file_name,
                      std::ios::binary | std::ios::out | std::ios::trunc));
  }

  // Meet video frames are always in YUV420p format.
  const webrtc::I420BufferInterface* i420 = buffer->GetI420();

  int width = buffer->width();
  int height = buffer->height();
  int chroma_width = (width + 1) / 2;    // Increment by 1 for rounding
  int chroma_height = (height + 1) / 2;  // Increment by 1 for rounding

  // Write Y plane.
  const uint8_t* y_plane = i420->DataY();
  int y_stride = i420->StrideY();
  for (int i = 0; i < height; i++) {
    video_file_->file.write(
        reinterpret_cast<const char*>(y_plane + i * y_stride), width);
  }

  // Write U plane.
  const uint8_t* u_plane = i420->DataU();
  int u_stride = i420->StrideU();
  for (int i = 0; i < chroma_height; i++) {
    video_file_->file.write(
        reinterpret_cast<const char*>(u_plane + i * u_stride), chroma_width);
  }

  // Write V plane.
  const uint8_t* v_plane = i420->DataV();
  int v_stride = i420->StrideV();
  for (int i = 0; i < chroma_height; i++) {
    video_file_->file.write(
        reinterpret_cast<const char*>(v_plane + i * v_stride), chroma_width);
  }
}

}  // namespace media_api_samples
