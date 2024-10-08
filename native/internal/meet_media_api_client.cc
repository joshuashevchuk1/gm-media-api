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

#include "native/internal/meet_media_api_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "url/gurl.h"
#include "nlohmann/json.hpp"
#include "native/api/meet_media_api_client_interface.h"
#include "native/api/meet_media_sink_interface.h"
#include "native/internal/conference_resource_data_channel.h"
#include "native/internal/curl_request.h"
#include "native/internal/media_entries_resource_handler.h"
#include "native/internal/meet_connect_response.h"
#include "native/internal/meet_media_api_audio_device_module.h"
#include "native/internal/meet_media_streams.h"
#include "native/internal/meet_session_observers.h"
#include "native/internal/participants_resource_handler.h"
#include "native/internal/session_control_resource_handler.h"
#include "native/internal/video_assignment_resource_handler.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/audio_codecs/opus_audio_decoder_factory.h"
#include "webrtc/api/create_peerconnection_factory.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_direction.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "webrtc/api/video_codecs/video_encoder_factory_template.h"
#include "webrtc/api/video_codecs/video_encoder_factory_template_libvpx_vp9_adapter.h"

namespace meet {
namespace {
constexpr absl::Duration kAsyncWorkWaitTime = absl::Seconds(3);
constexpr absl::string_view kMediaEntriesDataChannelLabel = "media-entries";
constexpr absl::string_view kParticipantsDataChannelLabel = "participants";
constexpr absl::string_view kVideoAssignmentDataChannelLabel =
    "video-assignment";
constexpr absl::string_view kSessionControlDataChannelLabel = "session-control";

using Json = ::nlohmann::json;

webrtc::PeerConnectionInterface::RTCConfiguration GetRtcConfiguration() {
  webrtc::PeerConnectionInterface::RTCConfiguration config;
  config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;

  // This will never be a point of concern since Meet servers only support
  // bundling. This is mostly set to ensure unit testing is possible with no
  // issues. If no media descriptions exist after configuring the peer
  // connection, and this is set to anything other than `kBundlePolicyBalanced`,
  // the peer connection will fail to create an offer.
  config.bundle_policy = webrtc::PeerConnectionInterface::kBundlePolicyBalanced;
  config.rtcp_mux_policy =
      webrtc::PeerConnectionInterface::kRtcpMuxPolicyRequire;

  // Meet servers do not actually care about the remote client's ICE candidates.
  // Using the default STUN server should be sufficient for now.
  webrtc::PeerConnectionInterface::IceServer stun_server;
  stun_server.urls.push_back("stun:stun.l.google.com:19302");
  config.servers.push_back(stun_server);
  return config;
}

absl::StatusOr<std::unique_ptr<rtc::Thread>> CreateRunningThread(
    absl::string_view name) {
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::Create();
  thread->SetName(name, nullptr);
  if (!thread->Start()) {
    return absl::InternalError(absl::StrCat("Failed to start thread: ", name));
  }
  return thread;
}

absl::string_view MaybeRemoveTrailingSlash(absl::string_view str) {
  if (str.empty() || str.size() == 1) {
    return str;
  }
  if (str.back() == '/') {
    return str.substr(0, str.size() - 1);
  }
  return str;
}

}  // namespace

absl::StatusOr<std::unique_ptr<MeetMediaApiClientInterface>>
MeetMediaApiClientInterface::Create(
    const MeetMediaApiClientConfiguration& api_config,
    rtc::scoped_refptr<MeetMediaApiSessionObserverInterface>
        api_session_observer,
    rtc::scoped_refptr<MeetMediaSinkFactoryInterface> sink_factory) {
  absl::StatusOr<std::unique_ptr<rtc::Thread>> worker_create_status =
      CreateRunningThread("media_api_worker_thread");
  if (!worker_create_status.ok()) {
    return worker_create_status.status();
  }
  std::unique_ptr<rtc::Thread> worker_thread = *std::move(worker_create_status);

  absl::StatusOr<std::unique_ptr<rtc::Thread>> signaling_create_status =
      CreateRunningThread("media_api_signaling_thread");
  if (!signaling_create_status.ok()) {
    return signaling_create_status.status();
  }
  std::unique_ptr<rtc::Thread> signaling_thread =
      *std::move(signaling_create_status);

  auto adm = worker_thread->BlockingCall([&]() {
    // Audio device module should be created on the worker thread.
    return rtc::make_ref_counted<MeetMediaApiAudioDeviceModule>();
  });

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory = webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(), std::move(adm),
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          std::make_unique<webrtc::VideoEncoderFactoryTemplate<
              webrtc::LibvpxVp9EncoderTemplateAdapter>>(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  auto curl_request_factory = std::make_unique<CurlRequestFactory>();

  InternalConfigurations configurations = {
      .receiving_video_stream_count = api_config.receiving_video_stream_count,
      .enable_audio_streams = api_config.enable_audio_streams,
      .enable_media_entries_resource = api_config.enable_media_entries_resource,
      .enable_participants_resource = api_config.enable_participants_resource,
      .enable_video_assignment_resource =
          api_config.enable_video_assignment_resource,
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = true,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = std::move(api_session_observer),
      .sink_factory = std::move(sink_factory),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory),
  };

  return CreateMeetMediaApiClient(std::move(configurations));
}

MeetMediaApiClientState MeetMediaApiClient::state() const {
  // TODO: Return the client's state.
  return MeetMediaApiClientState::kReady;
}

absl::Status MeetMediaApiClient::SendRequest(const ResourceRequest& request) {
  // TODO: Check if the client is in the correct state to send
  // requests and return an error if not.

  switch (request.hint) {
    case ResourceHint::kSessionControl:
      if (session_control_data_channel_ == nullptr) {
        return absl::FailedPreconditionError(
            "Session control data channel is not enabled.");
      }

      if (!request.session_control_request.has_value()) {
        return absl::InvalidArgumentError(
            "Received session control hint, but Session control request is not "
            "set.");
      }
      return signaling_thread_->BlockingCall([&]() {
        return session_control_data_channel_->SendRequest(
            request.session_control_request.value());
      });
    case ResourceHint::kVideoAssignment:
      if (video_assignment_data_channel_ == nullptr) {
        return absl::FailedPreconditionError(
            "Video assignment data channel is not enabled.");
      }

      if (!request.video_assignment_request.has_value()) {
        return absl::InvalidArgumentError(
            "Received video assignment hint, but Video assignment request is "
            "not set.");
      }

      return signaling_thread_->BlockingCall([&]() {
        return video_assignment_data_channel_->SendRequest(
            request.video_assignment_request.value());
      });
      // TDOO: b/347055783 - Add support for Stats resource
      // updates once they've been implemented in the backend.
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unrecognized or unsupported resource request: ", request.hint));
  }
}

absl::Status MeetMediaApiClient::ConnectActiveConference(
    absl::string_view join_endpoint, absl::string_view conference_id,
    absl::string_view access_token) {
  absl::StatusOr<std::string> local_description_status = GetLocalDescription();
  if (!local_description_status.ok()) {
    return local_description_status.status();
  }

  GURL url(std::string(MaybeRemoveTrailingSlash(join_endpoint)));
  if (!url.is_valid()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid request join endpoint provided: ", join_endpoint));
  }

  // endpoint has form `url + /spaces/conference_id:connectActiveConference`
  // For example: ".../spaces/IVScp6C6QNkB:connectActiveConference"
  return signaling_thread_->BlockingCall([&] {
    return Connect(absl::StrCat(url.spec(), "/spaces/", conference_id,
                                ":connectActiveConference"),
                   access_token, *std::move(local_description_status));
  });
}

absl::Status MeetMediaApiClient::Connect(absl::string_view full_join_endpoint,
                                         absl::string_view access_token,
                                         std::string local_description) {
  nlohmann::basic_json<> offer_json;
  offer_json["offer"] = std::move(local_description);
  std::string offer_json_string = offer_json.dump();

  LOG(INFO) << "Join request offer: " << offer_json_string;

  std::unique_ptr<CurlRequest> request = curl_request_factory_->Create();
  request->SetRequestUrl(std::string(full_join_endpoint));
  request->SetRequestHeader("Content-Type", "application/json;charset=UTF-8");
  request->SetRequestHeader("Authorization",
                            absl::StrCat("Bearer ", access_token));
  request->SetRequestBody(std::move(offer_json_string));

  absl::Status send_result = request->Send();
  if (!send_result.ok()) {
    return send_result;
  }

  absl::StatusOr<MeetConnectResponse> response_parse_status =
      MeetConnectResponse::FromRequestResponse(request->GetResponseData());
  if (!response_parse_status.ok()) {
    return response_parse_status.status();
  }

  MeetConnectResponse connect_response = *std::move(response_parse_status);
  if (!connect_response.status.ok()) {
    return connect_response.status;
  }

  return SetRemoteDescription(std::move(connect_response.answer));
}

absl::Status MeetMediaApiClient::SetRemoteDescription(std::string answer) {
  LOG(INFO) << "Answer from Meet servers: " << answer;

  webrtc::SdpParseError error;
  std::unique_ptr<webrtc::SessionDescriptionInterface> answer_desc =
      webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer,
                                       std::move(answer), &error);
  if (answer_desc == nullptr) {
    return absl::InternalError(
        absl::StrCat("Failed to parse answer SDP: ", error.description));
  }

  auto set_remote_description_observer =
      webrtc::make_ref_counted<SetRemoteDescriptionObserver>();
  peer_connection_->SetRemoteDescription(std::move(answer_desc),
                                         set_remote_description_observer);
  absl::Status wait_status =
      set_remote_description_observer->WaitWithTimeout(kAsyncWorkWaitTime);
  if (!wait_status.ok()) {
    return wait_status;
  }

  // Final results of setting the remote description will be notified via the
  // callbacks in `MeetPeerConnectionObserver`. There's nothing more to do here.
  return absl::OkStatus();
}

absl::Status MeetMediaApiClient::LeaveConference(int64_t request_id) {
  return SendRequest(ResourceRequest{
      .hint = ResourceHint::kSessionControl,
      .session_control_request = SessionControlChannelFromClient{
          .request = SessionControlRequest{.request_id = request_id,
                                           .leave_request = LeaveRequest()}}});
}

absl::StatusOr<std::string> MeetMediaApiClient::GetLocalDescription() const {
  return signaling_thread_->BlockingCall(
      [&]() { return GetLocalDescriptionInternal(); });
}

absl::StatusOr<std::string> MeetMediaApiClient::GetLocalDescriptionInternal()
    const {
  const webrtc::SessionDescriptionInterface* desc =
      peer_connection_->local_description();
  if (desc == nullptr) {
    return absl::InternalError(
        "Local description is null. This should never happen, yet here we "
        "are.");
  }
  std::string local_description;
  desc->ToString(&local_description);
  return std::move(local_description);
}

absl::StatusOr<std::unique_ptr<MeetMediaApiClientInterface>>
CreateMeetMediaApiClient(InternalConfigurations configurations) {
  if (configurations.receiving_video_stream_count > 3 ||
      configurations.receiving_video_stream_count < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid number of receiving video streams: ",
                     configurations.receiving_video_stream_count));
  }

  auto media_stream_manager = std::make_unique<MeetMediaStreamManager>(
      std::move(configurations.sink_factory));

  // This callback will be invoked by `MeetPeerConnectionObserver` whenever
  // a new remote track is added. The `MeetMediaStreamManager` will then handle
  // the track appropriately. Because of the dependency graph, the owning object
  // must ensure that `MeetMediaStreamManager` must outlive the
  // `MeetPeerConnectionObserver`.
  auto remote_track_added_callback =
      [stream_manager = media_stream_manager.get()](
          rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
        if (stream_manager != nullptr) {
          stream_manager->OnRemoteTrackAdded(std::move(transceiver));
        }
      };

  auto observer = std::make_unique<MeetPeerConnectionObserver>(
      configurations.api_session_observer,
      std::move(remote_track_added_callback));
  webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::PeerConnectionInterface>>
      create_result = configurations.signaling_thread->BlockingCall([&] {
        return configurations.peer_connection_factory
            ->CreatePeerConnectionOrError(
                GetRtcConfiguration(),
                webrtc::PeerConnectionDependencies(observer.get()));
      });

  if (!create_result.ok()) {
    return absl::InternalError(absl::StrCat(
        "Failed to create peer connection: ", create_result.error().message()));
  }

  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection(
      create_result.MoveValue());

  if (configurations.enable_audio_streams) {
    for (int i = 0; i < 3; ++i) {
      webrtc::RtpTransceiverInit audio_init;
      audio_init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
      audio_init.stream_ids = {absl::StrCat("audio_stream_", i)};

      // We don't need the transceiver objects, but we do need to add them
      // so the proper media descriptions will be included in the offer SDP. The
      // receiver tracks of the added transceivers will be utilized when setting
      // the remote answer SDP at a later point. They are exposed in the
      // `OnTrack` callback of the `MeetPeerConnectionObserver`.
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
          audio_result = peer_connection->AddTransceiver(
              cricket::MediaType::MEDIA_TYPE_AUDIO, audio_init);

      if (!audio_result.ok()) {
        return absl::InternalError(
            absl::StrCat("Failed to add audio transceiver: ",
                         audio_result.error().message()));
      }
    }
  }

  for (uint32_t i = 0; i < configurations.receiving_video_stream_count; ++i) {
    webrtc::RtpTransceiverInit video_init;
    video_init.direction = webrtc::RtpTransceiverDirection::kRecvOnly;
    video_init.stream_ids = {absl::StrCat("video_stream_", i)};

    webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
        video_result = peer_connection->AddTransceiver(
            cricket::MediaType::MEDIA_TYPE_VIDEO, video_init);

    if (!video_result.ok()) {
      return absl::InternalError(absl::StrCat(
          "Failed to add video transceiver: ", video_result.error().message()));
    }
  }

  std::unique_ptr<MediaEntriesDataChannel> media_entries_channel;
  if (configurations.enable_media_entries_resource) {
    auto media_entries_resource_handler =
        std::make_unique<MediaEntriesResourceHandler>();
    auto media_entries_create_status = MediaEntriesDataChannel::Create(
        configurations.api_session_observer, peer_connection,
        kMediaEntriesDataChannelLabel,
        std::make_unique<MediaEntriesResourceHandler>(),
        configurations.worker_thread.get());
    if (!media_entries_create_status.ok()) {
      return media_entries_create_status.status();
    }
    media_entries_channel = *std::move(media_entries_create_status);
  }

  std::unique_ptr<ParticipantsDataChannel> participants_channel;
  if (configurations.enable_participants_resource) {
    auto participants_create_status = ParticipantsDataChannel::Create(
        configurations.api_session_observer, peer_connection,
        kParticipantsDataChannelLabel,
        std::make_unique<ParticipantsResourceHandler>(),
        configurations.worker_thread.get());
    if (!participants_create_status.ok()) {
      return participants_create_status.status();
    }
    participants_channel = *std::move(participants_create_status);
  }

  std::unique_ptr<VideoAssignmentDataChannel> video_assignment_channel;
  if (configurations.enable_video_assignment_resource) {
    auto video_assignment_create_status = VideoAssignmentDataChannel::Create(
        configurations.api_session_observer, peer_connection,
        kVideoAssignmentDataChannelLabel,
        std::make_unique<VideoAssignmentResourceHandler>(),
        configurations.worker_thread.get());
    if (!video_assignment_create_status.ok()) {
      return video_assignment_create_status.status();
    }
    video_assignment_channel = *std::move(video_assignment_create_status);
  }

  std::unique_ptr<SessionControlDataChannel> session_control_channel;
  // This is always required to be signaled by the client. Can only be disabled
  // for testing purposes.
  if (configurations.enable_session_control_data_channel) {
    auto session_control_create_status = SessionControlDataChannel::Create(
        configurations.api_session_observer, peer_connection,
        kSessionControlDataChannelLabel,
        std::make_unique<SessionControlResourceHandler>(),
        configurations.worker_thread.get());
    if (!session_control_create_status.ok()) {
      return session_control_create_status.status();
    }
    session_control_channel = *std::move(session_control_create_status);
  }

  auto set_local_description_observer =
      webrtc::make_ref_counted<SetLocalDescriptionObserver>();
  configurations.signaling_thread->PostTask(
      [peer_connection, set_local_description_observer]() {
        peer_connection->SetLocalDescription(set_local_description_observer);
      });

  absl::Status wait_status =
      set_local_description_observer->WaitWithTimeout(kAsyncWorkWaitTime);
  if (!wait_status.ok()) {
    return wait_status;
  }

  // Fields will be in an undefined state after creating the MeetMediaApiClient.
  // The client will take ownership of the data channels.
  MeetMediaApiClient::MediaApiSessionDataChannels data_channels = {
      .media_entries = std::move(media_entries_channel),
      .participants = std::move(participants_channel),
      .video_assignment = std::move(video_assignment_channel),
      .session_control = std::move(session_control_channel),
  };

  // Using `new` to access a non-public constructor.
  return absl::WrapUnique(new MeetMediaApiClient(
      std::move(configurations.signaling_thread),
      std::move(configurations.worker_thread),
      std::move(configurations.api_session_observer),
      std::move(media_stream_manager), std::move(observer), data_channels,
      std::move(peer_connection),
      std::move(configurations.curl_request_factory)));
}

}  // namespace meet
