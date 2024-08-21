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

#include "native/internal/curl_request.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include <curl/curl.h>
#include "native/internal/testing/mock_curl_api_wrapper.h"

namespace meet {
namespace {
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::StrEq;

TEST(CurlRequestTest, NoUrlSetReturnsError) {
  CurlRequestFactory request_factory;
  std::unique_ptr<CurlRequest> request = request_factory.Create();

  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(request_status.message(), HasSubstr("Request URL is empty"));
}

TEST(CurlRequestTest, NoRequestBodySetReturnsError) {
  CurlRequestFactory request_factory;
  std::unique_ptr<CurlRequest> request = request_factory.Create();

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(request_status.message(), HasSubstr("Request body is empty"));
}

TEST(CurlRequestTest, NoHeadersSetReturnsError) {
  CurlRequestFactory request_factory;
  std::unique_ptr<CurlRequest> request = request_factory.Create();

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(request_status.message(), HasSubstr("Request headers are empty"));
}

TEST(CurlRequestTest, CurlNullPtrReturnsError) {
  auto request = std::make_unique<CurlRequest>(nullptr);

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(), HasSubstr("Curl is null"));
}

TEST(CurlRequestTest, FailureToInitEasyCurlReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasyInit()).WillOnce(Return(nullptr));
  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(), HasSubstr("Failed to initialize curl"));
}

TEST(CurlRequestTest, FailureToSetCurlHeaderReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce([](CURL* curl, CURLoption option, void* value) {
        return CURLE_UNKNOWN_OPTION;
      });

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Failed to set curl http header"));
}

TEST(CurlRequestTest, FailureToSetCurlMethodReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt(_, CURLOPT_UPLOAD, 1))
      .WillOnce([](CURL* curl, CURLoption option, int value) {
        EXPECT_EQ(option, CURLOPT_UPLOAD);
        return CURLE_UNKNOWN_OPTION;
      });

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestMethod(CurlRequest::Method::kPut);
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Failed to set curl http method"));
}

TEST(CurlRequestTest, FailureToSetCurlUrlReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt)
      .WillOnce([](CURL* curl, CURLoption option, int value) {
        // Default method should be post.
        EXPECT_EQ(option, CURLOPT_POST);
        return CURLE_OK;
      });

  std::string url = "www.this_is_sparta.com";
  EXPECT_CALL(*mock_curl_api, EasySetOptStr(_, CURLOPT_URL, _))
      .WillOnce(
          [&url](CURL* curl, CURLoption option, const std::string& value) {
            EXPECT_EQ(value, url);
            return CURLE_UNKNOWN_OPTION;
          });

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl(url);
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(), HasSubstr("Failed to set curl url"));
}

TEST(CurlRequestTest, FailureToSetCurlRequestBodyReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr(_, CURLOPT_URL, _))
      .WillOnce(Return(CURLE_OK));

  std::string body = "{\"offer\": \"some random sdp offer\"}";
  EXPECT_CALL(*mock_curl_api, EasySetOptStr(_, CURLOPT_POSTFIELDS, _))
      .WillOnce(
          [&body](CURL* curl, CURLoption option, const std::string& value) {
            EXPECT_EQ(value, body);
            return CURLE_UNKNOWN_OPTION;
          });

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody(body);
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Failed to set curl request body"));
}

TEST(CurlRequestTest, FailureToSetCurlWriteFunctionReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr).WillRepeatedly(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptCallback(_, CURLOPT_WRITEFUNCTION, _))
      .WillOnce(Return((CURLE_UNKNOWN_OPTION)));

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Failed to set curl write function"));
}

TEST(CurlRequestTest, FailureToSetCurlWriteDataReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr).WillRepeatedly(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptCallback(_, CURLOPT_WRITEFUNCTION, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce(Return((CURLE_UNKNOWN_OPTION)));

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Failed to set curl write data"));
}

TEST(CurlRequestTest, FailureToPerformCurlRequestReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr).WillRepeatedly(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptCallback(_, CURLOPT_WRITEFUNCTION, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasyPerform)
      .WillOnce(Return(CURLE_UNKNOWN_OPTION));

  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");
  auto request_status = request->Send();

  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("An unknown option was passed in to libcurl"));
}

TEST(CurlRequestTest, ResponseDataStoresResponse) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce([](CURL* curl, CURLoption option, void* value) {
        auto headers_list = static_cast<struct curl_slist*>(value);
        EXPECT_THAT(headers_list->data,
                    StrEq("Authorization: Bearer iliketurtles"));
        return CURLE_OK;
      });
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr).WillRepeatedly(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptCallback(_, CURLOPT_WRITEFUNCTION, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasyPerform).WillOnce(Return(CURLE_OK));

  absl::Cord response("the answer to life is 42");
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response](CURL* curl, CURLoption option, void* value) {
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = response;
        return CURLE_OK;
      });
  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");

  EXPECT_TRUE(request->Send().ok());
  EXPECT_EQ(request->GetResponseData(), response);
}

TEST(CurlRequestTest, ReusedRequestReturnsError) {
  auto mock_curl_api = std::make_unique<MockCurlApiWrapper>();
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_HTTPHEADER, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptInt).WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptStr).WillRepeatedly(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasySetOptCallback(_, CURLOPT_WRITEFUNCTION, _))
      .WillOnce(Return(CURLE_OK));
  EXPECT_CALL(*mock_curl_api, EasyPerform).WillOnce(Return(CURLE_OK));

  absl::Cord response("the answer to life is 42");
  EXPECT_CALL(*mock_curl_api, EasySetOptPtr(_, CURLOPT_WRITEDATA, _))
      .WillOnce([&response](CURL* curl, CURLoption option, void* value) {
        absl::Cord* str_response = reinterpret_cast<absl::Cord*>(value);
        *str_response = response;
        return CURLE_OK;
      });
  auto request = std::make_unique<CurlRequest>(std::move(mock_curl_api));

  request->SetRequestUrl("www.this_is_sparta.com");
  request->SetRequestHeader("Authorization", "Bearer iliketurtles");
  request->SetRequestHeader("Content-Type", "application/json");
  request->SetRequestBody("{\"offer\": \"some random sdp offer\"}");

  EXPECT_TRUE(request->Send().ok());
  auto request_status = request->Send();
  EXPECT_EQ(request_status.code(), absl::StatusCode::kInternal);
  EXPECT_THAT(request_status.message(),
              HasSubstr("Request object has already been used for "
                        "another curl request"));
}

}  // namespace
}  // namespace meet
