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

#include <string>
#include <tuple>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "native/internal/testing/mock_meet_media_api_session_observer.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/peer_connection_interface.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/rtp_transceiver_interface.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/test/mock_data_channel.h"
#include "webrtc/api/test/mock_media_stream_interface.h"
#include "webrtc/api/test/mock_rtp_transceiver.h"

namespace meet {
namespace {

using ::base_logging::INFO;
using ::base_logging::WARNING;
using ::testing::_;
using ::testing::Exactly;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::Values;
using ::testing::status::StatusIs;
using enum webrtc::PeerConnectionInterface::IceGatheringState;
using enum webrtc::PeerConnectionInterface::SignalingState;

class OnSignalingChangeTest
    : public ::testing::TestWithParam<std::tuple<
          webrtc::PeerConnectionInterface::SignalingState, std::string>> {};

TEST_P(OnSignalingChangeTest, LogsStateValueCorrectly) {
  const auto& [state, state_string] = GetParam();

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log,
              Log(INFO, _, absl::StrCat("OnSignalingChange: ", state_string)))
      .Times(Exactly(1));
  log.StartCapturingLogs();

  MockFunction<MeetPeerConnectionObserver::OnRemoteTrackAddedCallback> callback;
  MeetPeerConnectionObserver observer(MockMeetMediaApiSessionObserver::Create(),
                                      callback.AsStdFunction());
  observer.OnSignalingChange(state);
}

INSTANTIATE_TEST_SUITE_P(
    StateStringValues, OnSignalingChangeTest,
    Values(std::make_tuple(kStable, "stable"),
           std::make_tuple(kHaveLocalOffer, "have-local-offer"),
           std::make_tuple(kHaveLocalPrAnswer, "have-local-pranswer"),
           std::make_tuple(kHaveRemoteOffer, "have-remote-offer"),
           std::make_tuple(kHaveRemotePrAnswer, "have-remote-pranswer"),
           std::make_tuple(kClosed, "closed")));

class OnIceGatheringChangeTest
    : public ::testing::TestWithParam<std::tuple<
          webrtc::PeerConnectionInterface::IceGatheringState, std::string>> {};

TEST_P(OnIceGatheringChangeTest, LogsStateValueCorrectly) {
  auto [state, state_string] = GetParam();

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(
      log, Log(INFO, _, absl::StrCat("OnIceGatheringChange: ", state_string)))
      .Times(Exactly(1));
  log.StartCapturingLogs();

  MockFunction<MeetPeerConnectionObserver::OnRemoteTrackAddedCallback> callback;
  MeetPeerConnectionObserver observer(MockMeetMediaApiSessionObserver::Create(),
                                      callback.AsStdFunction());
  observer.OnIceGatheringChange(state);
}

INSTANTIATE_TEST_SUITE_P(
    StateStringValues, OnIceGatheringChangeTest,
    Values(std::make_tuple(kIceGatheringComplete, "complete"),
           std::make_tuple(kIceGatheringNew, "new"),
           std::make_tuple(kIceGatheringGathering, "gathering")));

TEST(MeetPeerConnectionObserverTest, OnTrackInvokesCallback) {
  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver =
      webrtc::MockRtpTransceiver::Create();
  MockFunction<MeetPeerConnectionObserver::OnRemoteTrackAddedCallback> callback;

  EXPECT_CALL(callback, Call(transceiver)).Times(1);

  MeetPeerConnectionObserver observer(MockMeetMediaApiSessionObserver::Create(),
                                      callback.AsStdFunction());
  observer.OnTrack(transceiver);
}

TEST(MeetPeerConnectionObserverTest, OnDataChannelClosesDataChannel) {
  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();

  EXPECT_CALL(*mock_data_channel, label).WillOnce(Return("test_label"));
  EXPECT_CALL(*mock_data_channel, Close).Times(1);

  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log,
              Log(WARNING, _,
                  "OnDataChannel unexpectedly triggered. Closing and ignoring "
                  "data channel with label: test_label"))
      .Times(Exactly(1));
  log.StartCapturingLogs();

  MockFunction<MeetPeerConnectionObserver::OnRemoteTrackAddedCallback> callback;
  MeetPeerConnectionObserver observer(MockMeetMediaApiSessionObserver::Create(),
                                      callback.AsStdFunction());
  observer.OnDataChannel(mock_data_channel);
}

TEST(SetLocalDescriptionObserverTest, OnSetLocalDescriptionCompleteOk) {
  auto observer = webrtc::make_ref_counted<SetLocalDescriptionObserver>();
  observer->OnSetLocalDescriptionComplete(webrtc::RTCError::OK());

  EXPECT_OK(observer->WaitWithTimeout(absl::Seconds(1)));
}

TEST(SetLocalDescriptionObserverTest, OnSetLocalDescriptionCompleteError) {
  auto observer = webrtc::make_ref_counted<SetLocalDescriptionObserver>();
  observer->OnSetLocalDescriptionComplete(
      webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR, "test_error"));

  absl::Status result = observer->WaitWithTimeout(absl::Seconds(1));

  EXPECT_THAT(
      result,
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Failed to set local description: test_error")));
}

TEST(SetLocalDescriptionObserverTest, WaitWithTimeoutTimesOut) {
  auto observer = webrtc::make_ref_counted<SetLocalDescriptionObserver>();

  absl::Status result = observer->WaitWithTimeout(absl::Milliseconds(10));

  EXPECT_THAT(
      result,
      StatusIs(
          absl::StatusCode::kDeadlineExceeded,
          HasSubstr("Timed out waiting for local description to be set.")));
}

TEST(SetRemoteDescriptionObserverTest, OnSetRemoteDescriptionCompleteOk) {
  auto observer = webrtc::make_ref_counted<SetRemoteDescriptionObserver>();
  observer->OnSetRemoteDescriptionComplete(webrtc::RTCError::OK());

  EXPECT_OK(observer->WaitWithTimeout(absl::Seconds(1)));
}

TEST(SetRemoteDescriptionObserverTest, OnSetRemoteDescriptionCompleteError) {
  auto observer = webrtc::make_ref_counted<SetRemoteDescriptionObserver>();
  observer->OnSetRemoteDescriptionComplete(
      webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR, "test_error"));

  absl::Status result = observer->WaitWithTimeout(absl::Seconds(1));

  EXPECT_THAT(
      result,
      StatusIs(absl::StatusCode::kInternal,
               HasSubstr("Failed to set remote description: test_error")));
}

TEST(SetRemoteDescriptionObserverTest, WaitWithTimeoutTimesOut) {
  auto observer = webrtc::make_ref_counted<SetRemoteDescriptionObserver>();

  absl::Status result = observer->WaitWithTimeout(absl::Milliseconds(10));

  EXPECT_THAT(
      result,
      StatusIs(
          absl::StatusCode::kDeadlineExceeded,
          HasSubstr("Timed out waiting for remote description to be set.")));
}

}  // namespace
}  // namespace meet
