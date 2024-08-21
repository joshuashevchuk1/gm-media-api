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

#ifndef NATIVE_INTERNAL_TESTING_MOCK_CURL_REQUEST_FACTORY_H_
#define NATIVE_INTERNAL_TESTING_MOCK_CURL_REQUEST_FACTORY_H_

#include <memory>

#include "gmock/gmock.h"
#include "native/internal/curl_request.h"

namespace meet {

class MockCurlRequestFactory : public CurlRequestFactory {
 public:
  static std::unique_ptr<MockCurlRequestFactory> CreateUnique() {
    return std::make_unique<MockCurlRequestFactory>();
  }
  ~MockCurlRequestFactory() override = default;
  MOCK_METHOD(std::unique_ptr<CurlRequest>, Create, (), (override));
};

}  // namespace meet

#endif  // NATIVE_INTERNAL_TESTING_MOCK_CURL_REQUEST_FACTORY_H_
