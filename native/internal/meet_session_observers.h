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

#ifndef NATIVE_INTERNAL_MEET_SESSION_OBSERVERS_H_
#define NATIVE_INTERNAL_MEET_SESSION_OBSERVERS_H_

// This file contains the implementation of observers required by the WebRTC API
// to handle events during the session creation and connection process.
//
// Each observer highlights the important implementation details necessary to
// successfully create a Media API session with Meet servers. They also place
// emphasis on the events of relevance when triggered by the WebRTC API.
//
// Because Meet Media API sessions have strict requirements for how they are
// created and connected, (e.g. Unified Plan only and "Receive-only"
// transceivers), certain events are not expected, ignored, or simply logged.
// Each implementation attempts to highlight important details every client
// should adhere to.

#include <utility>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "native/api/meet_media_api_client_interface.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/set_local_description_observer_interface.h"
#include "webrtc/api/set_remote_description_observer_interface.h"

namespace meet {

// Implements SetLocalDescriptionObserverInterface to handle the result of
// setting the local description on the peer connection.
//
// This observer is used during the initial session creation process. It is
// invoked when the local description is set on the peer connection.
//
// Not suitable for reuse. A new instance should be create for each local
// description that needs to be set.
class SetLocalDescriptionObserver
    : public webrtc::SetLocalDescriptionObserverInterface {
 public:
  void OnSetLocalDescriptionComplete(webrtc::RTCError error) override;

  absl::Status WaitWithTimeout(absl::Duration timeout);

 private:
  absl::Notification result_notification_;
  absl::Status result_status_ = absl::OkStatus();
};

// Implements SetRemoteDescriptionObserverInterface to handle the result of
// setting the remote description on the peer connection.
//
// This observer is used during the initial session creation process. It is
// invoked when the remote description is set on the peer connection.
//
// Not suitable for reuse. A new instance should be create for each remote
// description that needs to be set.
class SetRemoteDescriptionObserver
    : public webrtc::SetRemoteDescriptionObserverInterface {
 public:
  void OnSetRemoteDescriptionComplete(webrtc::RTCError error) override;

  absl::Status WaitWithTimeout(absl::Duration timeout);

 private:
  absl::Notification result_notification_;
  absl::Status result_status_ = absl::OkStatus();
};

class MeetPeerConnectionObserver : public webrtc::PeerConnectionObserver {
 public:
  // Callback to be invoked when a remote track is added to the peer connection.
  // This happens when accepting a SDP answer from the remote peer.
  using OnRemoteTrackAddedCallback = absl::AnyInvocable<void(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface>)>;

  MeetPeerConnectionObserver(
      rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
          api_session_observer,
      OnRemoteTrackAddedCallback remote_track_added_callback)
      : api_session_observer_(std::move(api_session_observer)),
        remote_track_added_callback_(std::move(remote_track_added_callback)) {};

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override;

  // No data channel will be opened by Meet servers. Any data channel opened
  // remotely is unexpected and will be closed.
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel) override;

  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override;

  // Triggered when a new ICE candidate is found. Currently ignored and not
  // logged. Its results will exist in the SDP.
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void OnTrack(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override;

 private:
  rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
      api_session_observer_;
  OnRemoteTrackAddedCallback remote_track_added_callback_;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEET_SESSION_OBSERVERS_H_
