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

#ifndef NATIVE_INTERNAL_CONFERENCE_RESOURCE_DATA_CHANNEL_H_
#define NATIVE_INTERNAL_CONFERENCE_RESOURCE_DATA_CHANNEL_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/media_entries_resource.h"
#include "native/api/media_stats_resource.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/participants_resource.h"
#include "native/api/session_control_resource.h"
#include "native/api/video_assignment_resource.h"
#include "native/internal/resource_handler_interface.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/task_queue/pending_task_safety_flag.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {

inline constexpr std::string ResourceHintToString(ResourceHint hint) {
  switch (hint) {
    case ResourceHint::kSessionControl:
      return "session-control";
    case ResourceHint::kVideoAssignment:
      return "video-assignment";
    case ResourceHint::kMediaEntries:
      return "media-entries";
    case ResourceHint::kParticipants:
      return "participants";
    case ResourceHint::kStats:
      return "stats";
    case ResourceHint::kUnknownResource:
      return "unknown-resource";
  }
}

inline constexpr ResourceHint StringToResourceHint(absl::string_view hint) {
  if (hint == "session-control") {
    return ResourceHint::kSessionControl;
  } else if (hint == "video-assignment") {
    return ResourceHint::kVideoAssignment;
  } else if (hint == "media-entries") {
    return ResourceHint::kMediaEntries;
  } else if (hint == "participants") {
    return ResourceHint::kParticipants;
  } else if (hint == "stats") {
    return ResourceHint::kStats;
  } else {
    return ResourceHint::kUnknownResource;
  }
}

// ConferenceResourceDataChannel manages the sending of requests to, and
// receiving of updates from, Meet servers for the resource it was created for.
//
// It takes ownership of the provider handler whose template type declares the
// resource it is intended to parse. Because Meet servers push resource updates
// in the form of JSON, the handler is utilized to parse the JSON string and
// convert it to a resource update object. This parsing is part of a processing
// chain that is is offloaded to the worker thread.
//
//                       conference resource data channel
//                     ┌ ------------------------------- ┐
// ┌         ┐   Json  |┌ ------ ┐   ┌ ----- ┐   ┌ ---- ┐|   ┌ ------ ┐
// |   Meet  |-------->||validate|-->|handler|-->|notify||-->|callback
// | servers |  Update |└ ------ ┘   └ ----- ┘   └ ---- ┘|   └ ------ ┘
// └         ┘         └ ------------------------------- ┘
//
// The end of the chain is a callback that is registered with the resource data
// channel instance. The callback is  owned by the integrating client.
//
// When clients send resource requests to Meet servers, the request is
// serialized to JSON and sent over the data channel. The provided handler is
// also utilized to serialize the request to JSON prior to transmission.
//
//                       conference resource data channel
//                      ┌ -------------------------------- ┐
// ┌         ┐  Struct  |┌ ---- ┐   ┌ ----- ┐   ┌ ------- ┐|   ┌ ------- ┐
// | Client  |--------->||verify|-->|handler|-->|stringify||-->|   Meet  |
// | Request | Request  |└ ---- ┘   └ ----- ┘   └ ------- ┘|   | Servers |
// └         ┘          └ -------------------------------- ┘   └ ------- ┘
//
// If WebRTC experiences an error while transmitting the request, the client
// will be notified via the provided MeetMediaApiSessionObserverInterface.
//
// Instances are always created on the worker thread provided to the factory
// `Create` method. It must always be the case that the thread is valid and
// running for this purpose.
template <typename ToClientUpdate, typename FromClientRequest>
class ConferenceResourceDataChannel : public webrtc::DataChannelObserver {
 public:
  // Creates a ConferenceResourceDataChannel.
  //
  // Does not take ownership of peer_connection or worker_thread. Both must not
  // be null. peer_connection is utilized only in the factory method.
  // worker_thread is expected to be valid for the lifetime of the created
  // instance. all processing is offloaded to the worker_thread. Hence,Thread
  // provider must ensure it outlives the created ConferenceResourceDataChannel
  // instance and be non-null.
  static absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel>> Create(
      rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
          api_session_observer,
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
      absl::string_view data_channel_label,
      std::unique_ptr<
          ResourceHandlerInterface<ToClientUpdate, FromClientRequest>>
          resource_handler,
      rtc::Thread* worker_thread);

  ~ConferenceResourceDataChannel() override {
    // The alive_flag_ was binded to the worker thread. Must be set to not alive
    // on the same thread it was attached to.
    worker_thread_.BlockingCall([&]() { alive_flag_->SetNotAlive(); });
  }

  // ConferenceResourceDataChannel is neither copyable nor movable.
  ConferenceResourceDataChannel(const ConferenceResourceDataChannel&) = delete;
  ConferenceResourceDataChannel& operator=(
      const ConferenceResourceDataChannel&) = delete;

  void OnStateChange() override;

  void OnMessage(const webrtc::DataBuffer& buffer) override;

  // Future WebRTC updates will force this to always be true. Ensure that
  // current behavior reflects desired future behavior.
  bool IsOkToCallOnTheNetworkThread() override { return true; }

  const std::string& label() const { return data_channel_label_; }

  // Queues up a request to be sent to Meet servers. Sent using WebRTC's
  // internal network thread. However the OnSuccess callback will be invoked on
  // the calling thread. Ensure to invoke on the signaling thread, always.
  absl::Status SendRequest(const FromClientRequest& request);

 private:
  ConferenceResourceDataChannel(
      rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
          api_session_observer,
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel,
      absl::string_view data_channel_label,
      std::unique_ptr<
          ResourceHandlerInterface<ToClientUpdate, FromClientRequest>>
          resource_handler,
      rtc::Thread* worker_thread)
      : api_session_observer_(std::move(api_session_observer)),
        data_channel_(std::move(data_channel)),
        data_channel_label_(data_channel_label),
        resource_handler_(std::move(resource_handler)),
        worker_thread_(*worker_thread),
        alive_flag_(webrtc::PendingTaskSafetyFlag::Create()) {
    data_channel_->RegisterObserver(this);
  };

  absl::StatusOr<MeetMediaApiSessionObserverInterface::ResourceUpdate>
  CreateResourceUpdate(ToClientUpdate update);
  int64_t ExtractRequestId(const FromClientRequest& request);

  rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
      api_session_observer_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel_;
  std::string data_channel_label_;
  std::unique_ptr<ResourceHandlerInterface<ToClientUpdate, FromClientRequest>>
      resource_handler_;

  rtc::Thread& worker_thread_;
  // Because all data channel work is offloaded to the worker thread, it may be
  // possible that the ConferenceResourceDataChannel is destroyed while there is
  // still work being processed on the worker thread. This flag is used to
  // ensure no further work is processed once the object is destroyed.
  rtc::scoped_refptr<webrtc::PendingTaskSafetyFlag> alive_flag_;
};

// Data channel for receiving media entries updates. This data channel does not
// support sending resource requests from the client to the server.
using MediaEntriesDataChannel =
    ConferenceResourceDataChannel<MediaEntriesChannelToClient,
                                  NoResourceRequestsFromClient>;
// Data channel for receiving participants updates. This data channel does not
// support sending resource requests from the client to the server.
using ParticipantsDataChannel =
    ConferenceResourceDataChannel<ParticipantsChannelToClient,
                                  NoResourceRequestsFromClient>;
// Data channel for sending video assignment resource requests and receiving
// video assignment resource updates.
using VideoAssignmentDataChannel =
    ConferenceResourceDataChannel<VideoAssignmentChannelToClient,
                                  VideoAssignmentChannelFromClient>;
// Data channel for sending media stats resource requests and receiving media
// stats resource updates.
using MediaStatsDataChannel =
    ConferenceResourceDataChannel<MediaStatsChannelToClient,
                                  MediaStatsChannelFromClient>;
// Data channel for sending session control resource requests and receiving
// session control resource updates.
using SessionControlDataChannel =
    ConferenceResourceDataChannel<SessionControlChannelToClient,
                                  SessionControlChannelFromClient>;

// Explicit instantiations in conference_resource_data_channel.cc file
extern template class ConferenceResourceDataChannel<
    MediaEntriesChannelToClient, NoResourceRequestsFromClient>;
extern template class ConferenceResourceDataChannel<
    ParticipantsChannelToClient, NoResourceRequestsFromClient>;
extern template class ConferenceResourceDataChannel<
    VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>;
extern template class ConferenceResourceDataChannel<
    SessionControlChannelToClient, SessionControlChannelFromClient>;
// Only used for testing
extern template class ConferenceResourceDataChannel<std::string, std::string>;

}  // namespace meet

#endif  // NATIVE_INTERNAL_CONFERENCE_RESOURCE_DATA_CHANNEL_H_
