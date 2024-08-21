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

#ifndef NATIVE_INTERNAL_MEET_MEDIA_API_CLIENT_H_
#define NATIVE_INTERNAL_MEET_MEDIA_API_CLIENT_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "native/api/conference_resources.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/internal/conference_resource_data_channel.h"
#include "native/internal/curl_request.h"
#include "native/internal/meet_media_streams.h"
#include "native/internal/meet_session_observers.h"
#include "native/internal/resource_handler_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/scoped_refptr.h"

namespace meet {

struct InternalConfigurations {
  uint32_t receiving_video_stream_count = 0;
  bool enable_audio_streams = false;
  bool enable_media_entries_resource = false;
  bool enable_video_assignment_resource = false;
  // Session control data channel is always required to be signaled by the
  // client with Meet servers. Hence the option is not exposed in the public
  // API. It is made available internally to allow for isolated unit testing of
  // each part of the factory logic.
  bool enable_session_control_data_channel = true;
  // The stats data channel is strongly recommended to be signaled by the
  // client with Meet servers. Hence the option is not exposed in the public
  // API. It is made available internally to allow for isolated unit testing of
  // each part of the factory logic.
  bool enable_stats_data_channel = true;

  // The following members are added here, as opposed to function arguments, to
  // ensure the destruction order of the dependencies injected into the factory
  // function. Because there are error checks in the factory that can return
  // early, it is possible that these dependencies never make it to their final
  // resting place in the MeetMediaApiClient. Hence, their destruction order as
  // function arguements is undefined. WebRTC has a nasty habit of posting tasks
  // to threads during the destruction process. Therefore, we must ensure the
  // threads always outlive the factory.
  std::unique_ptr<rtc::Thread> signaling_thread;
  std::unique_ptr<rtc::Thread> worker_thread;
  rtc::scoped_refptr<MeetMediaApiSessionObserverInterface> api_session_observer;
  rtc::scoped_refptr<MeetMediaSinkFactoryInterface> sink_factory;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory;
  std::unique_ptr<CurlRequestFactory> curl_request_factory;
};

// Factory function directly responsible for creating and
// injecting all dependencies required of the MeetMediaApiClient. It then
// returns a new concrete instance of MeetMediaApiClientInterface. It is invoked
// by MeetMediaApiClientInterface::Create.
absl::StatusOr<std::unique_ptr<MeetMediaApiClientInterface>>
CreateMeetMediaApiClient(InternalConfigurations configurations);

class MeetMediaApiClient : public MeetMediaApiClientInterface {
 public:
  // Just convenience aliases for specific conference resource data channels.
  using MediaEntriesDataChannel =
      ConferenceResourceDataChannel<MediaEntriesChannelToClient,
                                    NoResourceRequestsFromClient>;
  using VideoAssignmentDataChannel =
      ConferenceResourceDataChannel<VideoAssignmentChannelToClient,
                                    VideoAssignmentChannelFromClient>;
  using SessionControlDataChannel =
      ConferenceResourceDataChannel<SessionControlChannelToClient,
                                    SessionControlChannelFromClient>;

  // Contains all data channels that could possibly be opened for a Media API
  // session. Passed to the constructor as a parameter struct for ease of future
  // extension. New data channels should be added here as opposed to extending
  // the MeetMediaApiClient constructor. All members will be moved into and
  // owned by the created MeetMediaApiClient instance.
  struct MediaApiSessionDataChannels {
    std::unique_ptr<MediaEntriesDataChannel> media_entries;
    std::unique_ptr<VideoAssignmentDataChannel> video_assignment;
    std::unique_ptr<SessionControlDataChannel> session_control;
  };

  // MeetMediaApiClient is neither copyable nor movable.
  MeetMediaApiClient(const MeetMediaApiClient&) = delete;
  MeetMediaApiClient& operator=(const MeetMediaApiClient&) = delete;

  absl::Status ConnectActiveConference(absl::string_view join_endpoint,
                                       absl::string_view conference_id,
                                       absl::string_view access_token) override;

  // Queues up a resource request on the signaling thread.
  absl::Status SendRequest(const ResourceRequest& request) override;

  absl::StatusOr<std::string> GetLocalDescription() const override;

 private:
  explicit MeetMediaApiClient(
      std::unique_ptr<rtc::Thread> signaling_thread,
      std::unique_ptr<rtc::Thread> worker_thread,
      rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
          api_session_observer,
      std::unique_ptr<MeetMediaStreamManager> media_stream_manager,
      std::unique_ptr<MeetPeerConnectionObserver> observer,
      MediaApiSessionDataChannels& data_channels,
      rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection,
      std::unique_ptr<CurlRequestFactory> curl_request_factory)
      : signaling_thread_(std::move(signaling_thread)),
        worker_thread_(std::move(worker_thread)),
        api_session_observer_(std::move(api_session_observer)),
        media_stream_manager_(std::move(media_stream_manager)),
        observer_(std::move(observer)),
        media_entries_data_channel_(std::move(data_channels.media_entries)),
        video_assignment_data_channel_(
            std::move(data_channels.video_assignment)),
        session_control_data_channel_(std::move(data_channels.session_control)),
        peer_connection_(std::move(peer_connection)),
        curl_request_factory_(std::move(curl_request_factory)) {}

  // Ensures that creation of the client is only possible through the internal
  // factory.
  friend absl::StatusOr<std::unique_ptr<MeetMediaApiClientInterface>>
  CreateMeetMediaApiClient(InternalConfigurations configurations);

  absl::Status Connect(absl::string_view full_join_endpoint,
                       absl::string_view access_token,
                       std::string local_description);

  absl::Status SetRemoteDescription(std::string answer);

  absl::StatusOr<std::string> GetLocalDescriptionInternal() const;

  std::unique_ptr<rtc::Thread> signaling_thread_;
  std::unique_ptr<rtc::Thread> worker_thread_;
  rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
      api_session_observer_;
  std::unique_ptr<MeetMediaStreamManager> media_stream_manager_;
  std::unique_ptr<MeetPeerConnectionObserver> observer_;
  std::unique_ptr<MediaEntriesDataChannel> media_entries_data_channel_;
  std::unique_ptr<VideoAssignmentDataChannel> video_assignment_data_channel_;
  std::unique_ptr<SessionControlDataChannel> session_control_data_channel_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  std::unique_ptr<CurlRequestFactory> curl_request_factory_;
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_MEET_MEDIA_API_CLIENT_H_
