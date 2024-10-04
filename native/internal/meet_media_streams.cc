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

#include "native/internal/meet_media_streams.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "native/api/meet_media_sink_interface.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/rtp_packet_info.h"
#include "webrtc/api/rtp_packet_infos.h"
#include "webrtc/api/rtp_receiver_interface.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/transport/rtp/rtp_source.h"
#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/video_source_interface.h"

namespace meet {
namespace {
constexpr int kLoudestIndicator = 42;
}  // namespace

void MeetMediaStreamTrack::MaybeNotifyFirstFrameReceived(uint32_t ssrc) {
  absl::MutexLock lock(&mutex_);
  if (first_frame_received_) {
    return;
  }

  if (ssrc == 0) {
    LOG(WARNING) << "First frame received for stream with MID and MSID" << mid_
                 << receiver_->stream_ids().at(0) << " without SSRC.";
    return;
  }

  // If this is the first frame this stream has received with a valid SSRC,
  // notify the external sink of the SSRC value.
  first_frame_received_ = true;
  ssrc_ = ssrc;
  external_sink_->OnFirstFrameReceived(ssrc);
}

void MeetVideoStreamTrack::OnFrame(const ::webrtc::VideoFrame& frame) {
  std::optional<uint32_t> csrc;
  uint32_t ssrc = 0;
  const webrtc::RtpPacketInfos& packet_infos = frame.packet_infos();

  // A single video frame will be made up of multiple packets. Each packet will
  // contain the contributing source identifiers (CSRCs). As the name implies,
  // these are the source of the frame, i.e. "who made it". Since the frame is
  // generated from a single source, we can simply extract the first CSRC from
  // the first packet.
  if (!packet_infos.empty()) {
    const webrtc::RtpPacketInfo& packet_info = packet_infos.front();
    ssrc = packet_info.ssrc();
    if (!packet_info.csrcs().empty()) {
      csrc = packet_info.csrcs().at(0);
    }
  }

  MaybeNotifyFirstFrameReceived(ssrc);
  video_sink_->OnFrame({.frame = frame, .csrc = csrc});
}

void MeetAudioStreamTrack::OnData(
    const void* audio_data, int bits_per_sample, int sample_rate,
    size_t number_of_channels, size_t number_of_frames,
    absl::optional<int64_t> absolute_capture_timestamp_ms) {
  if (bits_per_sample != 16) {
    LOG(ERROR) << "Unsupported bits per sample: " << bits_per_sample
               << ". Expected 16.";
    return;
  }

  const auto* pcm_data = reinterpret_cast<const int16_t*>(audio_data);
  MeetAudioSinkInterface::MeetAudioFrame::AudioData data;
  data.pcm16 =
      absl::MakeConstSpan(pcm_data, number_of_channels * number_of_frames);
  data.sample_rate = sample_rate;
  data.bits_per_sample = bits_per_sample;
  data.number_of_channels = number_of_channels;
  data.number_of_frames = number_of_frames;

  std::optional<int32_t> csrc;
  for (const auto& rtp_source : receiver_->GetSources()) {
    if (csrc.has_value() && ssrc_.has_value()) {
      break;
    }
    if (!csrc.has_value() &&
        rtp_source.source_type() == webrtc::RtpSourceType::CSRC &&
        rtp_source.source_id() != kLoudestIndicator) {
      csrc = rtp_source.source_id();
    }
    if (!ssrc_.has_value() &&
        rtp_source.source_type() == webrtc::RtpSourceType::SSRC &&
        // It is possible for some of the sources to have a source id of 0.
        // This is not a valid SSRC value within the context of a media api
        // session. So we ignore it.
        rtp_source.source_id() != 0) {
      ssrc_ = rtp_source.source_id();
    }
  }

  MaybeNotifyFirstFrameReceived(ssrc_.value_or(0));
  audio_sink_->OnFrame({.audio_data = data, .csrc = csrc});
}

void MeetMediaStreamManager::OnRemoteTrackAdded(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver =
      transceiver->receiver();
  cricket::MediaType media_type = receiver->media_type();
  rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> receiver_track =
      receiver->track();
  // MID should always exist since Meet only supports BUNDLE srtp streams
  std::string mid = transceiver->mid().has_value() ? *transceiver->mid() : "";

  if (media_type == cricket::MediaType::MEDIA_TYPE_VIDEO) {
    auto video_stream_track = std::make_unique<MeetVideoStreamTrack>(
        mid, sink_factory_->CreateVideoSink(), std::move(receiver));

    auto video_receiver =
        static_cast<webrtc::VideoTrackInterface*>(receiver_track.get());
    video_receiver->AddOrUpdateSink(video_stream_track.get(),
                                    rtc::VideoSinkWants());

    video_stream_tracks_.push_back(std::move(video_stream_track));
    return;
  }

  if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO) {
    auto audio_stream_track = std::make_unique<MeetAudioStreamTrack>(
        mid, sink_factory_->CreateAudioSink(), std::move(receiver));

    auto audio_receiver =
        static_cast<webrtc::AudioTrackInterface*>(receiver_track.get());
    audio_receiver->AddSink(audio_stream_track.get());

    audio_stream_tracks_.push_back(std::move(audio_stream_track));
    return;
  }

  LOG(WARNING) << "Received remote track of unsupported media type: "
               << media_type;
}

}  // namespace meet
