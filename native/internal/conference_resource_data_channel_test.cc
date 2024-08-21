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

#include "native/internal/conference_resource_data_channel.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/base/public/mock-log.h"
#include "absl/base/log_severity.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "nlohmann/json.hpp"
#include "native/api/conference_resources.h"
#include "native/api/meet_media_api_client_interface.h"
#include "native/internal/media_entries_resource_handler.h"
#include "native/internal/resource_handler_interface.h"
#include "native/internal/session_control_resource_handler.h"
#include "native/internal/testing/mock_meet_media_api_session_observer.h"
#include "native/internal/video_assignment_resource_handler.h"
#include "webrtc/api/data_channel_interface.h"
#include "webrtc/api/make_ref_counted.h"
#include "webrtc/api/rtc_error.h"
#include "webrtc/api/scoped_refptr.h"
#include "webrtc/api/test/mock_data_channel.h"
#include "webrtc/api/test/mock_peerconnectioninterface.h"
#include "webrtc/rtc_base/copy_on_write_buffer.h"
#include "webrtc/rtc_base/thread.h"

namespace meet {
namespace {

using ::base_logging::ERROR;
using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::HasSubstr;
using ::testing::kDoNotCaptureLogsYet;
using ::testing::Return;
using ::testing::ScopedMockLog;
using ::testing::StrEq;
using ::testing::status::StatusIs;
using Json = ::nlohmann::json;

class MockResourceHandler
    : public ResourceHandlerInterface<std::string, std::string> {
 public:
  static std::unique_ptr<MockResourceHandler> Create() {
    return std::make_unique<MockResourceHandler>();
  }

  MOCK_METHOD(absl::StatusOr<std::string>, ParseUpdate, (absl::string_view),
              (override));
  MOCK_METHOD(absl::StatusOr<std::string>, Stringify, (const std::string&),
              (override));
};

TEST(ConferenceResourceDataChannelTest, CreateReturnsResourceDataChannel) {
  std::string channel_label = "testing";
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  EXPECT_CALL(*peer_connection,
              CreateDataChannelOrError(std::string(channel_label), _))
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        EXPECT_TRUE(config->ordered);
        EXPECT_TRUE(config->reliable);
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              channel_label, std::move(resource_handler), worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>
      data_channel = *std::move(create_status);

  EXPECT_EQ(data_channel->label(), channel_label);
}

TEST(ConferenceResourceDataChannelTest, OnMessageInvokesResourceHandler) {
  std::string resource_update = "test update";
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  absl::Notification handler_done;
  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  EXPECT_CALL(*resource_handler, ParseUpdate(resource_update))
      .WillOnce([&handler_done, resource_update](absl::string_view update) {
        EXPECT_EQ(update, resource_update);
        handler_done.Notify();
        return std::string(update);
      });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler), worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>
      data_channel = *std::move(create_status);

  data_channel->OnMessage(webrtc::DataBuffer(std::string(resource_update)));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest,
     OnMessageNotifiesApiObserverWithVideoAssignmentUpdate) {
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();
  rtc::scoped_refptr<MockMeetMediaApiSessionObserver> api_observer =
      MockMeetMediaApiSessionObserver::Create();

  absl::Notification handler_done;
  EXPECT_CALL(*api_observer, OnResourceUpdate)
      .WillOnce([&](ResourceUpdate update) {
        EXPECT_EQ(update.hint, ResourceHint::kVideoAssignment);
        EXPECT_TRUE(update.video_assignment_update.has_value());
        handler_done.Notify();
      });

  std::unique_ptr<VideoAssignmentResourceHandler> resource_handler =
      std::make_unique<VideoAssignmentResourceHandler>();
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>>
      create_status = ConferenceResourceDataChannel<
          VideoAssignmentChannelToClient,
          VideoAssignmentChannelFromClient>::Create(std::move(api_observer),
                                                    peer_connection,
                                                    "video-assignment",
                                                    std::move(resource_handler),
                                                    worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>
      data_channel = *std::move(create_status);

  data_channel->OnMessage(webrtc::DataBuffer(R"json({
        "resources": [
          {
            "id": 1234
          }
        ]
    })json"));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest,
     OnMessageNotifiesApiObserverWithSessionControlUpdate) {
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();
  rtc::scoped_refptr<MockMeetMediaApiSessionObserver> api_observer =
      MockMeetMediaApiSessionObserver::Create();

  absl::Notification handler_done;
  EXPECT_CALL(*api_observer, OnResourceUpdate)
      .WillOnce([&](ResourceUpdate update) {
        EXPECT_EQ(update.hint, ResourceHint::kSessionControl);
        EXPECT_TRUE(update.session_control_update.has_value());
        handler_done.Notify();
      });

  std::unique_ptr<SessionControlResourceHandler> resource_handler =
      std::make_unique<SessionControlResourceHandler>();
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>>
      create_status = ConferenceResourceDataChannel<
          SessionControlChannelToClient,
          SessionControlChannelFromClient>::Create(std::move(api_observer),
                                                   peer_connection,
                                                   "session-control",
                                                   std::move(resource_handler),
                                                   worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>
      data_channel = *std::move(create_status);

  data_channel->OnMessage(webrtc::DataBuffer(R"json({
        "resources": []
    })json"));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest,
     OnMessageNotifiesApiObserverWithMediaEntriesUpdate) {
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();
  rtc::scoped_refptr<MockMeetMediaApiSessionObserver> api_observer =
      MockMeetMediaApiSessionObserver::Create();

  absl::Notification handler_done;
  EXPECT_CALL(*api_observer, OnResourceUpdate)
      .WillOnce([&](ResourceUpdate update) {
        EXPECT_EQ(update.hint, ResourceHint::kMediaEntries);
        EXPECT_TRUE(update.media_entries_update.has_value());
        handler_done.Notify();
      });

  std::unique_ptr<MediaEntriesResourceHandler> resource_handler =
      std::make_unique<MediaEntriesResourceHandler>();
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      MediaEntriesChannelToClient, NoResourceRequestsFromClient>>>
      create_status = ConferenceResourceDataChannel<
          MediaEntriesChannelToClient,
          NoResourceRequestsFromClient>::Create(std::move(api_observer),
                                                peer_connection,
                                                "media-entries",
                                                std::move(resource_handler),
                                                worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<MediaEntriesChannelToClient,
                                                NoResourceRequestsFromClient>>
      data_channel = *std::move(create_status);

  data_channel->OnMessage(webrtc::DataBuffer(R"json({
        "resources": [
          {
            "id": 42
          }
        ]
    })json"));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest, UnexpectedBinaryMessageLogsError) {
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler), worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>
      data_channel = *std::move(create_status);

  std::string message = "test message";
  std::vector<uint8_t> messageBytes(message.begin(), message.end());
  rtc::CopyOnWriteBuffer buffer(messageBytes.data(), messageBytes.size());
  webrtc::DataBuffer binary_message(buffer, true);
  absl::Notification handler_done;
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce([&handler_done](int, const std::string&,
                                const std::string& msg) {
        EXPECT_THAT(
            msg, ContainsRegex("Received unexpected binary testing update."));
        handler_done.Notify();
      });
  log.StartCapturingLogs();

  data_channel->OnMessage(binary_message);

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest, HandlerParseErrorLogsError) {
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  EXPECT_CALL(*resource_handler, ParseUpdate)
      .WillOnce([](absl::string_view update) {
        return absl::InternalError("failed to parse: test error");
      });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler), worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>
      data_channel = *std::move(create_status);

  absl::Notification handler_done;
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce(
          [&handler_done](int, const std::string&, const std::string& msg) {
            EXPECT_THAT(msg, ContainsRegex("test error"));
            handler_done.Notify();
          });
  log.StartCapturingLogs();

  data_channel->OnMessage(webrtc::DataBuffer("unused message"));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest, UnknownResourceLabelLogsError) {
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([](const std::string& label,
                   const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            webrtc::MockDataChannelInterface::Create());
      });

  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  EXPECT_CALL(*resource_handler, ParseUpdate)
      .WillOnce([](absl::string_view update) { return "unused response"; });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler), worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>
      data_channel = *std::move(create_status);

  absl::Notification handler_done;
  ScopedMockLog log(kDoNotCaptureLogsYet);
  EXPECT_CALL(log, Log(ERROR, _, _))
      .WillOnce(
          [&handler_done](int, const std::string&, const std::string& msg) {
            EXPECT_THAT(msg, ContainsRegex("Unknown resource hint: testing"));
            handler_done.Notify();
          });
  log.StartCapturingLogs();

  data_channel->OnMessage(webrtc::DataBuffer("unused message"));

  EXPECT_TRUE(handler_done.WaitForNotificationWithTimeout(absl::Seconds(5)));
}

TEST(ConferenceResourceDataChannelTest, CreateReturnsError) {
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  std::unique_ptr<MockResourceHandler> resource_handler =
      MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce(
          [](const std::string& label, const webrtc::DataChannelInit* config) {
            return webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR);
          });

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler), worker_thread.get());

  EXPECT_THAT(create_status, StatusIs(absl::StatusCode::kInternal));
}

TEST(ConferenceResourceDataChannelTest,
     CreateNullPtrPeerConnectionReturnsError) {
  auto resource_handler = MockResourceHandler::Create();
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(),
              /* peer_connection= */ nullptr, "testing",
              std::move(resource_handler), worker_thread.get());

  EXPECT_THAT(create_status,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Provided peer connection is null")));
}

TEST(ConferenceResourceDataChannelTest, CreateNullPtrWorkerThreadReturnsError) {
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  auto resource_handler = MockResourceHandler::Create();

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              MockMeetMediaApiSessionObserver::Create(), peer_connection,
              "testing", std::move(resource_handler),
              /* worker_thread= */ nullptr);

  EXPECT_THAT(create_status,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Provided worker thread is null")));
}

TEST(ConferenceResourceDataChannelTest, CreateNullPtrApiObserverReturnsError) {
  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();
  auto resource_handler = MockResourceHandler::Create();

  absl::StatusOr<
      std::unique_ptr<ConferenceResourceDataChannel<std::string, std::string>>>
      create_status =
          ConferenceResourceDataChannel<std::string, std::string>::Create(
              /* api_session_observer= */ nullptr, peer_connection, "testing",
              std::move(resource_handler), worker_thread.get());

  EXPECT_THAT(create_status,
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Provided api session observer is null")));
}

TEST(ConferenceResourceDataChannelTest,
     VideoAssignmentSendRequestIsSuccessful) {
  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*mock_data_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_data_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError)&&> on_complete) {
        absl::string_view message(buffer.data.cdata<char>(), buffer.size());
        EXPECT_THAT(message, StrEq(Json::parse(R"json({
                                              "request": {
                                              "requestId": 42
                                              }
                                            })json")
                                       .dump()));
      });

  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([&mock_data_channel](const std::string& label,
                                     const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_data_channel);
      });

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>>
      create_status =
          ConferenceResourceDataChannel<VideoAssignmentChannelToClient,
                                        VideoAssignmentChannelFromClient>::
              Create(MockMeetMediaApiSessionObserver::Create(), peer_connection,
                     "video-assignment",
                     std::make_unique<VideoAssignmentResourceHandler>(),
                     worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>
      data_channel = *std::move(create_status);

  EXPECT_OK(data_channel->SendRequest(
      VideoAssignmentChannelFromClient{.request = {.request_id = 42}}));
}

TEST(ConferenceResourceDataChannelTest, SessionControlSendRequestIsSuccessful) {
  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*mock_data_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_data_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError)&&> on_complete) {
        absl::string_view message(buffer.data.cdata<char>(), buffer.size());
        EXPECT_THAT(message, StrEq(Json::parse(R"json({
                                                "request": {
                                                  "requestId": 42,
                                                  "leave": {}
                                                }
                                              })json")
                                       .dump()));
      });

  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([&mock_data_channel](const std::string& label,
                                     const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_data_channel);
      });

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>>
      create_status =
          ConferenceResourceDataChannel<SessionControlChannelToClient,
                                        SessionControlChannelFromClient>::
              Create(MockMeetMediaApiSessionObserver::Create(), peer_connection,
                     "session-control",
                     std::make_unique<SessionControlResourceHandler>(),
                     worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>
      data_channel = *std::move(create_status);

  EXPECT_OK(data_channel->SendRequest(SessionControlChannelFromClient{
      .request = {.request_id = 42, .leave_request = LeaveRequest()}}));
}

TEST(ConferenceResourceDataChannelTest,
     SendRequestWithClosedChannelReturnsError) {
  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();
  EXPECT_CALL(*mock_data_channel, state)
      .WillRepeatedly(Return(webrtc::DataChannelInterface::kClosed));
  EXPECT_CALL(*mock_data_channel, SendAsync).Times(0);

  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([&mock_data_channel](const std::string& label,
                                     const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_data_channel);
      });

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>>
      create_status =
          ConferenceResourceDataChannel<SessionControlChannelToClient,
                                        SessionControlChannelFromClient>::
              Create(MockMeetMediaApiSessionObserver::Create(), peer_connection,
                     "testing",
                     std::make_unique<SessionControlResourceHandler>(),
                     worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>
      data_channel = *std::move(create_status);

  EXPECT_THAT(
      data_channel->SendRequest(SessionControlChannelFromClient{}),
      StatusIs(absl::StatusCode::kFailedPrecondition, HasSubstr("not open")));
}

TEST(ConferenceResourceDataChannelTest,
     SessionControlSendRequestFailsToSendAndNotifiesApiSessionObserver) {
  rtc::scoped_refptr<MockMeetMediaApiSessionObserver> mock_observer =
      MockMeetMediaApiSessionObserver::Create();

  EXPECT_CALL(*mock_observer, OnResourceRequestFailure)
      .WillOnce(
          [](MeetMediaApiSessionObserverInterface::ResourceRequestError error) {
            EXPECT_EQ(error.hint, ResourceHint::kSessionControl);
            EXPECT_EQ(error.request_id, 42);
          });

  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();

  EXPECT_CALL(*mock_data_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_data_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError)&&> on_complete) {
        std::move(on_complete)(
            webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
      });

  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([&mock_data_channel](const std::string& label,
                                     const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_data_channel);
      });

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>>
      create_status =
          ConferenceResourceDataChannel<SessionControlChannelToClient,
                                        SessionControlChannelFromClient>::
              Create(std::move(mock_observer), peer_connection,
                     "session-control",
                     std::make_unique<SessionControlResourceHandler>(),
                     worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      SessionControlChannelToClient, SessionControlChannelFromClient>>
      data_channel = *std::move(create_status);

  EXPECT_OK(data_channel->SendRequest(SessionControlChannelFromClient{
      .request = {.request_id = 42, .leave_request = LeaveRequest()}}));
}

TEST(ConferenceResourceDataChannelTest,
     VideoAssignmentSendRequestFailsToSendAndNotifiesApiSessionObserver) {
  rtc::scoped_refptr<MockMeetMediaApiSessionObserver> mock_observer =
      MockMeetMediaApiSessionObserver::Create();

  EXPECT_CALL(*mock_observer, OnResourceRequestFailure)
      .WillOnce(
          [](MeetMediaApiSessionObserverInterface::ResourceRequestError error) {
            EXPECT_EQ(error.hint, ResourceHint::kVideoAssignment);
            EXPECT_EQ(error.request_id, 42);
          });

  rtc::scoped_refptr<webrtc::MockDataChannelInterface> mock_data_channel =
      webrtc::MockDataChannelInterface::Create();

  EXPECT_CALL(*mock_data_channel, state)
      .WillOnce(Return(webrtc::DataChannelInterface::kOpen));
  EXPECT_CALL(*mock_data_channel, SendAsync)
      .WillOnce([](webrtc::DataBuffer buffer,
                   absl::AnyInvocable<void(webrtc::RTCError)&&> on_complete) {
        std::move(on_complete)(
            webrtc::RTCError(webrtc::RTCErrorType::INTERNAL_ERROR));
      });

  auto peer_connection =
      rtc::make_ref_counted<webrtc::MockPeerConnectionInterface>();

  EXPECT_CALL(*peer_connection, CreateDataChannelOrError)
      .WillOnce([&mock_data_channel](const std::string& label,
                                     const webrtc::DataChannelInit* config) {
        return static_cast<rtc::scoped_refptr<webrtc::DataChannelInterface>>(
            mock_data_channel);
      });

  std::unique_ptr<rtc::Thread> worker_thread = rtc::Thread::Create();
  worker_thread->Start();

  absl::StatusOr<std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>>
      create_status =
          ConferenceResourceDataChannel<VideoAssignmentChannelToClient,
                                        VideoAssignmentChannelFromClient>::
              Create(std::move(mock_observer), peer_connection,
                     "video-assignment",
                     std::make_unique<VideoAssignmentResourceHandler>(),
                     worker_thread.get());
  ASSERT_OK(create_status);

  std::unique_ptr<ConferenceResourceDataChannel<
      VideoAssignmentChannelToClient, VideoAssignmentChannelFromClient>>
      data_channel = *std::move(create_status);

  EXPECT_OK(data_channel->SendRequest(
      VideoAssignmentChannelFromClient{.request = {.request_id = 42}}));
}

}  // namespace
}  // namespace meet
