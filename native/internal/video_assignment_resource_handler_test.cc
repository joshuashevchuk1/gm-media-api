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

#include "native/internal/video_assignment_resource_handler.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "native/api/conference_resources.h"

namespace meet {
namespace {
using ::testing::HasSubstr;
using ::testing::SizeIs;
using ::testing::StrEq;
using enum VideoCanvas::AssignmentProtocol;
using Json = ::nlohmann::json;

TEST(VideoAssignmentResourceHandlerTest, ParsesResourceSnapshots) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 1234,
            "videoAssignment": {
              "label": "the answer to life is 42",
              "canvases": [
                {
                  "canvasId": 42,
                  "ssrc": 424242,
                  "mediaEntryId": 242424
                }
              ]
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 1234);
  EXPECT_EQ(parsed_update.resources[0].assignment->label,
            "the answer to life is 42");
  ASSERT_THAT(parsed_update.resources[0].assignment->canvases, SizeIs(1));

  VideoCanvasAssignment canvas =
      parsed_update.resources[0].assignment->canvases[0];
  EXPECT_EQ(canvas.canvas_id, 42);
  EXPECT_EQ(canvas.ssrc, 424242);
  EXPECT_EQ(canvas.media_entry_id, 242424);
}

TEST(VideoAssignmentResourceHandlerTest,
     ParsesResourceSnapshotsMultipleCanvases) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 1234,
            "videoAssignment": {
              "label": "the answer to life is 42",
              "canvases": [
                {
                  "canvasId": 42,
                  "ssrc": 424242,
                  "mediaEntryId": 242424
                },
                {
                  "canvasId": 55,
                  "ssrc": 99999,
                  "mediaEntryId": 11111
                }
              ]
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  ASSERT_THAT(parsed_update.resources[0].assignment->canvases, SizeIs(2));

  VideoCanvasAssignment canvas1 =
      parsed_update.resources[0].assignment->canvases[0];
  EXPECT_EQ(canvas1.canvas_id, 42);
  EXPECT_EQ(canvas1.ssrc, 424242);
  EXPECT_EQ(canvas1.media_entry_id, 242424);

  VideoCanvasAssignment canvas2 =
      parsed_update.resources[0].assignment->canvases[1];
  EXPECT_EQ(canvas2.canvas_id, 55);
  EXPECT_EQ(canvas2.ssrc, 99999);
  EXPECT_EQ(canvas2.media_entry_id, 11111);
}

TEST(VideoAssignmentResourceHandlerTest, ParsesMultipleResourceSnapshots) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 1234,
            "videoAssignment": {
              "label": "the answer to life is 42",
              "canvases": [
                {
                  "canvasId": 42,
                  "ssrc": 424242,
                  "mediaEntryId": 242424
                }
              ]
            }
          },
          {
            "id": 4321,
            "videoAssignment": {
              "label": "schwifty",
              "canvases": [
                {
                  "canvasId": 199,
                  "ssrc": 8,
                  "mediaEntryId": 2
                }
              ]
            }
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(2));
  EXPECT_EQ(parsed_update.resources[0].id, 1234);
  EXPECT_EQ(parsed_update.resources[0].assignment->label,
            "the answer to life is 42");
  ASSERT_THAT(parsed_update.resources[0].assignment->canvases, SizeIs(1));

  VideoCanvasAssignment canvas1 =
      parsed_update.resources[0].assignment->canvases[0];
  EXPECT_EQ(canvas1.canvas_id, 42);
  EXPECT_EQ(canvas1.ssrc, 424242);
  EXPECT_EQ(canvas1.media_entry_id, 242424);

  EXPECT_EQ(parsed_update.resources[1].id, 4321);
  EXPECT_EQ(parsed_update.resources[1].assignment->label, "schwifty");
  ASSERT_THAT(parsed_update.resources[1].assignment->canvases, SizeIs(1));

  VideoCanvasAssignment canvas2 =
      parsed_update.resources[1].assignment->canvases[0];
  EXPECT_EQ(canvas2.canvas_id, 199);
  EXPECT_EQ(canvas2.ssrc, 8);
  EXPECT_EQ(canvas2.media_entry_id, 2);
}

TEST(VideoAssignmentResourceHandlerTest,
     ParsesResourceSnapshotsNoVideoAssignmentOk) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 1234
          }
        ]
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_FALSE(parsed_update.resources[0].assignment.has_value());
}

TEST(VideoAssignmentResourceHandlerTest, ParsesResponseField) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "response": {
        "requestId": 1,
        "status": {
          "code": 13,
          "message": "Broken"
        },
        "setAssignment": {}
      }
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_TRUE(parsed_update.response.has_value());
  EXPECT_EQ(parsed_update.response->request_id, 1);
  EXPECT_EQ(parsed_update.response->status.code(), absl::StatusCode::kInternal);
  EXPECT_EQ(parsed_update.response->status.message(), "Broken");
  EXPECT_TRUE(parsed_update.response->set_assignment.has_value());
}

TEST(VideoAssignmentResourceHandlerTest,
     ParsesResponseFieldNoVideoAssignmentOk) {
  VideoAssignmentResourceHandler handler;

  auto status_or_parsed_update = handler.ParseUpdate(R"json({
        "response": {
        "requestId": 1,
        "status": {
          "code": 13,
          "message": "Broken"
        }
      }
    })json");
  ASSERT_TRUE(status_or_parsed_update.ok());
  VideoAssignmentChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_TRUE(parsed_update.response.has_value());
  EXPECT_FALSE(parsed_update.response->set_assignment.has_value());
}

TEST(VideoAssignmentResourceHandlerTest, MalformedJsonReturnsErrorStatus) {
  VideoAssignmentResourceHandler handler;
  absl::StatusOr<VideoAssignmentChannelToClient> parsed_update =
      handler.ParseUpdate(" random garbage that is not json!");

  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_EQ(parsed_update.status().message(),
            "Invalid video-assignment json format:  random garbage that is not "
            "json!");
}

TEST(VideoAssignmentResourceHandlerTest,
     UnexpectedResourcesReturnsErrorStatus) {
  VideoAssignmentResourceHandler handler;
  absl::StatusOr<VideoAssignmentChannelToClient> parsed_update =
      handler.ParseUpdate(R"json({
        "resources":
          {
            "id": 1234,
            "videoAssignment": {
              "label": "the answer to life is 42",
              "canvases": [
                {
                  "canvasId": 42,
                  "ssrc": 424242,
                  "mediaEntryId": 242424
                }
              ]
            }
          }
    })json");

  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid video-assignment json format. Expected "
                        "resources field to be an array:"));
}

TEST(VideoAssignmentResourceHandlerTest, UnexpectedCanvasesReturnsErrorStatus) {
  VideoAssignmentResourceHandler handler;
  absl::StatusOr<VideoAssignmentChannelToClient> parsed_update =
      handler.ParseUpdate(R"json({
        "resources": [
          {
            "id": 1234,
            "videoAssignment": {
              "label": "the answer to life is 42",
              "canvases":
                {
                  "canvasId": 42,
                  "ssrc": 424242,
                  "mediaEntryId": 242424
                }
            }
          }
        ]
    })json");

  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid video-assignment json format. Expected "
                        "canvases field to be an array:"));
}

TEST(VideoAssignmentResourceHandlerTest, ParsesClientRequestId) {
  VideoAssignmentChannelFromClient client_request;
  client_request.request.request_id = 1;
  VideoAssignmentResourceHandler handler;

  auto status_or_json_request = handler.Stringify(client_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
        "request": {
          "requestId": 1
        }
      })json")
                                      .dump()));
}

TEST(VideoAssignmentResourceHandlerTest, NoClientRequestIdReturnsErrorStatus) {
  VideoAssignmentChannelFromClient client_request;
  client_request.request.request_id = 0;

  VideoAssignmentResourceHandler handler;

  auto status = handler.Stringify(client_request).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("Request ID must be set"));
}

TEST(VideoAssignmentResourceHandlerTest,
     ParsesClientLayoutModelLabelAndDefaultMaxVideoResolution) {
  VideoAssignmentChannelFromClient client_request;
  client_request.request.request_id = 1;
  client_request.request.set_video_assignment_request = {
      .layout_model = {.label = "the answer to life is 42"}};

  VideoAssignmentResourceHandler handler;

  auto status_or_json_request = handler.Stringify(client_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
        "request": {
          "requestId": 1,
          "setAssignment": {
            "layoutModel": {
              "label": "the answer to life is 42"
            },
            "maxVideoResolution": {
              "frameRate": 30,
              "height": 480,
              "width": 640
            }
          }
        }
      })json")
                                      .dump()));
}

TEST(VideoAssignmentResourceHandlerTest, ParsesClientLayoutModelWithCanvases) {
  VideoCanvas canvas1 = {.id = 1,
                         .dimensions = {.height = 100, .width = 100},
                         .assignment_protocol = kDirect};
  VideoCanvas canvas2 = {.id = 2,
                         .dimensions = {.height = 200, .width = 200},
                         .assignment_protocol = kRelevant};
  VideoAssignmentChannelFromClient client_request;
  client_request.request.request_id = 1;
  client_request.request.set_video_assignment_request = {
      .layout_model = {.label = "the answer to life is 42",
                       .canvases = {canvas1, canvas2}},
  };

  VideoAssignmentResourceHandler handler;

  auto status_or_json_request = handler.Stringify(client_request);
  ASSERT_TRUE(status_or_json_request.ok());
  std::string json_request = status_or_json_request.value();
  EXPECT_THAT(json_request, StrEq(Json::parse(R"json({
        "request": {
          "requestId": 1,
          "setAssignment": {
            "layoutModel": {
              "canvases": [
                {
                  "id": 1,
                  "dimensions": {
                    "height": 100,
                    "width": 100
                  },
                  "direct": {}
                },
                {
                  "id": 2,
                  "dimensions": {
                    "height": 200,
                    "width": 200
                  },
                  "relevant": {}
                }
              ],
              "label": "the answer to life is 42"
            },
            "maxVideoResolution": {
              "frameRate": 30,
              "height": 480,
              "width": 640
            }
          }
        }
      })json")
                                      .dump()));
}

TEST(VideoAssignmentResourceHandlerTest, MissingCanvasIdReturnsErrorStatus) {
  VideoAssignmentChannelFromClient client_request;
  client_request.request.request_id = 1;
  client_request.request.set_video_assignment_request = {
      .layout_model = {.label = "the answer to life is 42",
                       .canvases = {{.dimensions = {.height = 100,
                                                    .width = 100},
                                     .assignment_protocol = kDirect}}},
  };

  VideoAssignmentResourceHandler handler;

  auto status = handler.Stringify(client_request).status();
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(status.message(), HasSubstr("Canvas ID must be set"));
}

}  // namespace
}  // namespace meet
