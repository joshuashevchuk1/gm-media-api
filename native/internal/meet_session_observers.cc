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

#include "native/internal/meet_session_observers.h"

#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/media_stream_interface.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"

namespace meet {

void SetLocalDescriptionObserver::OnSetLocalDescriptionComplete(
    webrtc::RTCError error) {
  if (!error.ok()) {
    result_status_ = absl::InternalError(
        absl::StrCat("Failed to set local description: ", error.message()));
  }

  result_notification_.Notify();
}

absl::Status SetLocalDescriptionObserver::WaitWithTimeout(
    absl::Duration timeout) {
  if (!result_notification_.WaitForNotificationWithTimeout(timeout)) {
    return absl::DeadlineExceededError(
        "Timed out waiting for local description to be set.");
  }

  return result_status_;
}

void SetRemoteDescriptionObserver::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  if (!error.ok()) {
    result_status_ = absl::InternalError(
        absl::StrCat("Failed to set remote description: ", error.message()));
  }

  result_notification_.Notify();
}

absl::Status SetRemoteDescriptionObserver::WaitWithTimeout(
    absl::Duration timeout) {
  if (!result_notification_.WaitForNotificationWithTimeout(timeout)) {
    return absl::DeadlineExceededError(
        "Timed out waiting for remote description to be set.");
  }

  return result_status_;
}

void MeetPeerConnectionObserver::OnSignalingChange(
    webrtc::PeerConnectionInterface::SignalingState new_state) {
  LOG(INFO) << "OnSignalingChange: "
            << webrtc::PeerConnectionInterface::AsString(new_state);
}

void MeetPeerConnectionObserver::OnDataChannel(
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) {
  LOG(WARNING) << "OnDataChannel unexpectedly triggered. Closing and ignoring "
                  "data channel with label: "
               << data_channel->label();
  data_channel->Close();
}

void MeetPeerConnectionObserver::OnConnectionChange(
    webrtc::PeerConnectionInterface::PeerConnectionState new_state) {}

void MeetPeerConnectionObserver::OnIceGatheringChange(
    webrtc::PeerConnectionInterface::IceGatheringState new_state) {
  LOG(INFO) << "OnIceGatheringChange: "
            << webrtc::PeerConnectionInterface::AsString(new_state);
}

void MeetPeerConnectionObserver::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {}

void MeetPeerConnectionObserver::OnTrack(
    rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
  // Parsing and reacting to remote tracks is handled by the
  // `MeetMediaStreamManager`.
  remote_track_added_callback_(std::move(transceiver));
}

}  // namespace meet
