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

#ifndef NATIVE_API_MEET_MEDIA_API_CLIENT_INTERFACE_H_
#define NATIVE_API_MEET_MEDIA_API_CLIENT_INTERFACE_H_

// This file contains the MeetMediaApiClient interface. It is designed to
// utilize the PeerConnection interface as defined in
// https://w3c.github.io/webrtc-pc/#peer-to-peer-connections.
//
// It demonstrates "how-to", and establishes the required configurations and
// SCTP/SRTP connections with Meet servers. These connections enable the
// streaming of conference metadata, video, and audio streams from Google Meet
// conferences to the client.
//
// Note that this is a reference client. It is not intended to be a complete SDK
// with full support, customization, nor optimizations of WebRTC and real time
// communication.
//
// All conference media streams are "receive-only". Currently, Meet Media API
// does not support sending of media from the MeetMediaApiClient interface into
// a conference.
//
// API requests from the client intended to affect application state of a
// conference or received media (e.g. change video resolution), are transmitted
// via SCTP data channels. This is in contrast to typical API requests over
// HTTP or RPC.
//
// The following steps are needed to setup a typical Media API session:
//
// 1. Create an implementation of the MeetMediaSinkFactoryInterface. This
// factory should provide implementations of `MeetVideoSinkInterface` and
// `MeetAudioSinkInterface` when requested. Check `meet_media_sink_interface.h`
// for more information.
//
// 2. Create an implementation of the `MeetMediaApiSessionObserverInterface`.
// This is used to notify the client of various events that occur during the
// initialization and lifetime of a Media API session. Check
// interface definition for more information.
//
// 3. Create a MeetMediaApiClientInterface instance with the
// static `Create` factory method. Check function signature for more
// information about input parameters.
//
// 4. Call `ConnectActiveConference` with the appropriate parameters. This
// initiates the connection with Meet servers.
//
// 5. Wait for the `OnResourceUpdate` callback to be invoked with a
// `ResourceHint::kSessionControl` hint. This indicates the session is fully
// joined into the conference and should be receiving audio streams.
//
// 6. If video was enabled, send a `SetVideoAssignment` request via
// `SendRequest` method of the `MeetMediaApiClientInterface`. No video will be
// transmitted from Meet servers to the client until a successful request has
// been sent. Check `conference_resources.h` for more information.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/media_entries_resource.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/api/participants_resource.h"
#include "native/api/session_control_resource.h"
#include "native/api/video_assignment_resource.h"
#include "webrtc/api/ref_count.h"
#include "webrtc/api/scoped_refptr.h"

namespace meet {

struct MeetMediaApiClientConfiguration {
  // For values greater than zero, the Media API client will establish that many
  // video SRTP streams. After the session is initialized, no other streams will
  // be created nor intentionally terminated. All connections will be cleaned up
  // after the session is complete. Up to three streams are supported and they
  // are "receive-only". Attempts to set a value greater than three will result
  // in an error.
  uint32_t receiving_video_stream_count = 0;
  // If audio is enabled, three "receive-only" audio SRTP streams will be
  // created, always. After the session is initialized, no other streams will be
  // created nor intentionally terminated. All connections will be cleaned up
  // after the session is complete.
  bool enable_audio_streams = false;
  // If enabled, the client will signal a dedicated data channel with Meet
  // servers. The channel will be used to receive media entries resource
  // updates. This resource pushes updates to the client when new media streams
  // become available in the conference and the ID of the remote participant
  // that owns the streams. Conversely, it also communicates when media streams
  // are removed from the conference. If disabled, no data channel will be
  // opened for this resource.
  bool enable_media_entries_resource = false;
  // If enabled, the client will signal a dedicated data channel with Meet
  // servers. The channel will be used to receive participants resource updates.
  // This resource pushes updates to the client when participants join or leave
  // the conference. If disabled, no data channel will be opened for this
  // resource.
  bool enable_participants_resource = false;
  // If enabled, the client will signal a dedicated data channel with Meet
  // servers. The channel will be used to receive video assignment resource
  // updates and send video assignment requests. This resource pushes updates to
  // the client to communicate the assignment of a RTP stream, identified by its
  // SSRC, to a video canvas identified by a unique ID. This canvas describes
  // the resolution and fps of the video stream. It also provides needed
  // metadata to associate the stream with the remote participant that owns it.
  // Request from client to Meet servers on this channel allow the client to
  // alter these canvases and video assignments. If disabled, no channel will be
  // opened for this resource. This is ideal for sessions that are audio only.
  bool enable_video_assignment_resource = false;
};

// This enum is used to indicate which resource a request or update is
// associated with.
//
// See `MeetMediaApiSessionObserverInterface::ResourceUpdate`,
// `MeetMediaApiSessionObserverInterface::ResourceRequestError`, and
// `MeetMediaApiClientInterface::ResourceRequest` for more information.
enum class ResourceHint {
  kUnknownResource,
  kSessionControl,
  kVideoAssignment,
  kMediaEntries,
  kParticipants,
  kStats,
};

// MeetMediaApiSessionObserverInterface notifies the integrating client of
// various events that occur during the initialization and lifetime of a Media
// API session.
//
// It is through this interface that communication of failures and state changes
// will be notified. Also, resource update pushes from Meet servers are notified
// via this interface.
//
// Observer callback logic should be moved to a different thread if the work is
// non-trivial.
class MeetMediaApiSessionObserverInterface : public webrtc::RefCountInterface {
 public:
  // Contains an update for a single resource.  Hint will always be set for the
  // resource update that is populated. Only one resource will be populated for
  // a single update. Every request is expected to have a unique request ID.
  // Hence, clients can use this to determine exactly which request this update
  // is associated with.
  struct ResourceUpdate {
    ResourceHint hint;
    std::optional<SessionControlChannelToClient> session_control_update;
    std::optional<VideoAssignmentChannelToClient> video_assignment_update;
    std::optional<MediaEntriesChannelToClient> media_entries_update;
    std::optional<ParticipantsChannelToClient> participants_update;
    std::optional<MediaEntriesChannelToClient> stats_update;
  };

  // The ResourceHint indicates which resource experienced a transmission
  // failure. Every request is expected to have a unique request ID. Hence,
  // clients can use this to determine exactly which request failed if multiple
  // were in flight. The status will indicate the reason for the failure.
  struct ResourceRequestError {
    ResourceHint hint;
    int64_t request_id;
    absl::Status status;
  };

  virtual ~MeetMediaApiSessionObserverInterface() override = default;

  // Invoked when a resource update is received from Meet servers.
  virtual void OnResourceUpdate(ResourceUpdate update) = 0;

  // Invoked when a resource request fails after transmission begins.
  virtual void OnResourceRequestFailure(ResourceRequestError error) = 0;
  // TODO: Add callback for session state change. The media api
  // state should only be communicated via this callback. Internally it is
  // affected by the peer connection state, data channel states, and join flow
  // state.
};

class MeetMediaApiClientInterface {
 public:
  // Contains a request for a single resource. The hint must always be present
  // and indicate which resource the request is intended for. Only one request
  // should be populated for a single request.
  //
  // See MeetMediaApiClientInterface `SendRequest` method for more information.
  struct ResourceRequest {
    ResourceHint hint;
    std::optional<const SessionControlChannelFromClient>
        session_control_request;
    std::optional<const VideoAssignmentChannelFromClient>
        video_assignment_request;
  };

  virtual ~MeetMediaApiClientInterface() = default;

  // Attempts to connect with Meet servers. This process involves
  // communicating the intent to join an active Meet conference. It establishes
  // the signaled SRTP and SCTP connections with the backend.
  //
  // A successful connection does not mean the client is fully joined into the
  // conference. Active participants are first notified and given the
  // option to reject the client. Once allowed by participants into the
  // conference, the client will receive a resource push across the
  // `session-control` data channel with the updated session state.
  //
  // Once fully joined, if audio was enabled, the client will begin receiving
  // any available streams immediately. If video was enabled, the client will
  // not receive any video streams until a `SetVideoAssignment` request is
  // successfully sent to Meet servers and applied..
  //
  // The provided join_endpoint must be a valid URL including the protocol and
  // host. There aren't very robust checks performed on the provided URL. It is
  // expected that the provided URL is well formed.
  virtual absl::Status ConnectActiveConference(
      absl::string_view join_endpoint, absl::string_view conference_id,
      absl::string_view access_token) = 0;

  // Convenience method for sending a `SessionControlChannelFromClient` request
  // with a `LeaveRequest` to Meet servers. This tells the server that the
  // client is about to disconnect. The request will use the provided request
  // ID.
  //
  // If successful, a `LeaveResponse` will be sent via the `OnResourceUpdate`
  // callback in the `MeetMediaApiSessionObserverInterface`. If an error occurs
  // while transmitting the request, the `OnResourceRequestFailure` callback
  // will be invoked instead. Both the response and error will contain the
  // request ID and therefore can be traced back to a specific request.
  //
  // After receiving the `LeaveResponse`, the client should not expect to
  // receive any other messages or media RTP. This is an optional request to
  // terminate the session faster than waiting for termination due to client
  // inactivity timeout.
  virtual absl::Status LeaveConference(int64_t request_id) = 0;

  // Sends a resource request to Meet servers. The request is sent over the
  // appropriate resource data channel.
  //
  // If the returned status is not OK, this indicates an issue prior to
  // transmitting the request. An OK status indicates that transmission
  // successfully began. An OK status does NOT guarantee a successful response
  // or even a successful transmission.
  //
  // Successful transmission will be reported via the `OnResourceUpdate`
  // callback in the `MeetMediaApiSessionObserverInterface`.
  //
  // Transmission and server-side failures will be reported via the
  // `OnResourceRequestFailure` callback in the
  // `MeetMediaApiSessionObserverInterface`.
  //
  // Requests must have a non-zero, unique `request_id` in the nested request.
  // For example, a `SessionControlRequest`'s `request_id` must be non-zero and
  // unique to other requests' `request_id`s. The `request_id` can be used to
  // associate the request to the response or error in the
  // `MeetMediaApiSessionObserverInterface`.
  virtual absl::Status SendRequest(const ResourceRequest& request) = 0;

  // Returns the local description of the configured peer connection for the
  // created client. The description is passed up "as-is" from WebRTC.
  virtual absl::StatusOr<std::string> GetLocalDescription() const = 0;

  // Creates a new instance of MeetMediaApiClientInterface. It is configured
  // with the required codecs to support streaming media from Meet conferences.
  // Required SCTP data channels will be opened and proper number of SRTP
  // streams will be signaled with Meet servers. Host system must support the
  // following video codecs:
  // - VP8, VP9, AV1
  //
  // Host system must support the following audio codecs:
  // - Opus
  static absl::StatusOr<std::unique_ptr<MeetMediaApiClientInterface>> Create(
      const MeetMediaApiClientConfiguration& api_config,
      rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
          api_session_observer,
      rtc::scoped_refptr<MeetMediaSinkFactoryInterface> sink_factory);
};

}  // namespace meet

#endif  // NATIVE_API_MEET_MEDIA_API_CLIENT_INTERFACE_H_
