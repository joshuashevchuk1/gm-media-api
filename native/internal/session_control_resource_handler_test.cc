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

#include "native/internal/session_control_resource_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "nlohmann/json.hpp"

namespace meet {
namespace {
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::StrEq;
using Json = ::nlohmann::json;

TEST(SessionControlResourceHandlerTest, ParsesMultipleResourceSnapshots) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 42,
            "sessionStatus": {
              "connectionState": "STATE_JOINED"
            }
          },
          {
            "id": 24,
            "sessionStatus": {
              "connectionState": "STATE_DISCONNECTED"
            }
          },
          {
            "id": 99,
            "sessionStatus": {
              "connectionState": "STATE_WAITING"
            }
          },
          {
            "id": 199,
            "sessionStatus": {
              "connectionState": "STATE_UNKNOWN"
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  SessionControlChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(4));
  EXPECT_EQ(parsed_update.resources[0].id, 42);
  EXPECT_EQ(parsed_update.resources[0].session_status.connection_state,
            SessionStatus::MeetingConnectionState::kJoined);
  EXPECT_EQ(parsed_update.resources[1].id, 24);
  EXPECT_EQ(parsed_update.resources[1].session_status.connection_state,
            SessionStatus::MeetingConnectionState::kDisconnected);
  EXPECT_EQ(parsed_update.resources[2].id, 99);
  EXPECT_EQ(parsed_update.resources[2].session_status.connection_state,
            SessionStatus::MeetingConnectionState::kWaiting);
  EXPECT_EQ(parsed_update.resources[3].id, 199);
  EXPECT_EQ(parsed_update.resources[3].session_status.connection_state,
            SessionStatus::MeetingConnectionState::kUnknown);
}

TEST(SessionControlResourceHandlerTest, ResourcesUpdateEmptyArrayParsesJson) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": []
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  SessionControlChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  EXPECT_THAT(parsed_update.resources, SizeIs(0));
}

TEST(SessionControlResourceHandlerTest, ParsesResponseField) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "response": {
          "requestId": 123,
          "status": {
            "code": 13,
            "message": "The answer to life is 42"},
          "leave": {}
        }
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  SessionControlChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_TRUE(parsed_update.response.has_value());
  EXPECT_EQ(parsed_update.response->request_id, 123);
  EXPECT_EQ(parsed_update.response->status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(parsed_update.response->status.message(),
            "The answer to life is 42");
}

TEST(SessionControlResourceHandlerTest, MalformedJsonReturnsErrorStatus) {
  SessionControlResourceHandler handler;
  absl::StatusOr<SessionControlChannelToClient> parsed_update =
      handler.ParseUpdate(" random garbage that is not json!");

  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(
      parsed_update.status().message(),
      "Invalid session-control json format:  random garbage that is not json!");
}

TEST(SessionControlResourceHandlerTest, UnexpectedResourcesReturnsErrorStatus) {
  SessionControlResourceHandler handler;
  absl::StatusOr<SessionControlChannelToClient> parsed_update =
      handler.ParseUpdate(R"json({
        "resources": {
          "id": 42,
          "sessionStatus": {
            "connectionState": "STATE_JOINED"
          }
        }
    })json");

  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_TRUE(absl::StrContains(parsed_update.status().message(),
                                "Invalid session-control json format. Expected "
                                "resources field to be an array:"));
}

TEST(SessionControlResourceHandlerTest,
     NoConnectionStateIsUnknownConnectionState) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 42,
            "sessionStatus": {}
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  SessionControlChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].session_status.connection_state,
            SessionStatus::MeetingConnectionState::kUnknown);
}

TEST(SessionControlResourceHandlerTest,
     NoResourceSnapshotIdPresentIsSingletonResource) {
  SessionControlResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "sessionStatus": {
              "connectionState": "STATE_JOINED"
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  SessionControlChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  // Singleton resources have id value of 0
  EXPECT_EQ(parsed_update.resources[0].id, 0);
}

TEST(SessionControlResourceHandlerTest, ParsesClientRequestId) {
  SessionControlChannelFromClient client_request;
  client_request.request.request_id = 42;
  SessionControlResourceHandler handler;

  auto status_or_json_request = handler.Stringify(client_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
    "request": {
      "requestId": 42
    }
  })json")
                                      .dump()));
}

TEST(SessionControlResourceHandlerTest, ParseLeaveRequest) {
  SessionControlChannelFromClient client_request;
  client_request.request.request_id = 42;
  client_request.request.leave_request = LeaveRequest();
  SessionControlResourceHandler handler;

  auto status_or_json_request = handler.Stringify(client_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
    "request": {
      "requestId": 42,
      "leave": {}
    }
  })json")
                                      .dump()));
}

TEST(SessionControlResourceHandlerTest, NoClientRequestIdReturnsErrorStatus) {
  SessionControlChannelFromClient client_request;
  client_request.request.request_id = 0;

  SessionControlResourceHandler handler;

  auto status_or_string = handler.Stringify(client_request);
  EXPECT_EQ(status_or_string.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status_or_string.status().message(),
              HasSubstr("Request ID must be set"));
}

}  // namespace
}  // namespace meet
