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

#include "native/internal/meet_connect_response.h"

#include <string>
#include <tuple>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

namespace meet {
namespace {
using ::testing::HasSubstr;
using ::testing::StrEq;
using ::testing::Values;
using enum ::absl::StatusCode;

TEST(MeetConnectResponseTest, ParsesAnswerField) {
  std::string answer = "What is this, a center for ants?";
  auto status_or_response = MeetConnectResponse::FromRequestResponse(
      absl::StrFormat(R"json({"answer": "%s"})json", answer));
  ASSERT_TRUE(status_or_response.ok());
  MeetConnectResponse response = status_or_response.value();
  EXPECT_THAT(response.answer, StrEq(answer));
}

class MeetConnectResponseErrorTest
    : public ::testing::TestWithParam<
          std::tuple<absl::StatusCode, std::string, std::string>> {};

TEST_P(MeetConnectResponseErrorTest, ParsesErrorField) {
  const auto& [absl_code, status_code, status_message] = GetParam();

  auto status_or_response =
      MeetConnectResponse::FromRequestResponse(absl::StrFormat(
          R"json({
                              "error": {
                                "code": 999,
                                "message": "%s",
                                "status": "%s"
                              }
                            })json",
          status_message, status_code));
  ASSERT_TRUE(status_or_response.ok());
  MeetConnectResponse response = status_or_response.value();
  EXPECT_EQ(response.status.code(), absl_code);
  EXPECT_THAT(response.status.message(), StrEq(status_message));
}

INSTANTIATE_TEST_SUITE_P(
    MeetConnectResponseErrorTest, MeetConnectResponseErrorTest,
    Values(std::make_tuple(kInvalidArgument,
                           absl::StatusCodeToString(kInvalidArgument),
                           "If you can dodge a wrench, you can dodge a ball."),
           std::make_tuple(
               kFailedPrecondition,
               absl::StatusCodeToString(kFailedPrecondition),
               "I don't know how to put this, but I'm kind of a big deal."),
           std::make_tuple(kPermissionDenied,
                           absl::StatusCodeToString(kPermissionDenied),
                           "If you ain't first, you're last."),
           std::make_tuple(kDeadlineExceeded,
                           absl::StatusCodeToString(kDeadlineExceeded),
                           "You sit on a throne of lies."),
           std::make_tuple(kAborted, absl::StatusCodeToString(kAborted),
                           "You're killing me smalls."),
           std::make_tuple(kUnauthenticated,
                           absl::StatusCodeToString(kUnauthenticated),
                           "There's no crying in baseball.")));

TEST(MeetConnectResponseTest, MalformedJsonReturnsErrorStatus) {
  auto status_or_response = MeetConnectResponse::FromRequestResponse(
      "random garbage that is not json!");
  EXPECT_EQ(status_or_response.status().code(), absl::StatusCode::kInternal);
  EXPECT_THAT(status_or_response.status().message(),
              HasSubstr("Unexpected or malformed response from Meet servers."));
}

}  // namespace
}  // namespace meet
