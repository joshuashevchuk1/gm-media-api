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

#ifndef NATIVE_API_MEDIA_STATS_RESOURCE_H_
#define NATIVE_API_MEDIA_STATS_RESOURCE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"

// TODO: Update the docs for all the resource structs in this file
// and make it clear how resources are used. I.e. what each update is and how a
// client can/should react to them.

namespace meet {

struct MediaStatsResponse {
  struct UploadMediaStatsResponse {};

  int64_t request_id = 0;
  // The response status from Meet servers to an incoming request. This should
  // be used by clients to determine the outcome of the request.
  absl::Status status;
  std::optional<UploadMediaStatsResponse> method;
};

// The configuration for the media stats upload. This will be sent by the server
// to the client when the data channel is opened. The client is then expected to
// start uploading media stats at the specified interval.
//
// This configuration is immutable and a singleton and will only be sent once
// when the data channel is opened.
struct MediaStatsConfiguration {
  // The interval between each upload of media stats. If this is zero, the
  // client should not upload any media stats.
  int32_t upload_interval_seconds = 0;
  // A map of allowlisted RTCStats sections. The key is the section type, and
  // the value is a vector of the names of data that are allowlisted for that
  // section.
  //
  // Allowlisted sections and section data are expected to be uploaded by the
  // client. Other data will be ignored by the server and can be safely
  // omitted.
  absl::flat_hash_map<std::string, std::vector<std::string>> allowlist;
};

// A resource snapshot managed by the server and replicated to the client.
struct MediaStatsResourceSnapshot {
  int64_t id = 0;
  MediaStatsConfiguration configuration;
};

// The top-level transport container for messages sent from server to client in
// the "media-stats" data channel. Any combination of fields may be set, but the
// message is never empty.
struct MediaStatsChannelToClient {
  // An optional response to an incoming request.
  std::optional<MediaStatsResponse> response;
  // Resource snapshots.
  std::optional<std::vector<MediaStatsResourceSnapshot>> resources;
};

// See https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-codec.
struct MediaStatsCodecSection {
  // https://www.w3.org/TR/webrtc-stats/#dom-rtccodecstats.
  std::string mime_type;
  uint32_t payload_type;
};

// See https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-inbound-rtp.
struct MediaStatsInboundRtpSection {
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcrtpstreamstats.
  std::string codec_id;
  std::string kind;
  uint32_t ssrc;
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcreceivedrtpstreamstats.
  double jitter;
  int64_t packets_lost;
  uint64_t packets_received;
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcinboundrtpstreamstats.
  uint64_t bytes_received;
  double jitter_buffer_delay;
  uint64_t jitter_buffer_emitted_count;
  double jitter_buffer_minimum_delay;
  double jitter_buffer_target_delay;
  double total_audio_energy;
  uint32_t fir_count;
  uint32_t frame_height;
  uint32_t frame_width;
  uint32_t frames_decoded;
  uint32_t frames_dropped;
  double frames_per_second;
  uint32_t frames_received;
  uint32_t freeze_count;
  uint32_t key_frames_decoded;
  uint32_t nack_count;
  uint32_t pli_count;
  uint64_t retransmitted_packets_received;
  double total_freezes_duration;
  double total_pauses_duration;
  double audio_level;
  uint64_t concealed_samples;
  uint64_t total_samples_received;
  double total_samples_duration;
};

// See https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-candidate-pair.
struct MediaStatsCandidatePairSection {
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcicecandidatepairstats.
  double available_outgoing_bitrate;
  uint64_t bytes_received;
  uint64_t bytes_sent;
  uint64_t consent_requests_sent;
  double current_round_trip_time;
  std::string local_candidate_id;
  uint32_t packets_discarded_on_send;
  uint64_t packets_sent;
  std::string remote_candidate_id;
  uint64_t requests_sent;
  uint64_t responses_received;
  std::string transport_id;
};

// See https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-media-playout.
struct MediaStatsMediaPlayoutSection {
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcmediaplayoutstats
  double synthesized_samples_duration;
  uint32_t synthesized_samples_events;
  double total_samples_duration;
  double total_playout_delay;
  uint64_t total_samples_count;
};

// See https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-transport.
struct MediaStatsTransportSection {
  // https://www.w3.org/TR/webrtc-stats/#transportstats-dict*.
  std::string selected_candidate_pair_id;
};

// See
// https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-local-candidate and
// https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype-remote-candidate.
struct MediaStatsIceCandidateSection {
  std::string address;
  std::string candidate_type;
  int32_t port;
  std::string protocol;
  std::string network_type;
};

// This type represents a RTCStats-derived dictionary contained in
// https://w3c.github.io/webrtc-pc/#rtcstatsreport-object which is returned by
// calling `RTCPeerConnection::getStats`.
struct MediaStatsSection {
  // https://www.w3.org/TR/webrtc-stats/#dom-rtcstatstype
  enum class MediaStatsSectionType {
    kUnknown,
    kCodec,
    kInboundRtp,
    kCandidatePair,
    kMediaPlayout,
    kTransport,
    kLocalIceCandidate,
    kRemoteIceCandidate
  };

  // https://w3c.github.io/webrtc-pc/#dom-rtcstats
  std::string id;
  // The type of the section. `section` will be populated with the section
  // corresponding to this type.
  MediaStatsSectionType type;
  // Represents the additional data (excluding the fields in `RTCStats`) of the
  // derived dictionary, depending on the type value in
  // https://w3c.github.io/webrtc-pc/#dom-rtcstats.
  //
  // Only one of the following sections will be populated (indicated by `type`).
  // If `type` is `kUnknown`, then none of the sections will be populated.
  std::optional<MediaStatsCodecSection> codec;
  std::optional<MediaStatsInboundRtpSection> inbound_rtp;
  std::optional<MediaStatsCandidatePairSection> candidate_pair;
  std::optional<MediaStatsMediaPlayoutSection> media_playout;
  std::optional<MediaStatsTransportSection> transport;
  std::optional<MediaStatsIceCandidateSection> local_ice_candidate;
  std::optional<MediaStatsIceCandidateSection> remote_ice_candidate;
};

// Uploads media stats from the client to the server. The stats are retrieved
// from WebRTC by calling `RTCPeerConnection::getStats` and the returned
// RTCStatsReport can be easily mapped to the sections below.
struct UploadMediaStatsRequest {
  // Represents the entries in
  // https://w3c.github.io/webrtc-pc/#dom-rtcstatsreport.
  std::vector<MediaStatsSection> sections;
};

struct MediaStatsRequest {
  // A unique client-generated identifier for this request. Different requests
  // must never have the same request ID.
  int64_t request_id = 0;
  // Request payload.
  std::optional<UploadMediaStatsRequest> upload_media_stats;
};

// The top-level transport container for messages sent from client to server in
// the "media-stats" data channel.
struct MediaStatsChannelFromClient {
  MediaStatsRequest request;
};

}  // namespace meet

#endif  // NATIVE_API_MEDIA_STATS_RESOURCE_H_
