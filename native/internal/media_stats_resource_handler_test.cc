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

#include "native/internal/media_stats_resource_handler.h"

#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"
#include "native/api/media_stats_resource.h"

namespace meet {
namespace {

using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using Json = ::nlohmann::json;

TEST(MediaStatsResourceHandlerTest, ParsesPopulatedResource) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [{
        "id": 5,
        "configuration":{
          "uploadIntervalSeconds":10,
          "allowlist":{
            "remote-candidate":{
              "keys":[
                "id",
                "address"
              ]},
            "codec":{
              "keys":[
                "id",
                "mimeType",
                "payloadType"
              ]}
            }}}]})json");

  ASSERT_TRUE(parsed_update.ok());
  EXPECT_EQ(parsed_update->resources->size(), 1);
  EXPECT_EQ(parsed_update->resources->at(0).id, 5);
  EXPECT_EQ(
      parsed_update->resources->at(0).configuration.upload_interval_seconds,
      10);
  EXPECT_THAT(
      parsed_update->resources->at(0).configuration.allowlist,
      UnorderedElementsAre(
          Pair("remote-candidate", UnorderedElementsAre("id", "address")),
          Pair("codec",
               UnorderedElementsAre("id", "mimeType", "payloadType"))));
}

TEST(MediaStatsResourceHandlerTest, ParsesEmptyAllowlist) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [{
        "id": 5,
        "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{}
          }}]})json");

  ASSERT_TRUE(parsed_update.ok());
  EXPECT_EQ(parsed_update->resources->size(), 1);
  EXPECT_THAT(parsed_update->resources->at(0).configuration.allowlist,
              IsEmpty());
}

TEST(MediaStatsResourceHandlerTest, ZeroResourcesReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": []
      })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid media-stats json format. Expected resources "
                        "field to be an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, MultipleResourcesReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5,
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{}
          }
        },
        {
          "id": 6,
          "configuration":{
            "uploadIntervalSeconds":20,
            "allowlist":{}
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected resources field to be "
          "an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, NonArrayResourcesReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": "non-array-value"
      })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected resources field to be "
          "an array with exactly one element"));
}

TEST(MediaStatsResourceHandlerTest, MissingResourceIdReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{}
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty id field"));
}

TEST(MediaStatsResourceHandlerTest, MissingConfigurationReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty configuration "
          "field"));
}

TEST(MediaStatsResourceHandlerTest,
     MissingUploadIntervalSecondsReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5,
          "configuration":{
            "allowlist":{}
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(parsed_update.status().message(),
              HasSubstr("Invalid media-stats json format. Expected non-empty "
                        "uploadIntervalSeconds field"));
}

TEST(MediaStatsResourceHandlerTest, MissingAllowlistReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5,
          "configuration":{
            "uploadIntervalSeconds":10
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr("Invalid media-stats json format. Expected non-empty allowlist "
                "field"));
}

TEST(MediaStatsResourceHandlerTest, MissingKeysReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5,
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{
              "remote-candidate":{}
            }
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty keys array"));
}

TEST(MediaStatsResourceHandlerTest, NonArrayKeysReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
      "resources": [
        {
          "id": 5,
          "configuration":{
            "uploadIntervalSeconds":10,
            "allowlist":{
              "remote-candidate":{
                "keys": "non-array-value"
              }
            }
          }
        }
      ]})json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty keys array"));
}

TEST(MediaStatsResourceHandlerTest, ParsesResponse) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13,
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_TRUE(parsed_update.ok());
  EXPECT_EQ(parsed_update->response->request_id, 1);
  EXPECT_EQ(parsed_update->response->status.code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(parsed_update->response->status.message(), "Test Error");
  EXPECT_TRUE(parsed_update->response->upload_media_stats.has_value());
}

TEST(MediaStatsResourceHandlerTest, ParsesResponseWithoutUploadMediaStats) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13,
        "message": "Test Error"
      }
    }
  })json");
  ASSERT_TRUE(parsed_update.ok());
  EXPECT_EQ(parsed_update->response->request_id, 1);
  EXPECT_EQ(parsed_update->response->status.code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(parsed_update->response->status.message(), "Test Error");
  EXPECT_FALSE(parsed_update->response->upload_media_stats.has_value());
}

TEST(MediaStatsResourceHandlerTest, MissingRequestIdReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "status": {
        "code": 13,
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr("Invalid media-stats json format. Expected non-empty requestId "
                "field"));
}

TEST(MediaStatsResourceHandlerTest, MissingStatusReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty status field"));
}

TEST(MediaStatsResourceHandlerTest, MissingStatusCodeReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "message": "Test Error"
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty code field"));
}

TEST(MediaStatsResourceHandlerTest, MissingStatusMessageReturnsErrorStatus) {
  absl::StatusOr<MediaStatsChannelToClient> parsed_update =
      MediaStatsResourceHandler().ParseUpdate(R"json({
    "response": {
      "requestId": 1,
      "status": {
        "code": 13
      },
      "uploadMediaStats": {}
    }
  })json");
  ASSERT_FALSE(parsed_update.ok());
  EXPECT_EQ(parsed_update.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(
      parsed_update.status().message(),
      HasSubstr(
          "Invalid media-stats json format. Expected non-empty message field"));
}

TEST(MediaStatsResourceHandlerTest, ParsesClientRequest) {
  absl::StatusOr<std::string> json_request =
      MediaStatsResourceHandler().Stringify(MediaStatsChannelFromClient{
          .request = MediaStatsRequest{
              .request_id = 7,
              .upload_media_stats = UploadMediaStatsRequest{
                  .sections = {
                      MediaStatsSection{.type = "codec",
                                        .id = "1",
                                        .values = {{"mimeType", "video/VP8"},
                                                   {"payloadType", "111"}}},
                      MediaStatsSection{
                          .type = "remote-candidate",
                          .id = "2",
                          .values = {{"id", "2"},
                                     {"address", "127.0.0.1"}}}}}}});
  ASSERT_TRUE(json_request.ok());
  EXPECT_THAT(*std::move(json_request), StrEq(Json::parse(R"json({
        "request": {
          "requestId": 7,
          "uploadMediaStats": {
            "sections": [
              {
                "id": "1",
                "codec": {
                  "mimeType": "video/VP8",
                  "payloadType": "111"
                }
              },
              {
                "id": "2",
                "remote-candidate": {
                  "id": "2",
                  "address": "127.0.0.1"
                }
              }
            ]
          }
        }
      })json")
                                                  .dump()));
}

TEST(MediaStatsResourceHandlerTest, NoClientRequestIdReturnsErrorStatus) {
  absl::StatusOr<std::string> json_request =
      MediaStatsResourceHandler().Stringify(MediaStatsChannelFromClient());
  ASSERT_FALSE(json_request.ok());
  EXPECT_EQ(json_request.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(json_request.status().message(),
              HasSubstr("Request ID must be set"));
}

}  // namespace
}  // namespace meet
