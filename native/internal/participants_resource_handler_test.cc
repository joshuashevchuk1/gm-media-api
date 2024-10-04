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

#include "native/internal/participants_resource_handler.h"

#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "native/api/participants_resource.h"

namespace meet {
namespace {
using ::testing::SizeIs;

TEST(ParticipantsResourceHandlerTest, ParsesSignedInUserFromSnapshot) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{
      "id": 3,
        "participant": {
          "participantId": 7,
          "name": "some-participant-name",
          "signedInUser": {
            "user": "some-obfuscated-gaia-id",
            "displayName": "some-display-name"
          }
        }
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 3);
  ASSERT_TRUE(parsed_update.resources[0].participant.has_value());
  Participant participant = parsed_update.resources[0].participant.value();
  EXPECT_EQ(participant.participant_id, 7);
  EXPECT_EQ(participant.name, "some-participant-name");
  ASSERT_TRUE(participant.signed_in_user.has_value());
  EXPECT_EQ(participant.signed_in_user.value().user, "some-obfuscated-gaia-id");
  EXPECT_EQ(participant.signed_in_user.value().display_name,
            "some-display-name");
}

TEST(ParticipantsResourceHandlerTest, ParsesAnonymousUserFromDeletedSnapshot) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{
      "id": 3,
        "participant": {
          "participantId": 7,
          "name": "some-participant-name",
          "anonymousUser": {
            "displayName": "some-display-name"
          }
        }
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 3);
  ASSERT_TRUE(parsed_update.resources[0].participant.has_value());
  Participant participant = parsed_update.resources[0].participant.value();
  EXPECT_EQ(participant.participant_id, 7);
  EXPECT_EQ(participant.name, "some-participant-name");
  ASSERT_TRUE(participant.anonymous_user.has_value());
  EXPECT_EQ(participant.anonymous_user.value().display_name,
            "some-display-name");
}

TEST(ParticipantsResourceHandlerTest, ParsesPhoneUserFromDeletedSnapshot) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{
      "id": 3,
        "participant": {
          "participantId": 7,
          "name": "some-participant-name",
          "phoneUser": {
            "displayName": "some-display-name"
          }
        }
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();

  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  EXPECT_EQ(parsed_update.resources[0].id, 3);
  ASSERT_TRUE(parsed_update.resources[0].participant.has_value());
  Participant participant = parsed_update.resources[0].participant.value();
  EXPECT_EQ(participant.participant_id, 7);
  EXPECT_EQ(participant.name, "some-participant-name");
  ASSERT_TRUE(participant.phone_user.has_value());
  EXPECT_EQ(participant.phone_user.value().display_name, "some-display-name");
}

TEST(ParticipantsResourceHandlerTest, ParsesMultipleResourceSnapshots) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{
      "id": 3,
        "participant": {
          "participantId": 7,
          "name": "some-participant-name",
          "phoneUser": {
            "displayName": "some-display-name"
          }
        }
    },{
      "id": 5,
        "participant": {
          "participantId": 9,
          "name": "some-participant-name",
          "phoneUser": {
            "displayName": "some-display-name"
          }
        }
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.resources, SizeIs(2));
  ASSERT_THAT(parsed_update.resources[0].participant.value().participant_id, 7);
  ASSERT_THAT(parsed_update.resources[1].participant.value().participant_id, 9);
}

TEST(ParticipantsResourceHandlerTest, NoParticipantIsOk) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{
      "id": 3
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  ASSERT_FALSE(parsed_update.resources[0].participant.has_value());
}

TEST(ParticipantsResourceHandlerTest, ResourceSnapshotIdIsZeroIfMissing) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": [{}]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.resources, SizeIs(1));
  ASSERT_EQ(parsed_update.resources[0].id, 0);
  ASSERT_FALSE(parsed_update.resources[0].participant.has_value());
}

TEST(ParticipantsResourceHandlerTest, ResourcesUpdateEmptyArrayParsesJson) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": []
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.resources, SizeIs(0));
}

TEST(ParticipantsResourceHandlerTest, ParsesMultipleDeletedResources) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "deletedResources": [
      {
        "id": 3,
        "participant": true
      },
      {
        "id": 5,
        "participant": false
      }
    ]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(2));
  ASSERT_EQ(parsed_update.deleted_resources[0].id, 3);
  ASSERT_TRUE(parsed_update.deleted_resources[0].participant.has_value());
  ASSERT_EQ(parsed_update.deleted_resources[0].participant.value(), true);
  ASSERT_EQ(parsed_update.deleted_resources[1].id, 5);
  ASSERT_TRUE(parsed_update.deleted_resources[1].participant.has_value());
  ASSERT_EQ(parsed_update.deleted_resources[1].participant.value(), false);
}

TEST(ParticipantsResourceHandlerTest,
     DeletedResourcesUpdateEmptyArrayParsesJson) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "deletedResources": []
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(0));
}

TEST(ParticipantsResourceHandlerTest, DeletedResourcesIdIsZeroIfMissing) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "deletedResources": [{}]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(1));
  ASSERT_EQ(parsed_update.deleted_resources[0].id, 0);
}

TEST(MediaEntriesResourceHandlerTest, DeletedResourcesMissingParticipantIsOk) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "deletedResources": [{
      "id": 3
    }]
  })json");

  ASSERT_TRUE(status_or_parsed_update.ok());
  ParticipantsChannelToClient parsed_update =
      std::move(status_or_parsed_update).value();
  ASSERT_THAT(parsed_update.deleted_resources, SizeIs(1));
  ASSERT_EQ(parsed_update.deleted_resources[0].id, 3);
  ASSERT_FALSE(parsed_update.deleted_resources[0].participant.has_value());
}

TEST(ParticipantsResourceHandlerTest, MalformedJsonReturnsErrorStatus) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update =
      handler.ParseUpdate(" random garbage that is not json!");

  EXPECT_EQ(status_or_parsed_update.status().code(),
            absl::StatusCode::kInternal);
  EXPECT_EQ(
      status_or_parsed_update.status().message(),
      "Invalid participants json format:  random garbage that is not json!");
}

TEST(ParticipantsResourceHandlerTest,
     UnexpectedResourceSnapshotsReturnsErrorStatus) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "resources": {
      "id": 3,
        "participant": {
          "participantId": 7,
          "name": "some-participant-name",
          "signedInUser": {
            "user": "some-obfuscated-gaia-id",
            "displayName": "some-display-name"
          }
        }
    }
  })json");

  EXPECT_EQ(status_or_parsed_update.status().code(),
            absl::StatusCode::kInternal);
  EXPECT_TRUE(absl::StrContains(status_or_parsed_update.status().message(),
                                "Invalid participants json format. Expected "
                                "resources field to be an array:"));
}

TEST(ParticipantsResourceHandlerTest,
     UnexpectedDeletedResourcesReturnsErrorStatus) {
  ParticipantsResourceHandler handler;
  auto status_or_parsed_update = handler.ParseUpdate(R"json({
    "deletedResources": {
      "id": 3
    }
  })json");

  EXPECT_EQ(status_or_parsed_update.status().code(),
            absl::StatusCode::kInternal);
  EXPECT_TRUE(absl::StrContains(status_or_parsed_update.status().message(),
                                "Invalid participants json format. Expected "
                                "deletedResources field to be an array:"));
}

}  // namespace
}  // namespace meet
