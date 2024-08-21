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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include <curl/curl.h>
#include "nlohmann/json.hpp"
#include "native/api/conference_resources.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/internal/curl_request.h"
#include "native/internal/testing/mock_curl_api_wrapper.h"
#include "native/internal/testing/mock_curl_request_factory.h"
#include "native/internal/testing/mock_meet_media_api_session_observer.h"
#include "native/internal/testing/mock_meet_media_sink_factory.h"
#include "native/internal/testing/sdp_constants.h"
#include "webrtc/api/audio_codecs/builtin_audio_encoder_factory.h"
#include "webrtc/api/audio_codecs/opus_audio_decoder_factory.h"
#include "webrtc/api/create_peerconnection_factory.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/jsep.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/media_types.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_direction.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/set_local_description_observer_interface.h"
#include "webrtc/api/set_remote_description_observer_interface.h"
#include "webrtc/api/test/mock_data_channel.h"
#include "webrtc/api/test/mock_peer_connection_factory_interface.h"
#include "webrtc/api/test/mock_peerconnectioninterface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"
#include "webrtc/api/video_codecs/builtin_video_encoder_factory.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_dav1d_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp8_adapter.h"
#include "webrtc/api/video_codecs/video_decoder_factory_template_libvpx_vp9_adapter.h"
#include "webrtc/pc/session_description.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {
using ::testing::_;
using ::testing::An;
using ::testing::ByMove;
using ::testing::HasSubstr;
using ::testing::InvokeWithoutArgs;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::Values;
using ::testing::status::StatusIs;
using webrtc::RtpTransceiverDirection::kRecvOnly;
using Json = ::nlohmann::json;

// Add missing mock method to the WebRTC mock.
class MockPeerConnectionInterface
    : public ::webrtc::MockPeerConnectionInterface {
 public:
  using ::webrtc::MockPeerConnectionInterface::SetLocalDescription;
  MOCK_METHOD(
      void, SetLocalDescription,
      (rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>),
      (override));

  MOCK_METHOD(
      void, SetRemoteDescription,
      (std::unique_ptr<webrtc::SessionDescriptionInterface>,
       rtc::scoped_refptr<::webrtc::SetRemoteDescriptionObserverInterface>),
      (override));
};

std::unique_ptr<webrtc::SessionDescriptionInterface> StringToSessionDescription(
    webrtc::SdpType type, absl::string_view description) {
  std::unique_ptr<webrtc::SessionDescriptionInterface> offer =
      webrtc::CreateSessionDescription(type, std::string(description));
  CHECK(offer != nullptr);
  return offer;
}

// Produces a gmock `Action` which returns the given `peer_connection` instance
// as an RTCErrorOr.
auto ReturnPeerConnection(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection) {
  return InvokeWithoutArgs([peer_connection = std::move(peer_connection)]() {
    // RTCErrorOr is not copyable so produce a new one each time that returns
    // the same PeerConnectionInterface instance.
    return peer_connection;
  });
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfGettingLocalDescriptionFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });
  EXPECT_CALL(*peer_connection, local_description).WillOnce(Return(nullptr));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Local description is null")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfProvidedInvalidJoinEndpoint) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("random gibberish that's not a valid url",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid request join endpoint provided")));
}

TEST(MeetMediaApiClientTest, ConnectActiveConferenceFailsIfCurlRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*curl_api, EasyPerform)
      .WillOnce(Return(CURLcode::CURLE_HTTP_RETURNED_ERROR));

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Curl failed easy perform")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfInvalidMeetServerResponse) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        *str_response = absl::Cord("invalid meet server response");
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(
          absl::StatusCode::kInternal,
          HasSubstr("Unexpected or malformed response from Meet servers.")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfMeetServerFailsRequest) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        *str_response = absl::Cord(R"json({
                      "error": {
                        "code": 500,
                        "message": "Something went wrong. That's all we know.",
                        "status": "INTERNAL"
                      }
                    })json");
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Something went wrong. That's all we know.")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfInvalidMeetServerSdpAnswer) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        nlohmann::basic_json<> answer_json;
        answer_json["answer"] = "invalid sdp answer from Meet servers";
        *str_response = absl::Cord(answer_json.dump());
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Failed to parse answer SDP")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfSettingRemoteDescriptionFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });
  EXPECT_CALL(*peer_connection, SetRemoteDescription)
      .WillOnce(
          [](std::unique_ptr<webrtc::SessionDescriptionInterface> description,
             rtc::scoped_refptr<::webrtc::SetRemoteDescriptionObserverInterface>
                 observer) {
            observer->OnSetRemoteDescriptionComplete(
                webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        nlohmann::basic_json<> answer_json;
        answer_json["answer"] = std::string(kWebRtcAnswer);
        *str_response = absl::Cord(answer_json.dump());
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Failed to set remote description")));
}

TEST(MeetMediaApiClientTest,
     ConnectActiveConferenceFailsIfSettingRemoteDescriptionTimesOut) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });
  EXPECT_CALL(*peer_connection, SetRemoteDescription).Times(1);

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        nlohmann::basic_json<> answer_json;
        answer_json["answer"] = std::string(kWebRtcAnswer);
        *str_response = absl::Cord(answer_json.dump());
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->ConnectActiveConference("https://meet.googleapis.com/",
                                      "random_conference", "some_random_token"),
      StatusIs(
          absl::StatusCode::kDeadlineExceeded,
          HasSubstr("Timed out waiting for remote description to be set")));
}

TEST(MeetMediaApiClientTest, ConnectActiveConferenceSucceeds) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  ON_CALL(*peer_connection, AddTransceiver(An<cricket::MediaType>(), _))
      .WillByDefault(InvokeWithoutArgs([]() {
        return static_cast<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>(
            webrtc::MockRtpTransceiver::Create());
      }));
  ON_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillByDefault(InvokeWithoutArgs([]() {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      }));
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });
  EXPECT_CALL(*peer_connection, SetRemoteDescription)
      .WillOnce(
          [](std::unique_ptr<webrtc::SessionDescriptionInterface> description,
             rtc::scoped_refptr<::webrtc::SetRemoteDescriptionObserverInterface>
                 observer) {
            observer->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());
          });

  std::unique_ptr<webrtc::SessionDescriptionInterface> description =
      StringToSessionDescription(webrtc::SdpType::kOffer, kWebRtcOffer);
  ON_CALL(*peer_connection, local_description())
      .WillByDefault(Return(description.get()));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  auto curl_api = std::make_unique<MockCurlApiWrapper>();
  // When sending the connect request, curl_request `send` method will invoke
  // the EasySetOptPtr method of the curl_api for the header list first.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  // Then it will invoke EasySetOptPtr for the response data. This is where we
  // need to mock the response from Meet servers.
  EXPECT_CALL(*curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([](CURL *curl, CURLoption option, void *value) {
        absl::Cord *str_response = reinterpret_cast<absl::Cord *>(value);
        nlohmann::basic_json<> answer_json;
        answer_json["answer"] = std::string(kWebRtcAnswer);
        *str_response = absl::Cord(answer_json.dump());
        return CURLE_OK;
      });

  std::unique_ptr<MockCurlRequestFactory> curl_request_factory =
      MockCurlRequestFactory::CreateUnique();
  EXPECT_CALL(*curl_request_factory, Create())
      .WillOnce(
          Return(ByMove(std::make_unique<CurlRequest>(std::move(curl_api)))));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 1,
      .enable_audio_streams = true,
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = std::move(curl_request_factory)};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  EXPECT_OK(client->ConnectActiveConference("https://meet.googleapis.com/",
                                            "random_conference",
                                            "some_random_token"));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfCreatingPeerConnectionFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to create peer connection")));
}

TEST(MeetMediaApiClientTest, CreateMediaApiClientFailsIfAddingAudioFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection,
              AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO, _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to add audio transceiver")));
}

TEST(MeetMediaApiClientTest, CreateMediaApiClientFailsIfAddingVideoFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 1,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to add video transceiver")));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfCreatingVideoAssignmentDataChannelFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_video_assignment_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to create data channel")));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfCreatingMediaEntriesDataChannelFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to create data channel")));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfCreatingSessionControlDataChannelFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(Return(webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR)));

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to create data channel")));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfSettingLocalDescriptionFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_))
      .WillOnce(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(
                webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(CreateMeetMediaApiClient(std::move(configurations)),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Failed to set local description")));
}

TEST(MeetMediaApiClientTest,
     CreateMediaApiClientFailsIfSettingLocalDescriptionTimesOut) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, SetLocalDescription(_)).Times(1);

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  EXPECT_THAT(
      CreateMeetMediaApiClient(std::move(configurations)),
      StatusIs(
          absl::StatusCode::kDeadlineExceeded,
          HasSubstr("Timed out waiting for local description to be set.")));
}

TEST(MeetMediaApiClientTest, WithAudioSetsAudioTransceiversInDescription) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);

  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  ASSERT_THAT(sdp_content, SizeIs(3));
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[0].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[1].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[1].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[2].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[2].media_description()->direction(), kRecvOnly);
}

TEST(MeetMediaApiClientTest, WithMediaEntriesDataChannelInDescription) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .enable_media_entries_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  ASSERT_THAT(sdp_content, SizeIs(1));

  // NOTE: The actual label of the data channel is not communicated in the SDP.
  // Hence no explicit check for label is performed in tests. The best we can do
  // is ensure a data channel is negotiated in the SDP.
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest, WithVideoAssignmentDataChannelInDescription) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .enable_video_assignment_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  ASSERT_THAT(sdp_content, SizeIs(1));

  // NOTE: The actual label of the data channel is not communicated in the SDP.
  // Hence no explicit check for label is performed in tests. The best we can do
  // is ensure a data channel is negotiated in the SDP.
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest, WithSessionControlDataChannelInDescription) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  ASSERT_THAT(sdp_content, SizeIs(1));

  // NOTE: The actual label of the data channel is not communicated in the SDP.
  // Hence no explicit check for label is performed in tests. The best we can do
  // is ensure a data channel is negotiated in the SDP.
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest, WithNothingSetSdpHasNoMediaDescriptions) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  EXPECT_THAT(sdp_content, IsEmpty());
}

class VideoMediaDescriptionTest : public ::testing::TestWithParam<uint32_t> {};

TEST_P(VideoMediaDescriptionTest, SetsCorrectNumberOfVideoTransceivers) {
  const uint32_t num_video_streams = GetParam();
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory =
      webrtc::CreatePeerConnectionFactory(
          /*network_thread=*/nullptr, worker_thread.get(),
          signaling_thread.get(),
          /*audio_device_module=*/nullptr,
          webrtc::CreateBuiltinAudioEncoderFactory(),
          webrtc::CreateOpusAudioDecoderFactory(),
          webrtc::CreateBuiltinVideoEncoderFactory(),
          std::make_unique<webrtc::VideoDecoderFactoryTemplate<
              webrtc::LibvpxVp8DecoderTemplateAdapter,
              webrtc::LibvpxVp9DecoderTemplateAdapter,
              webrtc::Dav1dDecoderTemplateAdapter>>(),
          /*audio_mixer=*/nullptr, /*audio_processing=*/nullptr);

  InternalConfigurations configurations = {
      .receiving_video_stream_count = num_video_streams,
      .enable_audio_streams = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  ASSERT_EQ(sdp_content.size(), num_video_streams);

  for (uint32_t i = 0; i < num_video_streams; ++i) {
    EXPECT_EQ(sdp_content[i].media_description()->type(),
              cricket::MediaType::MEDIA_TYPE_VIDEO);
    EXPECT_EQ(sdp_content[i].media_description()->direction(), kRecvOnly);
  }
}

INSTANTIATE_TEST_SUITE_P(NumberOfVideoStreamsValues, VideoMediaDescriptionTest,
                         Values(1, 2, 3));

TEST(MeetMediaApiClientTest, PublicApiCreateOnlyAudioEnabled) {
  MeetMediaApiClientConfiguration api_config = {.enable_audio_streams = true};

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MeetMediaApiClientInterface> client,
      MeetMediaApiClientInterface::Create(
          api_config, MockMeetMediaApiSessionObserver::Create(),
          MockMeetMediaSinkFactory::Create()));

  // Public API always signals data channels and configured number of
  // audio/video transceivers.
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  // 3 audio and data channel.
  ASSERT_THAT(sdp_content, SizeIs(4));
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[0].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[1].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[1].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[2].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_AUDIO);
  EXPECT_EQ(sdp_content[2].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[3].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest, PublicApiCreateOnlyVideoEnabled) {
  MeetMediaApiClientConfiguration api_config = {.receiving_video_stream_count =
                                                    1};

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MeetMediaApiClientInterface> client,
      MeetMediaApiClientInterface::Create(
          api_config, MockMeetMediaApiSessionObserver::Create(),
          MockMeetMediaSinkFactory::Create()));

  // Public API always signals data channels and configured number of
  // audio/video transceivers.
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  // 1 video and data channel.
  ASSERT_THAT(sdp_content, SizeIs(2));
  EXPECT_EQ(sdp_content[1].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_VIDEO);
  EXPECT_EQ(sdp_content[1].media_description()->direction(), kRecvOnly);
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest, PublicApiCreateOnlyMediaEntriesEnabled) {
  MeetMediaApiClientConfiguration api_config = {.enable_media_entries_resource =
                                                    true};

  ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<MeetMediaApiClientInterface> client,
      MeetMediaApiClientInterface::Create(
          api_config, MockMeetMediaApiSessionObserver::Create(),
          MockMeetMediaSinkFactory::Create()));

  // Public API always signals data channels and configured number of
  // audio/video transceivers.
  ASSERT_OK_AND_ASSIGN(std::string local_description,
                       client->GetLocalDescription());

  std::unique_ptr<webrtc::SessionDescriptionInterface> sdp =
      StringToSessionDescription(webrtc::SdpType::kOffer, local_description);
  EXPECT_EQ(sdp->GetType(), webrtc::SdpType::kOffer);

  const cricket::ContentInfos &sdp_content = sdp->description()->contents();
  // No matter how many data channels are configured, only a single data media
  // description exists.
  ASSERT_THAT(sdp_content, SizeIs(1));
  EXPECT_EQ(sdp_content[0].media_description()->type(),
            cricket::MediaType::MEDIA_TYPE_DATA);
}

TEST(MeetMediaApiClientTest,
     PublicApiCreateMediaApiClientFailsIfInvalidVideoStreamCount) {
  MeetMediaApiClientConfiguration api_config = {.receiving_video_stream_count =
                                                    4};

  EXPECT_THAT(
      MeetMediaApiClientInterface::Create(
          api_config, MockMeetMediaApiSessionObserver::Create(),
          MockMeetMediaSinkFactory::Create()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Invalid number of receiving video streams: 4")));
}

TEST(MeetMediaApiClientTest, VideoAssignmentDataChannelSendRequestSucceeds) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::MockDataChannelInterface>
      mock_video_assignment_channel =
          webrtc::MockDataChannelInterface::Create();

  EXPECT_CALL(*mock_video_assignment_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_video_assignment_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError) &&> on_complete) {
        absl::string_view message(buffer.data.cdata<char>(), buffer.size());
        EXPECT_THAT(message, StrEq(Json::parse(R"json({
                                              "request": {
                                                "requestId": 42
                                              }
                                            })json")
                                       .dump()));
      });

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(InvokeWithoutArgs([&]() {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_video_assignment_channel);
      }));
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  EXPECT_OK(client->SendRequest(
      {.hint = ResourceHint::kVideoAssignment,
       .video_assignment_request =
           VideoAssignmentChannelFromClient{.request = {.request_id = 42}}}));
}

TEST(MeetMediaApiClientTest, SessionControlDataChannelSendRequestSucceeds) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<webrtc::MockDataChannelInterface>
      mock_session_control_channel = webrtc::MockDataChannelInterface::Create();

  EXPECT_CALL(*mock_session_control_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_session_control_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError) &&> on_complete) {
        absl::string_view message(buffer.data.cdata<char>(), buffer.size());
        EXPECT_THAT(message, StrEq(Json::parse(R"json({
                                                "request": {
                                                  "requestId": 42,
                                                  "leave": {}
                                                }
                                              })json")
                                       .dump()));
      });

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(InvokeWithoutArgs([&]() {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_session_control_channel);
      }));
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = false,
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  EXPECT_OK(client->SendRequest(
      {.hint = ResourceHint::kSessionControl,
       .session_control_request = SessionControlChannelFromClient{
           .request = {.request_id = 42, .leave_request = LeaveRequest()}}}));
}

TEST(MeetMediaApiClientTest,
     SessionControlRequestNotSetInRequestMakesSendRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(InvokeWithoutArgs([&]() {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      }));
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = false,
      .enable_session_control_data_channel = true,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  EXPECT_THAT(client->SendRequest({.hint = ResourceHint::kSessionControl}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Session control request is not set.")));
}

TEST(MeetMediaApiClientTest,
     VideoAssignmentRequestNotSetInRequestMakesSendRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _))
      .WillOnce(InvokeWithoutArgs([&]() {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      }));
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = true,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));
  EXPECT_THAT(client->SendRequest({.hint = ResourceHint::kVideoAssignment}),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Video assignment request is not set.")));
}

TEST(MeetMediaApiClientTest,
     SessionControlDataChannelNotEnabledSendRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _)).Times(0);
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  ResourceRequest request = {
      .hint = ResourceHint::kSessionControl,
      .session_control_request = SessionControlChannelFromClient{
          .request = {.request_id = 42, .leave_request = LeaveRequest()}}};

  EXPECT_THAT(
      client->SendRequest(std::move(request)),
      StatusIs(absl::StatusCode::kFailedPrecondition,
               HasSubstr("Session control data channel is not enabled.")));
}

TEST(MeetMediaApiClientTest,
     VideoAssignmentDataChannelNotEnabledSendRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _)).Times(0);
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  ResourceRequest request = {
      .hint = ResourceHint::kVideoAssignment,
      .video_assignment_request =
          VideoAssignmentChannelFromClient{.request = {.request_id = 42}}};

  EXPECT_THAT(
      client->SendRequest(std::move(request)),
      StatusIs(absl::StatusCode::kFailedPrecondition,
               HasSubstr("Video assignment data channel is not enabled.")));
}

TEST(MeetMediaApiClientTest, UnsupportedResourceSendRequestFails) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  signaling_thread->Start();
  worker_thread->Start();

  rtc::scoped_refptr<MockPeerConnectionInterface> peer_connection =
      rtc::make_ref_counted<MockPeerConnectionInterface>();
  EXPECT_CALL(*peer_connection, CreateDataChannelOrError(_, _)).Times(0);
  ON_CALL(*peer_connection, SetLocalDescription(_))
      .WillByDefault(
          [](rtc::scoped_refptr<::webrtc::SetLocalDescriptionObserverInterface>
                 observer) {
            observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());
          });

  rtc::scoped_refptr<webrtc::MockPeerConnectionFactoryInterface>
      peer_connection_factory =
          webrtc::MockPeerConnectionFactoryInterface::Create();
  EXPECT_CALL(*peer_connection_factory, CreatePeerConnectionOrError)
      .WillOnce(ReturnPeerConnection(peer_connection));

  InternalConfigurations configurations = {
      .receiving_video_stream_count = 0,
      .enable_audio_streams = false,
      .enable_media_entries_resource = false,
      .enable_video_assignment_resource = false,
      .enable_session_control_data_channel = false,
      .enable_stats_data_channel = false,
      .signaling_thread = std::move(signaling_thread),
      .worker_thread = std::move(worker_thread),
      .api_session_observer = MockMeetMediaApiSessionObserver::Create(),
      .sink_factory = MockMeetMediaSinkFactory::Create(),
      .peer_connection_factory = std::move(peer_connection_factory),
      .curl_request_factory = MockCurlRequestFactory::CreateUnique()};

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<MeetMediaApiClientInterface> client,
                       CreateMeetMediaApiClient(std::move(configurations)));

  EXPECT_THAT(
      client->SendRequest({.hint = ResourceHint::kUnknownResource}),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Unrecognized or unsupported resource request")));
}

}  // namespace
}  // namespace meet
