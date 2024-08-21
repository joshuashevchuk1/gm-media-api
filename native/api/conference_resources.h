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

#ifndef NATIVE_API_CONFERENCE_RESOURCES_H_
#define NATIVE_API_CONFERENCE_RESOURCES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/optional.h"

// TODO: b/347055783 - Update the docs for all the resource structs in this file
// and make it clear how resources are used. I.e. what each update is and how a
// client can/should react to them.

namespace meet {
//// Session Control Resource

// Tells the server that the client is about to disconnect. After receiving the
// response to this, the client should not expect to receive any other messages
// or media RTP. This is an optional request to terminate the session
// faster than it would be terminated otherwise by client inactivity timeout.
struct LeaveRequest {};

struct SessionControlRequest {
  int64_t request_id = 0;
  std::optional<LeaveRequest> leave_request;
};

// The top-level transport container for messages sent from client to
// server in the "session-control" data channel. Any combination of fields may
// be set, but the message is never empty.
struct SessionControlChannelFromClient {
  SessionControlRequest request;
};

// This is a singleton resource containing the status of the media session.
struct SessionStatus {
  enum class MeetingConnectionState {
    kUnknown,
    // Session is waiting to be admitted into the meeting.
    // The client may never observe this state if it was admitted or rejected
    // quickly.
    kWaiting,
    // Session has fully joined the meeting.
    kJoined,
    // Session is not connected to the meeting.
    kDisconnected,
  };

  MeetingConnectionState connection_state = MeetingConnectionState::kUnknown;
};

struct SessionControlResourceSnapshot {
  int64_t id;
  SessionStatus session_status;
};

struct LeaveResponse {};

// An optional response from Meet servers to an incoming request.
struct SessionControlResponse {
  int64_t request_id;
  // The response status from Meet servers to an incoming request. This should
  // be used by clients to determine the outcome of the request.
  absl::Status status;
  LeaveResponse leave_response;
};

// The top-level transport container for messages sent from server to
// client in the "session-control" data channel. Any combination of fields may
// be set, but the message is never empty.
struct SessionControlChannelToClient {
  std::optional<SessionControlResponse> response;
  std::vector<SessionControlResourceSnapshot> resources;
};

//// Media Entries Resource

struct MediaEntriesDeletedResource {
  // The resource ID of the resource being deleted.
  int64_t id = 0;
  std::optional<bool> media_entry;
};

struct MediaEntry {
  // An ID associated with the participant producing the stream. It is
  // used to correlate other media entries being produced by the same
  // participant. E.g. a participant active in the same meeting from multiple
  // devices.
  int32_t participant_id = 0;
  // The CSRC for any audio stream contributed by this participant. Will be
  // zero if no stream is provided.
  uint32_t audio_csrc = 0;
  // The CSRC for any video stream contributed by this participant. Will be
  // empty if no stream is provided.
  std::vector<uint32_t> video_csrcs;
  // Signals if the current entry is presenting.
  bool presenter = false;
  // Signals if the current entry is a screenshare.
  bool screenshare = false;
  // Signals if the audio stream is currently muted by the remote participant.
  bool audio_muted = false;
  // Signals if the video stream is currently muted by the remote participant.
  bool video_muted = false;
};

struct MediaEntriesResourceSnapshot {
  // The resource ID of the resource being updated.
  int64_t id = 0;
  std::optional<MediaEntry> media_entry;
};

// The top-level transport container for messages sent from server to
// client in the "media-entries" data channel.
struct MediaEntriesChannelToClient {
  // Resource snapshots. There is no implied order between the snapshots in the
  // list.
  std::vector<MediaEntriesResourceSnapshot> resources;
  // The list of deleted resources. There is no order between the entries in the
  // list.
  std::vector<MediaEntriesDeletedResource> deleted_resources;
};

//// Video Assignment Resource

// Required dimensions of the canvas.
struct CanvasDimensions {
  // The vertical space, in pixels, for this canvas.
  int32_t height = 480;
  // The horizontal space, in pixels, for this canvas.
  int32_t width = 640;
};

struct VideoCanvas {
  enum class AssignmentProtocol {
    kRelevant,
    kDirect,
  };
  // An identifier for the video canvas.
  // This is required and must be unique within the containing LayoutModel.
  // Clients should prudently reuse VideoCanvas IDs. This allows the backend
  // to keep assigning video streams to the same canvas as much as possible.
  int32_t id = 0;

  // The dimensions for this video canvas. Failure to provide this
  // will result in an error.
  CanvasDimensions dimensions;

  // The protocol that governs how the backend should assign a video
  // feed to this canvas.
  AssignmentProtocol assignment_protocol = AssignmentProtocol::kRelevant;
};

struct LayoutModel {
  // A client-specified identifier for this assignment. The identifier
  // will be used to reference a given LayoutModel in subsequent
  // VideoAssignment resource update pushed from server -> client.
  std::string label;

  // The canvases that videos are assigned to from each virtual ssrc.
  // Providing more canvases than exists virtual streams will result in
  // an error status.
  std::vector<VideoCanvas> canvases;
};

struct VideoResolution {
  // The height and width are in square pixels. For cameras that can change
  // orientation, the width refers to the measurement on the horizontal axis,
  // and the height on the vertical.
  int32_t height = 480;
  int32_t width = 640;
  // The frame rate in frames per second.
  int32_t frame_rate = 30;
};

struct SetVideoAssignmentRequest {
  // The new video layout to use. This replaces any previously active video
  // layout.
  LayoutModel layout_model;
  // The maximum video resolution the client wants to receive for any video
  // feed.
  VideoResolution video_resolution;
};

struct VideoAssignmentRequest {
  // A unique client-generated identifier for this request. Different requests
  // must never have the same request ID.
  int64_t request_id = 0;

  std::optional<SetVideoAssignmentRequest> set_video_assignment_request;
};

// The top-level transport container for messages sent from client to
// server in the "video-assignment" data channel.
struct VideoAssignmentChannelFromClient {
  VideoAssignmentRequest request;
};

struct VideoAssignmentResponse {
  struct SetVideoAssignmentResponse {};

  // The request ID in the request this is the response to.
  int64_t request_id = 0;
  // The response status for this request. This should be used by clients to
  // determine the RPC result.
  absl::Status status;
  std::optional<SetVideoAssignmentResponse> set_assignment;
};

struct VideoCanvasAssignment {
  // The video canvas the video should be shown in.
  int32_t canvas_id = 0;
  // The virtual video SSRC that the video will be sent over, or zero if
  // there is no video from the participant.
  uint32_t ssrc = 0;
  // The `MediaEntry.id` of the media whose video is being shown.
  int32_t media_entry_id = 0;
};

struct VideoAssignment {
  // The LayoutModel that this assignment is based on. Taken from the
  // LayoutModel.label field.
  std::string label;
  // The individual canvas assignments, in no particular order.
  std::vector<VideoCanvasAssignment> canvases;
};

// A resource snapshot managed by the server and replicated to the client.
struct VideoAssignmentResourceSnapshot {
  // The resource ID of the resource being updated. For singleton
  // resources, this is zero.
  int64_t id = 0;

  std::optional<VideoAssignment> assignment;
};

// The top-level transport container for messages sent from server to
// client in the "video-assignment" data channel. Any combination of fields may
// be set, but the message is never empty.
struct VideoAssignmentChannelToClient {
  // An optional response to a incoming request.
  std::optional<VideoAssignmentResponse> response;
  // Resource snapshots. There is no implied order between the snapshots in the
  // list.
  std::vector<VideoAssignmentResourceSnapshot> resources;
};

enum class ResourceHint {
  kUnknownResource,
  kSessionControl,
  kVideoAssignment,
  kMediaEntries,
  kParticipants,
  kStats,
};

// Contains an update for a single resource.  Hint will always be set for the
// resource update that is populated.
//
// See MeetMediaApiSessionObserver for more information.
struct ResourceUpdate {
  ResourceHint hint;
  std::optional<SessionControlChannelToClient> session_control_update;
  std::optional<VideoAssignmentChannelToClient> video_assignment_update;
  std::optional<MediaEntriesChannelToClient> media_entries_update;
  std::optional<MediaEntriesChannelToClient> participants_update;
  std::optional<MediaEntriesChannelToClient> stats_update;
};

// Contains a request for a single resource. The hint must always be present and
// indicate which resource the request is intended for.
//
// See MeetMediaApiClientInterface `SendRequest` method for more information.
struct ResourceRequest {
  ResourceHint hint;
  std::optional<const SessionControlChannelFromClient> session_control_request;
  std::optional<const VideoAssignmentChannelFromClient>
      video_assignment_request;
};

}  // namespace meet

#endif  // NATIVE_API_CONFERENCE_RESOURCES_H_
