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

#ifndef NATIVE_INTERNAL_MEET_MEDIA_STREAMS_H_
#define NATIVE_INTERNAL_MEET_MEDIA_STREAMS_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "native/api/meet_media_sink_interface.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/rtp_receiver_interface.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/video_sink_interface.h"

namespace meet {

// Base class for `MeetVideoStreamTrack` and `MeetAudioStreamTrack`.
//
// Responsible for maintaining the webrtc track level metadata regardless
// of the media type. E.g. the SSRC and webrtc::RtpReceiverInterface.
class MeetMediaStreamTrack {
 protected:
  explicit MeetMediaStreamTrack(
      std::string mid, rtc::scoped_refptr<MeetMediaSinkInterface> external_sink,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      : mid_(std::move(mid)),
        external_sink_(std::move(external_sink)),
        receiver_(std::move(receiver)) {};
  virtual ~MeetMediaStreamTrack() = default;

  // Upon receiving the first media frame, it extracts and notifies the external
  // sink of the SSRC associated with the media stream track.
  void MaybeNotifyFirstFrameReceived(uint32_t ssrc);

  std::string mid_;
  rtc::scoped_refptr<MeetMediaSinkInterface> external_sink_;
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver_;
  std::optional<uint32_t> ssrc_;
  absl::Mutex mutex_;
  bool first_frame_received_ ABSL_GUARDED_BY(mutex_) = false;
};

// `MeetVideoStreamTrack` is responsible for forwarding video frames to the
// external sink.
//
// It processes frame level metadata in accordance with the logic expected for
// Meet media api sessions E.g. virtual video SSRCs. VVSSRCs is not a concept
// native to webrtc. Hence `MeetVideoStreamTrack` acts as a sink for webrtc,
// and processes the data provided by webrtc in accordance to the semantics
// of virtual SSRCs before forwarding the data to the external sink.
class MeetVideoStreamTrack : public rtc::VideoSinkInterface<webrtc::VideoFrame>,
                             protected MeetMediaStreamTrack {
 public:
  explicit MeetVideoStreamTrack(
      std::string mid, rtc::scoped_refptr<MeetVideoSinkInterface> external_sink,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      : MeetMediaStreamTrack(std::move(mid), external_sink,
                             std::move(receiver)),
        video_sink_(std::move(external_sink)) {};

  void OnFrame(const webrtc::VideoFrame& frame) override;

 private:
  rtc::scoped_refptr<MeetVideoSinkInterface> video_sink_;
};

class MeetAudioStreamTrack : public webrtc::AudioTrackSinkInterface,
                             protected MeetMediaStreamTrack {
 public:
  explicit MeetAudioStreamTrack(
      std::string mid, rtc::scoped_refptr<MeetAudioSinkInterface> external_sink,
      rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
      : MeetMediaStreamTrack(std::move(mid), external_sink,
                             std::move(receiver)),
        audio_sink_(std::move(external_sink)) {};

  void OnData(const void* audio_data, int bits_per_sample, int sample_rate,
              size_t number_of_channels, size_t number_of_frames,
              absl::optional<int64_t> absolute_capture_timestamp_ms) override;

 private:
  rtc::scoped_refptr<MeetAudioSinkInterface> audio_sink_;
};

// MeetMediaStreamManager is responsible for creating and managing all media
// sinks.
//
// For each remote track added, the manager will create a meet media stream
// track for the expected media type. The meet stream track implements the
// webrtc interface for the media type. External sinks are created from the
// factory provided by the integrating codebase. Meet media stream tracks
// are responsible for forwarding all media frames to external sinks after
// extracting the appropriate metadata, e.g. the contributing source identifiers
// (CSRCs) and synchronization source identifiers (SSRCs).
//
// See the `MeetVideoStreamTrack` and `MeetAudioStreamTrack` for more details.
class MeetMediaStreamManager {
 public:
  explicit MeetMediaStreamManager(
      rtc::scoped_refptr<MeetMediaSinkFactoryInterface> sink_factory)
      : sink_factory_(std::move(sink_factory)) {};

  ~MeetMediaStreamManager() = default;

  // MeetMediaStreamManager is neither copyable nor movable.
  MeetMediaStreamManager(const MeetMediaStreamManager&) = delete;
  MeetMediaStreamManager& operator=(const MeetMediaStreamManager&) = delete;

  void OnRemoteTrackAdded(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);

 private:
  rtc::scoped_refptr<MeetMediaSinkFactoryInterface> sink_factory_;
  std::vector<std::unique_ptr<MeetVideoStreamTrack>> video_stream_tracks_;
  std::vector<std::unique_ptr<MeetAudioStreamTrack>> audio_stream_tracks_;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEET_MEDIA_STREAMS_H_
